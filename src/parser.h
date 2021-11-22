#pragma once

#include "json.h"
#include <string>
#include <unordered_map>
#include <set>
#include <unordered_set>
#include <cmath>
#include <fstream>

#include "transport_catalog.pb.h"
#include "database.pb.h"
#include <google/protobuf/util/json_util.h>

#include <sstream>


enum class RequestType {
    Bus,
    Stop,
    Route,
    Map
};


struct Request { //? union\variant
    RequestType type;
    int  requestId;
    std::string name;
    std::string name2; //На коленке адаптация для route
};


struct Phone {
    int type = -1;
    std::string countryCode;
    std::string localCode;
    std::string number;
    std::string extension;
};

struct FindCompanyRequest {
    int requestId;
    std::vector<std::string> names;
    std::vector<std::string> urls;
    std::vector<std::string> rubrics;
    std::vector<Phone> phones;
};

struct RouteToCompanyRequest : FindCompanyRequest {
    std::string from;
    uint32_t currentTime;
};


struct Coordinates {
    double lat; //latitude
    double lon; //longitude
};


using DistanceMap = std::unordered_map<std::string, double>; //uint32_t


struct StopParams {
    Coordinates coords;
    DistanceMap distance;
};


struct BusRoute {
    std::vector<std::string> stops;
    bool isCyclic;
    std::vector<std::string> endPoints;
};


struct BusStats {
    double routeLength;
    double routeLengthNew;
    double routeCoef;

    int totalStops;
    int uniqueStops;
};


struct StopStats {
    std::set<std::string> buses;
    std::set<std::string> neighbors;
};

struct Company {
    std::vector<std::string> names;
    std::vector<std::string> urls;
    std::vector<uint64_t> rubrics;
};


class Parser {
public:

    void parseMakeRequests(const std::map<std::string, Json::Node>& document) {
        readInputRequestsJson(document.at("base_requests")); 
        build();
    }

    void parseProcessRequests(const std::map<std::string, Json::Node>& document) {
        readOutputRequestsJson(document.at("stat_requests"));
    }

    void serializeYellowPages(Database::TransportCatalog& db, const Json::Node& node) {
        std::stringstream ss;  
        ss << node;
        auto yp = db.mutable_yellow_pages();
        google::protobuf::util::JsonStringToMessage(ss.str(), yp);
        const auto& yellowPages = db.yellow_pages(); //Можно не копировать здесь, создать ссылку

        for (const auto& p : yellowPages.rubrics()) 
            rubrics[p.first] = p.second.name();

        for (size_t i = 0; i < yellowPages.companies_size(); ++i) {
            const auto& company = yellowPages.companies()[i];
            std::string companyName;
            for (const auto& name: company.names()) 
                if (name.type() == 0){
                    companyName = name.value();
                    break;
                }
            companyIdx[companyName] = i;
            companyNames.push_back(companyName);

            std::string fullName = companyName;
            if (company.rubrics_size())
                fullName = rubrics[company.rubrics()[0]] + " " + companyName;

            companyFullNames.push_back(fullName);
            
            companyNeighbors[fullName] = {};
            for (const auto& n: company.nearby_stops()) {
                const auto& stopName = n.name();
                companyNeighbors[fullName].insert(stopName);
                stopStats[stopName].neighbors.insert(fullName);
            }
        }
    }


    void serialize(Database::TransportCatalog& db) {
        for (const auto& name: stopsNames) {
            const auto& stat = stopStats.at(name);
            db.add_stop_names(name);
            auto stopSerial = db.add_stop_stats();
            for (const auto& busName: stat.buses)
                stopSerial->add_bus_names(busName);
        }

        for (const auto& [name, busInfo]: routes) {
            db.add_bus_names(name);
            auto busSerial = db.add_bus_stats();
            const auto& stat = busStats[name];
            busSerial->set_stop_count(stat.totalStops);
            busSerial->set_unique_stop_count(stat.uniqueStops);
            busSerial->set_route_length(stat.routeLengthNew);
            busSerial->set_curvative(stat.routeCoef);

            auto busInfoSer = db.add_bus_info();
            busInfoSer->set_is_cyclic(busInfo.isCyclic);
            for (const auto& stopName: busInfo.stops) {
                size_t idx = stopsIdx[stopName];
                busInfoSer->add_stops(idx);
            }
            for (const auto& stopName: busInfo.endPoints) {
                size_t idx = stopsIdx[stopName];
                busInfoSer->add_end_points(idx);
            }
        }
    }


