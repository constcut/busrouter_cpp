#pragma once

#include "parser.h"
#include "json.h"
#include "route.h"

#include <vector>
#include <string>
#include <memory>

#include "transport_catalog.pb.h"



struct EdgeAction {
    std::string type;
    double time;
    std::string name;
    unsigned int spans;
    std::string companyName = ""; //Adaptation for part S
};

struct RouteAction { //Возможно стоит вынести эту структуру отдельно, чтобы не подключать в maprender.h всю
    bool notFound;
    double totalTime;
    std::vector<EdgeAction> actions;
    std::string finalStop;
};


class RouteFinder {

public:

  using BusGraph = Graph::DirectedWeightedGraph<double>;
  using Router = Graph::Router<double>;

    RouteAction findRoute(const std::string& from, const std::string& to, const Parser& parser) const {

        const auto& stopsIdx = parser.getStopsIdx();

        size_t fromIdx = 2 * stopsIdx.at(from);
        size_t toIdx;
        
        if (stopsIdx.count(to)) 
            toIdx = 2 * stopsIdx.at(to); 
        else {
            const auto& companyIdx = parser.getCompanyIdx();
            toIdx = stopsIdx.size() * 2 + companyIdx.at(to);
        }
         
        auto foundRoute = router->BuildRoute(fromIdx, toIdx);

        RouteAction routeAction;
        if (foundRoute.has_value() == false) {
            routeAction.notFound = true;
            return routeAction;
        }
        routeAction.notFound = false;
        routeAction.finalStop = to;

        std::vector<EdgeAction> routeActions;
        auto id = foundRoute.value().id;

        for (size_t i = 0; i < foundRoute.value().edge_count; ++i) {
            auto edgeIdx = router->GetRouteEdge(id, i);
            routeAction.actions.push_back(edgeActions[edgeIdx]);
        }

        routeAction.totalTime = foundRoute.value().weight;
        return routeAction;
    }




    void createAndSerialize(const Parser& parser, const Json::Node& routeNode, Database::TransportCatalog& db) {

        setSettings(routeNode);

        auto graphSize = 2 * parser.getStopsSize() + db.yellow_pages().companies_size();
        graph = BusGraph(graphSize);
        
        db.set_bus_wait_time(busWaitTime);

        const auto& stopsNames = parser.getStopNames();
        for (size_t i = 0; i < stopsNames.size(); ++i) 
            graph.AddEdge(Graph::Edge<double>{ 2 * i, 2 * i + 1, busWaitTime});

        const auto& routes = parser.getRoutes();
        const auto& stopsIdx = parser.getStopsIdx();
        const auto& stopsDist = parser.getStopsDist();

        for (const auto& [busName, bus] : routes) {
            const auto& busStops = bus.stops;

            auto busSerial = db.add_bus_edges();
            busSerial->set_bus_name(busName);

            for (size_t i = 0; i < busStops.size(); ++i) { //TODO внутренность в функцию
                double totalTime{};
                unsigned int spanCount{};
                for (size_t j = i + 1; j < busStops.size(); ++j) {
                    size_t idx1 = stopsIdx.at(busStops[j - 1]);
                    size_t idx2 = stopsIdx.at(busStops[j]);
                    totalTime += stopsDist.at(idx1).at(idx2) / (busVelocity * 1000 / 60); 
                    graph.AddEdge(Graph::Edge<double>{ 2 * stopsIdx.at(busStops[i]) + 1, 2 * idx2, totalTime });
                    ++spanCount;
                    auto element = busSerial->add_elements();
                    element->set_total_time(totalTime);
                    element->set_spans_count(spanCount);
                    element->set_idx1(2 * stopsIdx.at(busStops[i]) + 1);
                    element->set_idx2(2 * idx2);
                }
            }

            if (bus.isCyclic == false) 
                for (size_t i = 0; i < busStops.size(); ++i) { 
                    double totalTime{};
                    unsigned int spanCount{};
                    for (int j = i - 1; j >= 0; --j) {
                        size_t idx1 = stopsIdx.at(busStops[j]);   //idx1->2 2->1
                        size_t idx2 = stopsIdx.at(busStops[j + 1]);
                        totalTime += stopsDist.at(idx2).at(idx1) / (busVelocity * 1000 / 60); 
                        graph.AddEdge(Graph::Edge<double>{ 2 * stopsIdx.at(busStops[i]) + 1, 2 * idx1, totalTime });
                        ++spanCount;
                        auto element = busSerial->add_elements();
                        element->set_total_time(totalTime);
                        element->set_spans_count(spanCount);
                        element->set_idx1(2 * stopsIdx.at(busStops[i]) + 1);
                        element->set_idx2(2 * idx1);
                    }
                }
        }

        const auto& companyNames = parser.getCompanyNames();
        for (size_t i = 0; i < db.yellow_pages().companies_size(); ++i) {
            const auto& company = db.yellow_pages().companies()[i];
            size_t companyIdx = 2 * parser.getStopsSize() + i;
            const auto& companyName = companyNames[i];
            auto companySer = db.add_company_edges();
            companySer->set_company_name(companyName);
            for (const auto& stop: company.nearby_stops()) {
                size_t stopIdx = stopsIdx.at(stop.name()) * 2;
                double time = stop.meters() / (pedestrianVelocity * 1000 / 60);
                graph.AddEdge(Graph::Edge<double>{ stopIdx, companyIdx, time });
                auto element = companySer->add_elements();
                element->set_total_time(time);
                element->set_idx1(stopIdx); 
                element->set_idx2(companyIdx);
            }
        }

        router = std::make_unique<Graph::Router<double>>(graph);
        auto r = db.mutable_router();
        router->Serialize(*r->mutable_router());
    }


