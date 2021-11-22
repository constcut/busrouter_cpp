#include "json.h"
#include "svg.h"
#include "parser.h"
#include "routefinder.h"
#include "maprender.h"


#include <iostream>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <memory>


#include "transport_catalog.pb.h"

#include "memchecker.h"

using namespace std;


class RequestsManager { //TODO вынести в отдельны хэдер

public:
    RequestsManager() {} 

    string ReadFileData(const string& file_name) {
        ifstream file(file_name, ios::binary | ios::ate);
        const ifstream::pos_type end_pos = file.tellg();
        file.seekg(0, ios::beg);

        string data(end_pos, '\0');
        file.read(&data[0], end_pos);
        return data;
    }

    void MakeBase(const Json::Document& document) { 

        cerr << "RAM before MakeBase "<< getRAM() << endl;
        const auto& mainNode = document.GetRoot().AsMap();
        parser.parseMakeRequests(mainNode);
        Database::TransportCatalog db;
        parser.serialize(db);
        parser.serializeYellowPages(db, mainNode.at("yellow_pages")); 
        parser.interpolateGeoCoordinates();
        
        routeFinder.createAndSerialize(parser, mainNode.at("routing_settings"), db); 

        mapRender = make_unique<MapRender>(parser, mainNode.at("render_settings"));
        mapRender->serialize(db);

        ofstream os(getSerializeFilename(mainNode));
        os << db.SerializeAsString();
        cerr << "RAM after MakeBase "<< getRAM() << endl;
    }


    void ProcessRequests(const Json::Document& document) { 

        const auto& mainNode = document.GetRoot().AsMap();
        parser.parseProcessRequests(mainNode);

        Database::TransportCatalog db;
        db.ParseFromString(ReadFileData(getSerializeFilename(mainNode))); //db.ParseFromIstream(&is);
        
        parser.deserialize(db);
        routeFinder.deserialize(parser, db);

        mapRender = make_unique<MapRender>(db, parser);
        prepareInvertedIndecies();
    }


    void run(ostream& os) const { 
        os << setprecision(10);
        vector<Json::Node> jsonArray;
        processTransportRequests(jsonArray);
        processYellowPagesRequests(jsonArray);
        processRouteToCompanyRequests(jsonArray);
        os << jsonArray;
    }


    bool debug = true;

 private:

    Parser parser;
    RouteFinder routeFinder;
    unique_ptr <MapRender> mapRender;

    using CompanyPtr = const YellowPages::Company*;
    using SearchResults = unordered_set<CompanyPtr>;
    using InvIdxMap = unordered_map<string, SearchResults>;

    InvIdxMap invIdxNames; 
    InvIdxMap invIdxRubrics;
    InvIdxMap invIdxUrls;
    InvIdxMap invIdxPhones;


    void addInvertedIndex(const string& value, CompanyPtr ptr, InvIdxMap& indecies) {
        if (indecies.count(value)) 
            indecies[value].insert(ptr);
        else 
            indecies[value] = {ptr};
    }


    void prepareInvertedIndecies() { 
        const auto& yellowPages = parser.getYellowPages();
        const auto& rubrics = parser.getRubrics();
        for (const auto& c: yellowPages.companies()) { 
            for (const auto& n: c.names())
                addInvertedIndex(n.value(), &c, invIdxNames);
            for (const auto& r: c.rubrics()) 
                addInvertedIndex(rubrics.at(r), &c, invIdxRubrics);
            for (const auto& u: c.urls())
                addInvertedIndex(u.value(), &c, invIdxUrls);
            for (const auto& p: c.phones())
                addInvertedIndex(p.number(), &c, invIdxPhones);
        }
    }


    bool phoneCorrect(const YellowPages::Phone& phone, const Phone& request) const {
        if (request.type != -1 && request.type != phone.type()) 
            return false;
        if ((!request.countryCode.empty() || !request.localCode.empty()) && request.localCode != phone.local_code())
            return false;
        if (!request.extension.empty() && request.extension != phone.extension())
            return false;
        if (!request.countryCode.empty() && request.countryCode != phone.country_code())
            return false;
        return true;
    }