    void deserialize(Database::TransportCatalog& db) {

        for (size_t i = 0; i < db.stop_names_size(); ++i) {
            const auto& name = db.stop_names()[i];
            const auto& buses = db.stop_stats()[i].bus_names();
            StopStats stat;
            for (const auto& busName: buses)
                stat.buses.insert(busName); //Чисто в теории тут можно разворачивать часть busRoute
            stopStats[name] = std::move(stat);

            size_t idx = stopsNames.size();
            stopsNames.push_back(name);
            stopsIdx[name] = idx;
        }
        for (size_t i = 0; i < db.bus_names_size(); ++i) {
            const auto& name = db.bus_names()[i];
            const auto& bStats = db.bus_stats()[i];
            BusStats stat;
            stat.totalStops = bStats.stop_count();
            stat.uniqueStops = bStats.unique_stop_count();
            stat.routeLengthNew = bStats.route_length();
            stat.routeCoef = bStats.curvative();
            busStats[name] = std::move(stat);

            const auto& bInfo = db.bus_info()[i];
            BusRoute route;
            route.isCyclic = bInfo.is_cyclic();
            for (auto stopIdx : bInfo.stops()) {
                const auto& stopName = stopsNames[stopIdx];
                route.stops.push_back(stopName);
            }
            for (auto stopIdx : bInfo.end_points()) {
                const auto& stopName = stopsNames[stopIdx];
                route.endPoints.push_back(stopName);
            }
            routes[name] = std::move(route);
        }
        yellowPages = db.yellow_pages();
        for (const auto& p : yellowPages.rubrics()) 
            rubrics[p.first] = p.second.name();

        //Для построения маршрутов до компаний
        for (size_t i = 0; i < yellowPages.companies_size(); ++i) {
            const auto& company = yellowPages.companies()[i];
            std::string companyName;
            for (const auto& name: company.names()) 
                if (name.type() == 0){
                    companyName = name.value();
                    break;
                }
            std::string fullName = companyName;
            if (company.rubrics_size())
                fullName = rubrics[company.rubrics()[0]] + " " + companyName;
            companyFullNames.push_back(move(fullName));
            companyIdx[companyName] = i; //Move company name

            //TODO перенести потом на этап создания базы
            
            workInt[companyName] = {};

            const auto& workIntervals = company.working_time().intervals();
            for (const auto& interval: workIntervals) {
                uint32_t day = interval.day();
                uint32_t start = interval.minutes_from();
                uint32_t finish = interval.minutes_to();
                if (day > 0) {
                    start += (day - 1) * 24 * 60;
                    finish += (day - 1) * 24 * 60;
                    workInt[companyName].push_back({start, finish});
                }
                else 
                    for (uint32_t d = 0; d < 7; ++d) {
                        uint32_t localStart = start + d * 24 * 60;
                        uint32_t localFinish = finish + d * 24 * 60;
                        workInt[companyName].push_back({localStart, localFinish});
                    }
            }

            if (workIntervals.empty()) {
                workInt[companyName].push_back({0, 7*24*60});
            }
            sort(workInt[companyName].begin(), workInt[companyName].end());
        }
    }


    void readOutputRequestsJson(const Json::Node& node) {
        const auto& allRequests = node.AsArray();
        for (const auto& requestJson : allRequests) {
            const auto& request = requestJson.AsMap();
            const auto& type = request.at("type").AsString();
            int id = request.at("id").AsInt();

            if (type == "Map") {
                requests.push_back({ RequestType::Map, id });
            }
            else if (type == "Route") {
                std::string from = request.at("from").AsString();
                std::string to = request.at("to").AsString();
                requests.push_back({ RequestType::Route,  id, move(from), move(to) });
            }
            else if (type == "FindCompanies") {
                FindCompanyRequest r {id};
                parseCompanyRequest(r, request);
                companyRequests.push_back(std::move(r));
            }
            else if (type == "RouteToCompany") {
                RouteToCompanyRequest r{id};
                r.from = request.at("from").AsString();  
                const auto& compReq = request.at("companies").AsMap();
                parseCompanyRequest(r, compReq);

                const auto& timeArr = request.at("datetime").AsArray();
                r.currentTime = timeArr[2].AsInt() + timeArr[1].AsInt() * 60 
                    + timeArr[0].AsInt() * 60 * 24;

                routeToCompanyRequests.push_back(std::move(r));
            }
            else {
                std::string name = request.at("name").AsString();
                if (type == "Bus")
                    requests.push_back({ RequestType::Bus,  id,  std::move(name) });
                if (type == "Stop")
                    requests.push_back({ RequestType::Stop, id, std::move(name) });
            }
        }
    }