    void deserialize(const Parser& parser, Database::TransportCatalog& db) {

        busWaitTime = db.bus_wait_time();
        auto graphSize = 2 * parser.getStopsSize() + db.yellow_pages().companies_size();
        graph = BusGraph(graphSize);

        const auto& stopsNames = parser.getStopNames();
        for (size_t i = 0; i < stopsNames.size(); ++i) {
            graph.AddEdge(Graph::Edge<double>{ 2 * i, 2 * i + 1, busWaitTime});
            edgeActions.push_back(EdgeAction{ "WaitBus", busWaitTime, stopsNames[i], 0 });
        }

        for (const auto& busEdge: db.bus_edges()) {
            const auto& busName = busEdge.bus_name();
            for (const auto& element: busEdge.elements()) {
                graph.AddEdge(Graph::Edge<double>{element.idx1(), 
                    element.idx2(), element.total_time() });
                edgeActions.push_back(EdgeAction{ "RideBus", element.total_time(), 
                    busName, element.spans_count() });
            }
        }

        const auto& stopNames = parser.getStopNames();
        for (const auto& companyEdge: db.company_edges()) {
            const auto& companyName = companyEdge.company_name();
            for (const auto& element: companyEdge.elements()) {
                const auto& stopName = stopNames[element.idx1()/2];
                graph.AddEdge(Graph::Edge<double>{element.idx1(), 
                    element.idx2(), element.total_time() });
                edgeActions.push_back(EdgeAction{ "WalkToCompany", element.total_time(), 
                    stopName, 0, companyName });
            }
        }

        auto& proto = db.router();
        router = Router::Deserialize(proto.router(), graph);
    }


private:

    void setSettings(const Json::Node& routeNode) {
        const auto& routeSettings = routeNode.AsMap();
        busWaitTime = routeSettings.at("bus_wait_time").AsInt();
        busVelocity = routeSettings.at("bus_velocity").AsInt();
        pedestrianVelocity = routeSettings.at("pedestrian_velocity").AsDouble();
    }

    double busWaitTime; //int заменен на double для рассчётов
    double busVelocity; //и отсутствия сужающего преобразования
    double pedestrianVelocity;

    BusGraph graph;
    std::unique_ptr<Graph::Router<double>> router{ nullptr };
    std::vector<EdgeAction> edgeActions;
    size_t waitEdgeBorder; //Для сериализации
};