    void filterByPhoneRequest(SearchResults& found, const vector<Phone>& phones) const {
        if (phones.empty() || found.empty())
            return;

        SearchResults toDelete;
        for (const auto& comp: found) {
               bool companyIsFine = false;
                for (size_t i = 0; i < comp->phones_size(); ++i) {
                   const auto& p = comp->phones()[i];
                   const auto& num = p.number();
                    for (const auto& pRec: phones)  
                        if (pRec.number == num)
                            if (phoneCorrect(p, pRec)) {
                                companyIsFine = true;
                                break;
                            }
                    if (companyIsFine)
                        break;
                }
                if (companyIsFine == false)
                    toDelete.insert(comp);
            }
        for (const auto& d: toDelete)
            found.erase(d);
    }


    auto searchPhoneInIdxNoClean(const vector<Phone>& phones) const {
        SearchResults foundResults;
        for (const auto& phoneRecord: phones) {
            if (invIdxPhones.count(phoneRecord.number)) {
                const auto& found = invIdxPhones.at(phoneRecord.number);    
                foundResults.insert(found.begin(), found.end());
            }
        }
        return foundResults;
    }


    auto searchInIdx(const vector<string>& values, const InvIdxMap& indecies) const {
        SearchResults foundResults;
        for (const auto& val: values) 
            if (indecies.count(val)) {
                const auto& found = indecies.at(val);
                foundResults.insert(found.begin(), found.end());
            }
        return foundResults;
    }


    void processYellowPagesRequests(vector<Json::Node>& jsonArray) const  {
        const auto& requests = parser.getCompanyRequests();
        map<string, Json::Node> outputRecord;
        vector<Json::Node> companiesList;
        
        for (const auto& r : requests) {
            outputRecord["request_id"] = r.requestId;
            processSingleYellowRequest(r, companiesList);
            outputRecord["companies"] = move(companiesList);
            jsonArray.push_back(move(outputRecord));
        }
    }


    void processSingleYellowRequest(const FindCompanyRequest& r, vector<Json::Node>& companiesList) const {

        vector<bool> searchFlags;
        vector<SearchResults*> foundCategories; 
        SearchResults buffers[2]; 

        auto namesResults = searchInIdx(r.names, invIdxNames); 
        auto rubricResults = searchInIdx(r.rubrics, invIdxRubrics);
        auto urlsResults = searchInIdx(r.urls, invIdxUrls); 

        SearchResults* searchResults;
        if (r.names.size() + r.rubrics.size() + r.urls.size()) {
            searchFlags = {!r.names.empty(), !r.rubrics.empty(), !r.urls.empty()}; 
            foundCategories = {&namesResults, &rubricResults, &urlsResults}; 
            searchResults = mergeSearchResults(searchFlags, foundCategories, buffers);
        }
        else {
            buffers[0] = searchPhoneInIdxNoClean(r.phones); 
            searchResults = buffers;
        }

        if (searchResults) 
            filterByPhoneRequest(*searchResults, r.phones);     
        if (searchResults)
            for (const auto ptr: *searchResults)
                for (const auto& name: ptr->names()) 
                    if (name.type() == 0) {
                        companiesList.push_back(name.value());
                        break;
                    }  
    }


    void processRouteToCompanyRequests(vector<Json::Node>& jsonArray) const  {
        const auto& requests = parser.getRouteToCompanyRequest();
        map<string, Json::Node> outputRecord; 

        for (const auto& r: requests) {
            vector<Json::Node> companiesList;
            processSingleYellowRequest(r, companiesList);
            outputRecord["request_id"] = r.requestId;
            const auto& from = r.from;
            if (companiesList.empty()) {
                outputRecord["error_message"] = "not found"s;
                jsonArray.push_back(move(outputRecord));
                continue;
            }

            RouteAction bestRoute {true, numeric_limits<double>::max()};
            double waitTime = 0;
            const auto& workInt = parser.getWorkIntervals();
            
            for (const auto& company: companiesList) { //Если не нашлось - маршрут не найден
                RouteAction route = routeFinder.findRoute(from, company.AsString(), parser);
                if (route.notFound)
                    continue;
                const auto& companyIntervals = workInt.at(company.AsString());

                double now = r.currentTime + route.totalTime;

                if (route.notFound == false && 
                    (bestRoute.totalTime + waitTime) > route.totalTime) {

                    double waitInterval = findWaitInterval(companyIntervals, now);
                    double totalTime = waitInterval + route.totalTime;

                    if ((bestRoute.totalTime + waitTime) > totalTime) {
                        bestRoute = route;
                        waitTime = waitInterval;
                    }
                }
            }

            if (bestRoute.notFound) {
                outputRecord["error_message"] = "not found"s;
            }
            else {
                outputRecord["total_time"] = bestRoute.totalTime + waitTime; 
                vector<Json::Node> actions;
                fillRouteActions(actions, bestRoute);

                if (waitTime != 0) {
                    map<string, Json::Node> action;
                    action["type"] = "WaitCompany"s;
                    action["time"] = waitTime;
                    action["company"] = bestRoute.finalStop;
                    actions.push_back(action);
                }
                
                outputRecord["items"] = move(actions);

                ostringstream oss; 
                oss << setprecision(10);
                mapRender->buildCompanyRoute(bestRoute, oss);
                if (debug) {
                    static int routeCount = 0;
                    ++routeCount;
                    ofstream routeOutput("../inOut/lastCRoute" + to_string(routeCount) + ".svg");
                    routeOutput << oss.str();
                }
                outputRecord["map"] = oss.str();
            }
            jsonArray.push_back(move(outputRecord));
        }
    }