    void parseCompanyRequest(FindCompanyRequest& r, const std::map<std::string, Json::Node>& request) {
        if (request.count("names")) {
            const auto& namesArr = request.at("names").AsArray();
            for (const auto& name: namesArr) 
                r.names.push_back(name.AsString());
        }
        if (request.count("urls")) {
            const auto& urlsArr = request.at("urls").AsArray();
            for (const auto& url: urlsArr) 
                r.urls.push_back(url.AsString());
        }
        if (request.count("rubrics")) {
            const auto& rubricsArr = request.at("rubrics").AsArray();
            for (const auto& rubric: rubricsArr) 
                r.rubrics.push_back(rubric.AsString());
        }
        if (request.count("phones")) {
            const auto& phonesArr = request.at("phones").AsArray();
            for (const auto& phone: phonesArr) {
                const auto& phoneMap = phone.AsMap();
                Phone p;
                if (phoneMap.count("type")) {
                    const auto& t = phoneMap.at("type").AsString();
                    if (t == "FAX")
                        p.type = 1;
                    else
                        p.type = 0;
                }
                if (phoneMap.count("country_code"))
                    p.countryCode = phoneMap.at("country_code").AsString();
                if (phoneMap.count("local_code"))
                    p.localCode = phoneMap.at("local_code").AsString();
                if (phoneMap.count("number"))
                    p.number = phoneMap.at("number").AsString();
                if (phoneMap.count("extension"))
                    p.extension = phoneMap.at("extension").AsString();
                r.phones.push_back(std::move(p));
            }
        }
    }

    void readInputRequestsJson(const Json::Node& node) {
        const auto& allRequests = node.AsArray();
        for (const auto& r : allRequests) {
            const auto& request = r.AsMap();
            const auto& type = request.at("type").AsString();

            if (type == "Stop")
                readStopJson(request);
            if (type == "Bus")
                readBusJson(request);
        }
    }

    void readStopJson(const std::map<std::string, Json::Node>& request) {
        const auto& name = request.at("name").AsString();
        double lon = request.at("longitude").AsDouble();
        double lat = request.at("latitude").AsDouble();

        std::map<std::string, Json::Node> distances;
        if (request.count("road_distances"))
            distances = request.at("road_distances").AsMap();

        stopStats[name] = {}; //Для того чтобы учитывать остановки без астобыусов
        size_t idx = stopsNames.size();
        stopsNames.push_back(name);
        stopsIdx[name] = idx;  //Для тщетной попытки построения графа

        if (distances.empty())
            stops[name] = { { lat, lon } };
        else {
            auto stopsDist = parseStopsDistanceJson(distances);
            stops[name] = { { lat, lon },  stopsDist };
        }
        //Дистанции возможно простроить только если 
    }


    DistanceMap parseStopsDistanceJson(std::map<std::string, Json::Node>& distances) {
        DistanceMap distMap;
        for (const auto& d : distances)
            distMap[d.first] = d.second.AsDouble(); // Тут лежит int но мы его преобразуем к double при добавлении
        return distMap;
    }


    void readBusJson(const std::map<std::string, Json::Node>& request) {
        std::string name = request.at("name").AsString();
        BusRoute route;
        route.isCyclic = request.at("is_roundtrip").AsBool();
        const auto& stops = request.at("stops").AsArray();
        for (const auto& s : stops)
            route.stops.push_back(s.AsString());
        if (route.stops.empty() == false)  
            route.endPoints.push_back(route.stops[0]);
        if (route.isCyclic == false && route.stops.size() >= 2) {
            const auto& secondEndpoint = route.stops[route.stops.size() - 1];
            if (secondEndpoint != route.endPoints[0])
                route.endPoints.push_back(secondEndpoint);
        }
        routes[name] = route;
    }


    void build() {
        for (const auto& r : routes) {
            BusStats stats;
            const auto& busName = r.first;
            stats.routeLength = calculateLength(r.second.stops, r.second.isCyclic);
            stats.routeLengthNew = calculateLengthNew(r.second.stops, r.second.isCyclic);
            stats.routeCoef = stats.routeLengthNew / stats.routeLength;
            stats.totalStops = r.second.isCyclic ? r.second.stops.size() : r.second.stops.size() * 2 - 1;
            std::unordered_set<std::string> uniqueStops(r.second.stops.begin(), r.second.stops.end());
            stats.uniqueStops = uniqueStops.size();
            for (const auto& s : uniqueStops)
                stopStats[s].buses.insert(busName); //Сеты созданны гарантированно
            busStats[busName] = stats;

            if (r.second.stops.size() > 1) //Маркировка соседний остановок
                for (size_t i = 0; i < r.second.stops.size(); ++i) {
                    if (i != 0) {
                        const auto& currentStop = r.second.stops[i];
                        const auto& prevStop = r.second.stops[i - 1];
                        stopStats[currentStop].neighbors.insert(prevStop);
                    }
                    if (i != r.second.stops.size() - 1) {
                        const auto& currentStop = r.second.stops[i];
                        const auto& nextStop = r.second.stops[i + 1];
                        stopStats[currentStop].neighbors.insert(nextStop);
                    }
                }
        }
    }



    void interpolateGeoCoordinates() { 
        const auto& supportStops = findSupportStops();
     
        for (const auto& [_, route] : getRoutes()) {
            size_t lastSupportIdx = 0;
            for (size_t i = 1; i < route.stops.size(); ++i) {
                if (supportStops.count(route.stops[i])) {
                    const auto& startStop = route.stops[lastSupportIdx];
                    const auto& endStop = route.stops[i];
                    double lonStep = (stops.at(endStop).coords.lon - stops.at(startStop).coords.lon) / (i - lastSupportIdx);
                    double latStep = (stops.at(endStop).coords.lat - stops.at(startStop).coords.lat) / (i - lastSupportIdx);
                    for (size_t k = lastSupportIdx; k <= i; ++k) {
                        const auto& stopName = route.stops[k];
                        stops[stopName].coords.lat = stops[startStop].coords.lat + latStep * (k - lastSupportIdx);
                        stops[stopName].coords.lon = stops[startStop].coords.lon + lonStep * (k - lastSupportIdx);
                    }
                    lastSupportIdx = i;
                }
            }
        }
    }



private:


    std::unordered_set<std::string> findSupportStops() { 
        std::unordered_set<std::string> supportStops;
        std::unordered_map<std::string, int> stopsCount;

        for (const auto& [_, route] : getRoutes()) {
            for (const auto& stop : route.endPoints)
                supportStops.insert(stop);
            for (size_t i = 0; i < route.stops.size(); ++i) {
                const auto& stop = route.stops[i];
                int times = 1;
                if (route.isCyclic == false && i != route.stops.size() - 1)
                    times = 2;
                stopsCount[stop] += times;
            }
        }

        for (const auto& [stop, count] : stopsCount)
            if (count > 2)
                supportStops.insert(stop);

        for (const auto& [stop, params] : getStopStats())
            if (params.buses.size() > 1)
                supportStops.insert(stop); // По сути тут мы страхуемся только от ситуации когда два кольцевых автобуса разделяют 1 остановку

        return supportStops;
    }



    double calculateLengthNew(const std::vector<std::string>& stopNames, bool isCyclic) {

        double totalDistance = 0.;

        for (size_t i = 1; i < stopNames.size(); ++i) {
            const auto& n1 = stopNames[i - 1];
            const auto& n2 = stopNames[i];
            size_t idx1 = stopsIdx[n1];
            size_t idx2 = stopsIdx[n2];

            double dist{};
            if (stops[n1].distance.count(n2))
                dist = stops[n1].distance[n2];
            else
                dist = stops[n2].distance[n1];

            stopsDist[idx1][idx2] = dist;

            totalDistance += dist;
        }

        if (isCyclic == false) { //RoundTrip не симметричный в New случае
            for (size_t i = stopNames.size() - 1; i >= 1; --i) {
                const auto& n1 = stopNames[i];
                const auto& n2 = stopNames[i - 1];
                size_t idx1 = stopsIdx[n1];
                size_t idx2 = stopsIdx[n2];

                double dist{};
                if (stops[n1].distance.count(n2))
                    dist = stops[n1].distance[n2];
                else
                    dist = stops[n2].distance[n1];

                stopsDist[idx1][idx2] = dist;

                totalDistance += dist;
            }
        }

        return totalDistance;
    }