    double findWaitInterval(const vector<pair<uint32_t, uint32_t>>& intervals, double now) const {
        for (const auto& interval: intervals) 
            if (now >= interval.first && now < interval.second)  //Не просто больше
                return 0;
            else if (now < interval.first) {
                return interval.first - now;
            }
        double tillTheEnd = 24 * 60 * 7 - now;
        if (tillTheEnd < 0) {
            now = -tillTheEnd;
            tillTheEnd = 0;
        }
        else
            now = 0;
        double foundTime = 0;
        for (const auto& interval: intervals) 
            if (now >= interval.first && now < interval.second) { //<= во второй части тоже
                foundTime = 0;
                break;
            }
            else if (now < interval.first) {
                foundTime = interval.first - now;
                break;
            }
        return foundTime + tillTheEnd;
    }


    void fillRouteActions(vector<Json::Node>& actions, const RouteAction& route) const {
        for (const auto& a : route.actions) {
            map<string, Json::Node> action;
            action["type"] = a.type;

            if (isDoubleInteger(a.time)) {
                int intTime = a.time;
                action["time"] = intTime;
            }
            else
                action["time"] = a.time;

            if (a.type == "WaitBus")
                action["stop_name"] = a.name;
            if (a.type == "RideBus") {
                action["bus"] = a.name;
                action["span_count"] = int(a.spans); //Грязный хук :)
            }
            if (a.type == "WalkToCompany") {
                action["stop_name"] = a.name;
                action["company"] = a.companyName;
            }
            actions.push_back(move(action));
        }
    }


    SearchResults intersection(const SearchResults& set1, const SearchResults& set2) const {
        SearchResults newSet;    
        if (set1.size() > set2.size()) {
            for (const auto element: set2)
                if (set1.count(element)) 
                    newSet.insert(element);
        }  
        else 
            for (const auto element: set1)
                if (set2.count(element))
                    newSet.insert(element);

        return move(newSet);
    }


    SearchResults* mergeSearchResults(const vector<bool>& searchFlags, 
        const vector<SearchResults*>& foundCategories, SearchResults* buffers) const
    {           
        SearchResults* first = nullptr, * second = nullptr;
        size_t intersectionCount = 0;
        for (size_t i = 0; i < foundCategories.size(); ++i) 
            if (searchFlags[i]) {
                if (first == nullptr)
                    first = foundCategories[i];
                else {
                    second = foundCategories[i];
                    SearchResults& currentBuf = buffers[intersectionCount % 2];
                    currentBuf = move(intersection(*first, *second));
                    second = nullptr;
                    if (currentBuf.empty()) {
                        first = nullptr;
                        break;
                    }
                    first->clear(); //Мы не знаем, тут найденные значения, или буфер, обнулим тк буфер может понадобится ещё
                    first = &currentBuf;
                    ++intersectionCount;
                }
            }
        return first;
    }


    void processTransportRequests(vector<Json::Node>& jsonArray) const {
        const auto& requests = parser.getRequests();
        for (const auto& r : requests) {
            map<string, Json::Node> outputRecord;
            outputRecord["request_id"] = r.requestId;
            if (r.type == RequestType::Map)
                processMapRequest(outputRecord);
            if (r.type == RequestType::Bus)
                processBusRequest(r.name, outputRecord);
            if (r.type == RequestType::Stop)
                processStopRequest(r.name, outputRecord);
            if (r.type == RequestType::Route)
                processRouteRequest(r.name, r.name2, outputRecord);
            jsonArray.push_back(move(outputRecord));
        }
    }