    double calculateLength(const std::vector<std::string>& stopNames, bool isCyclic) {

        double totalDistance = 0.;

        for (size_t i = 1; i < stopNames.size(); ++i) {
            const auto& n1 = stopNames[i - 1];
            const auto& n2 = stopNames[i];
            double dist = distanceBetween(stops[n1].coords, stops[n2].coords);
            totalDistance += dist;
        }

        if (isCyclic == false)
            totalDistance *= 2.0; //Round-trip

        return totalDistance * 1000.0; //Приводим к метрам
    }

    double distanceBetween(const Coordinates& lhs, const Coordinates& rhs) {
        static const double p = 3.1415926535 / 180.0;
        static const double EARTH_RADIUS_2 = 6371.0 * 2.0;
        double a = 0.5 - cos((rhs.lat - lhs.lat) * p) / 2 + cos(lhs.lat * p)
            * cos(rhs.lat * p) * (1 - cos((rhs.lon - lhs.lon) * p)) / 2;
        return EARTH_RADIUS_2 * asin(sqrt(a));
    }


    std::unordered_map<std::string, StopParams> stops; //in
    std::map<std::string, BusRoute> routes; //раньше было u_o замененно с целью упрощения алгоритма

    //Тоже можно убрать в RouteFinder
    std::unordered_map<std::string, BusStats> busStats;
    std::unordered_map<std::string, StopStats> stopStats;

    std::vector<Request> requests; //out
    std::vector<FindCompanyRequest> companyRequests;
    std::vector<RouteToCompanyRequest> routeToCompanyRequests;

    std::vector<std::string> stopsNames;
    std::unordered_map<std::string, size_t> stopsIdx;
    std::unordered_map<size_t, std::unordered_map<size_t, unsigned int>> stopsDist;

    std::unordered_map<std::string, size_t> companyIdx;
    std::unordered_map<std::string, std::set<std::string>>  companyNeighbors;
    std::vector<std::string> companyNames;
    std::vector<std::string> companyFullNames;

    //YellowPages
    
    std::unordered_map<uint64_t, std::string> rubrics;
    YellowPages::Database yellowPages;
    std::unordered_map<std::string, std::vector<std::pair<uint32_t, uint32_t>>> workInt;


public:

    //Maybe const auto& all?

    const std::vector<RouteToCompanyRequest>& getRouteToCompanyRequest() const { return routeToCompanyRequests;}
    const std::vector<FindCompanyRequest>& getCompanyRequests() const { return companyRequests; }
    const std::unordered_map<uint64_t, std::string>& getRubrics() const { return rubrics; }
    const YellowPages::Database& getYellowPages() const { return yellowPages; }

    const std::vector<Request>& getRequests() const { return requests; }
    const std::unordered_map<std::string, BusStats>& getBusStats() const { return busStats;  }
    const std::unordered_map<std::string, StopStats>& getStopStats() const { return stopStats;  }

    const size_t getStopsSize() const { return stopsNames.size(); }

    const std::unordered_map<std::string, StopParams>& getStops() const { return stops; }
    const std::map<std::string, BusRoute>& getRoutes() const { return routes;  }

    const std::vector<std::string>& getStopNames() const { return stopsNames; }
    const std::unordered_map<std::string, size_t>& getStopsIdx() const { return stopsIdx;  }
    const std::unordered_map<std::string, size_t>& getCompanyIdx() const { return companyIdx;  }
    const std::unordered_map<size_t, std::unordered_map<size_t, unsigned int>>& getStopsDist() const { return stopsDist; }

    const std::unordered_map<std::string, std::set<std::string>>& getCompanyNeighbors() const { return companyNeighbors; }
    const auto& getCompanyNames() const { return companyNames; }
    const auto& getCompanyFullNames() const { return companyFullNames; }

    const auto& getWorkIntervals() const { return workInt; }
};