    void processMapRequest(map<string, Json::Node>& outputRecord) const {
        ostringstream oss;
        oss << setprecision(10); 
        mapRender->getMap().Render(oss);
        if (debug) {
            ofstream mapOutput("../inOut/lastMap.svg");
            mapOutput << oss.str();
        }
        outputRecord["map"] = oss.str();
    }


    void processBusRequest(const string& busName, map<string, Json::Node>& outputRecord) const {
        const auto& busStats = parser.getBusStats();

        if (busStats.count(busName)) {
            outputRecord["stop_count"] = busStats.at(busName).totalStops;
            outputRecord["unique_stop_count"] = busStats.at(busName).uniqueStops;

            double routeLendth = busStats.at(busName).routeLengthNew;
            if (isDoubleInteger(routeLendth)) {
                int intLen = routeLendth;
                outputRecord["route_length"] = intLen;
            }
            else
                outputRecord["route_length"] = routeLendth;

            outputRecord["curvature"] = busStats.at(busName).routeCoef;
        }
        else
            outputRecord["error_message"] = "not found"s;
    }


    void processStopRequest(const string& stopName, map<string, Json::Node>& outputRecord) const {
        const auto& stopStats = parser.getStopStats();

        if (stopStats.count(stopName)) {
            vector<Json::Node> buses;
            if (stopStats.at(stopName).buses.empty() == false)
                for (const auto& busName : stopStats.at(stopName).buses)
                    buses.push_back(busName);
            outputRecord["buses"] = move(buses);
        }
        else
            outputRecord["error_message"] = "not found"s;
    }


    void processRouteRequest(const string& from, const string& to, map<string, Json::Node>& outputRecord) const {
        
        RouteAction route = routeFinder.findRoute(from, to, parser);
        if (route.notFound) {
            outputRecord["error_message"] = "not found"s;
        }
        else {
            outputRecord["total_time"] = route.totalTime;
            vector<Json::Node> actions;
            fillRouteActions(actions, route);
            outputRecord["items"] = move(actions);
            ostringstream oss;
            oss << setprecision(10);
            mapRender->buildRoute(route, oss);
            if (debug) {
                static int routeCount = 0;
                ++routeCount;
                ofstream routeOutput("../inOut/lastRoute" + to_string(routeCount) + ".svg");
                routeOutput << oss.str();
            }
            outputRecord["map"] = oss.str();
        }
    }


    string shieldString(const string& str) const { //Возможно переместить в json с доп поиском
        ostringstream ossShield; 
        for (char c : str) {
            if (c == '\\' || c == '\"')
                ossShield << '\\';
            ossShield << c;
        }
        return ossShield.str();
    }


    string getSerializeFilename(const map<string, Json::Node>& document) const {
        const auto& serializationSettings = document.at("serialization_settings").AsMap();
        const auto& filename = serializationSettings.at("file").AsString();
        return filename;
    }


    double isDoubleInteger(double value) const {
        return value == round(value);
    }
};


void runMakeBase(int testNumber) {
    RequestsManager manager; 
    ifstream input("../inOut/in" + to_string(testNumber) + ".json"); 
    auto json = Json::Load(input); 
    manager.MakeBase(json); 
}


void runTest(int testNumber) {
    runMakeBase(testNumber);
    cout << "Make base finished" << endl;
    {   
        RequestsManager manager;
        ifstream input("../inOut/in" + to_string(testNumber) + "proc.json"); 
        auto json = Json::Load(input);
        manager.ProcessRequests(json);  
        //manager.debug = true; 
        ofstream output("../inOut/out_" + to_string(testNumber) + "_.json");
        manager.run(output);
        cout << "Output written to json file" << endl;
    }
}




//TODO проверить приватность всех классов, а так же константность всех методов - финальный рефакторинг
int main(int argc, const char* argv[]) {

    if (argc != 2) {
        runTest(1);
        runTest(2);
        runMakeBase(3);
    }

    if (argc != 2) {
        cerr << "Usage: transport_catalog_part_o [make_base|process_requests]\n";
        return 5;
    }

    const string_view mode(argv[1]);
    
    //*
    RequestsManager manager;
    if (mode == "make_base") {
        const auto& json = Json::Load(cin); 
        manager.MakeBase(json); 

    } else if (mode == "process_requests") {
        const auto& json = Json::Load(cin); 
        manager.ProcessRequests(json); 
        manager.run(cout);   
    }//*/

  return 0;
}
