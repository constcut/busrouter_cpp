#pragma once

#include "svg.h"
#include "json.h"
#include "parser.h"

#include "routefinder.h"

#include <vector>
#include <string>
#include <unordered_map>
#include <list>
#include <algorithm>
#include <numeric>
#include <unordered_set>
#include <stdexcept>

#include "transport_catalog.pb.h"




void colorSerialize(const Svg::Color& color, Database::SvgColor* colorSer) { //TODO move to svg color
        if (color.index() == 0) 
            colorSer->set_string_value("none");
        else if (color.index() == 1) 
            colorSer->set_string_value(std::get<std::string>(color));
        else if (color.index() == 2) {
            auto rgbSer = colorSer->mutable_rgb_value();
            auto rgbValue = std::get<Svg::Rgb>(color);
            rgbSer->set_red(rgbValue.red);
            rgbSer->set_green(rgbValue.green);
            rgbSer->set_blue(rgbValue.blue);
        }
        else if (color.index() == 3) {
            auto rgbaSer = colorSer->mutable_rgba_value();
            auto rgbaValue = std::get<Svg::Rgba>(color);
            rgbaSer->set_red(rgbaValue.red);
            rgbaSer->set_green(rgbaValue.green);
            rgbaSer->set_blue(rgbaValue.blue);
            rgbaSer->set_opacity(rgbaValue.opacity);
        }
    };


void colorDeserialize(Svg::Color& color, const Database::SvgColor& colorDeser) {
        if (colorDeser.has_rgb_value()) {
            const auto& rgbDeser = colorDeser.rgb_value();
            color = Svg::Rgb {static_cast<uint8_t>(rgbDeser.red()), 
                static_cast<uint8_t>(rgbDeser.green()), static_cast<uint8_t>(rgbDeser.blue())};
        }
        else if (colorDeser.has_rgba_value()) {
            const auto& rgbaDeser = colorDeser.rgba_value();
            color = Svg::Rgba {static_cast<uint8_t>(rgbaDeser.red()), static_cast<uint8_t>(rgbaDeser.green()), 
                static_cast<uint8_t>(rgbaDeser.blue()), rgbaDeser.opacity()};
        }
        else
            color = colorDeser.string_value();
};


Svg::Color readColor(const Json::Node& colorNode) {
        if (colorNode.hasString())
            return Svg::Color{ colorNode.AsString() };
        else {
            const auto& colorArr = colorNode.AsArray();
            if (colorArr.size() == 3)
                return Svg::Rgb{ static_cast<uint8_t>(colorArr[0].AsInt()),
                static_cast<uint8_t>(colorArr[1].AsInt()), static_cast<uint8_t>(colorArr[2].AsInt()) };
            else if (colorArr.size() == 4)
                return Svg::Rgba{ static_cast<uint8_t>(colorArr[0].AsInt()),
                static_cast<uint8_t>(colorArr[1].AsInt()), static_cast<uint8_t>(colorArr[2].AsInt()), colorArr[3].AsDouble() };
        }
        return Svg::NoneColor;
}



struct RendringSettings {
    double maxWidth;
    double maxHeight;
    double padding;

    std::vector<Svg::Color> palette;

    double lineWidth;
    Svg::Color underlayerColor;
    double underlayerWidth;

    double stopRadius;
    Svg::Point stopLabelOffset;
    int stopLabelFontSize;
    int busLabelFontSize;
    Svg::Point busLabelOffset;

    std::vector<std::string> layers;

    double outerMargin;

    double companyRadius;
    double companyLineWidth;

    void serialize(Database::RenderingSettings* settings) {
        settings->set_max_width(maxWidth); //TODO fun proto + settings as arguments
        settings->set_max_height(maxHeight);
        settings->set_padding(padding);
        settings->set_line_width(lineWidth);
        settings->set_underlayer_width(underlayerWidth);
        settings->set_stop_radius(stopRadius);
        settings->set_stop_label_font_size(stopLabelFontSize);
        settings->set_bus_label_font_size(busLabelFontSize);
        settings->set_outer_margin(outerMargin);
        settings->set_company_radius(companyRadius);
        settings->set_company_line_width(companyLineWidth);

        auto stopLabelSer = settings->mutable_stop_label_offset();
        stopLabelSer->set_x(stopLabelOffset.x);
        stopLabelSer->set_y(stopLabelOffset.y);

        auto busLabelSer= settings->mutable_bus_label_offset();
        busLabelSer->set_x(busLabelOffset.x);
        busLabelSer->set_y(busLabelOffset.y);

        auto underlayerColorSer = settings->mutable_underlayer_color();
        colorSerialize(underlayerColor, underlayerColorSer);

        for (const auto& l: layers) 
            settings->add_layers(l);
    }


    void deserialize(const Database::RenderingSettings& settings) {
        maxWidth = settings.max_width();
        maxHeight = settings.max_height();
        padding = settings.padding();
        lineWidth = settings.line_width();
        underlayerWidth = settings.underlayer_width();
        stopRadius = settings.stop_radius();
        stopLabelFontSize = settings.stop_label_font_size();
        busLabelFontSize = settings.bus_label_font_size();
        outerMargin = settings.outer_margin();
        companyRadius = settings.company_radius();
        companyLineWidth = settings.company_line_width();

        const auto& stopLabelOffsetDeser = settings.stop_label_offset();
        stopLabelOffset = {stopLabelOffsetDeser.x(), stopLabelOffsetDeser.y()};
        const auto& busLabelOffsetSer = settings.bus_label_offset();
        busLabelOffset = {busLabelOffsetSer.x(), busLabelOffsetSer.y()};
        const auto& underlayerColorDeser = settings.underlayer_color();
        colorDeserialize(underlayerColor, underlayerColorDeser);

        for (const auto& l: settings.layers())
            layers.push_back(l);
    }


    void set(const std::map<std::string, Json::Node>& settings) {
        maxWidth = settings.at("width").AsDouble();
        maxHeight = settings.at("height").AsDouble();
        padding = settings.at("padding").AsDouble();
        lineWidth = settings.at("line_width").AsDouble();
        stopRadius = settings.at("stop_radius").AsDouble();
        stopLabelFontSize = settings.at("stop_label_font_size").AsInt();
        underlayerWidth = settings.at("underlayer_width").AsDouble();
        underlayerColor = readColor(settings.at("underlayer_color"));

        const auto& pointArr = settings.at("stop_label_offset").AsArray();
        stopLabelOffset = Svg::Point{ pointArr[0].AsDouble(), pointArr[1].AsDouble() };

        const auto& paletteArr = settings.at("color_palette").AsArray();
        for (const auto& c : paletteArr)
            palette.push_back(readColor(c));

        busLabelFontSize = settings.at("bus_label_font_size").AsInt();
        const auto& pointArray = settings.at("bus_label_offset").AsArray();
        busLabelOffset = { pointArray[0].AsDouble(), pointArray[1].AsDouble() };
        
        const auto& layersArr = settings.at("layers").AsArray();
        for (const auto& layer : layersArr)
            layers.push_back(layer.AsString());
        
        outerMargin = settings.at("outer_margin").AsDouble();

        companyRadius = settings.at("company_radius").AsDouble();
        companyLineWidth = settings.at("company_line_width").AsDouble();
    }
};




class MapRender {

public:

    MapRender(const Parser& parser, const Json::Node& settingsNode) : parser(parser) {
        const auto& settings = settingsNode.AsMap();
        renderSettings.set(settings);
    } 

    MapRender(Database::TransportCatalog& db, const Parser& parser) 
        : parser(parser) 
    {
        deserialize(db);
        buildMap();
    } 


    void serialize(Database::TransportCatalog& db) { 

        prepareForMap(db);
         
        const auto& stopNames = parser.getStopNames();
        for (const auto& name: stopNames) {
            const auto& coord = stopCoordinates[name];
            auto stopCoord = db.add_stop_coords();
            stopCoord->set_x(coord.x);
            stopCoord->set_y(coord.y);
        }

        const auto& yp = db.yellow_pages();
        const auto& fullNames = parser.getCompanyFullNames();

        for (size_t i = 0; i < yp.companies_size(); ++i) {
            const auto& companyFullName = fullNames[i];
            const auto& coord = stopCoordinates[companyFullName];
            auto stopCoord = db.add_company_coords();
            stopCoord->set_x(coord.x);
            stopCoord->set_y(coord.y);
        }
        
        const auto& busRoutes = parser.getRoutes();
        for (const auto& [busName, _]: busRoutes) {
            auto colorSer = db.add_bus_colors();
            const auto& busColor = busColors[busName];
            colorSerialize(busColor, colorSer);
        }

        auto settings = db.mutable_render_settings();
        renderSettings.serialize(settings);
    }


    void deserialize(Database::TransportCatalog& db) {

        const auto& stopNames = parser.getStopNames();
        for (size_t i = 0; i < db.stop_coords_size(); ++i) {
            const auto& coord = db.stop_coords()[i];
            stopCoordinates[stopNames[i]] = {coord.x(), coord.y()};
        }

        const auto& yp = parser.getYellowPages();
        const auto& fullNames = parser.getCompanyFullNames();
        for (size_t i = 0; i < db.company_coords_size(); ++i) {
            const auto& coord = db.company_coords()[i];
            const auto& companyFullName = fullNames[i];
            stopCoordinates[companyFullName] = {coord.x(), coord.y()}; 
        }

        for (size_t i = 0; i < db.bus_names_size(); ++i) {
            const auto& name = db.bus_names()[i];
            const auto& colorDeser = db.bus_colors()[i];
            Svg::Color busColor;
            colorDeserialize(busColor,colorDeser);
            busColors[name] = busColor;
        }

        const auto& settings = db.render_settings();
        renderSettings.deserialize(settings);
    }


    void buildRoute(const RouteAction& route, std::ostream& os) const {

        if (cache.empty()) 
            mapDoc.RenderNoEnd(os);
        else
            os << cache;

        Svg::Document routeDoc;
        addRouteTransparentRect(routeDoc);
        if (route.actions.size() != 0)
            for (const auto& layer : renderSettings.layers)
                if (routeRenderFunctions.at(layer))
                    (this->*routeRenderFunctions.at(layer))(routeDoc, route);
        routeDoc.RenderNoStart(os);
    }


    void buildCompanyRoute(const RouteAction& route, std::ostream& os) const {

        if (cache.empty()) 
            mapDoc.RenderNoEnd(os);
        else
            os << cache;
            
        Svg::Document routeDoc;
        addRouteTransparentRect(routeDoc);
        if (route.actions.size() != 0)
            for (const auto& layer : renderSettings.layers)
                (this->*routeCompanyRenderFunctions.at(layer))(routeDoc, route);
        routeDoc.RenderNoStart(os);
    }

private:

    std::string cache;

    void buildMap() {
        Svg::Document doc;
        for (const auto& layer : renderSettings.layers)
            if (renderFunctions.at(layer))
                (this->*renderFunctions.at(layer))(doc);
        mapDoc = std::move(doc);
        if (cache.empty()) {
            std::ostringstream oss;
            try {
                mapDoc.RenderNoEnd(oss);
            }
            catch (std::runtime_error& re) {
                return;
            }
            cache = oss.str();
        }
    }


    void prepareForMap(Database::TransportCatalog& db) {
        coordinatesToSvg(db);
        glueAndCompressCoordinates();
        colorBuses();
    }


    void addRouteTransparentRect(Svg::Document& doc) const {
        auto rect = Svg::Rectangle{};
        rect.SetPosition({ -renderSettings.outerMargin, -renderSettings.outerMargin })
            .SetWidth(renderSettings.maxWidth + renderSettings.outerMargin * 2)
            .SetHeight(renderSettings.maxHeight + renderSettings.outerMargin * 2)
            .SetFillColor(renderSettings.underlayerColor);
        doc.Add(rect);
    }


    void coordinatesToSvg(Database::TransportCatalog& db) {
        const auto& stops = parser.getStops();
        double minLat = std::numeric_limits<double>::max();
        double minLon = std::numeric_limits<double>::max();
        double maxLat = std::numeric_limits<double>::min();
        double maxLon = std::numeric_limits<double>::min();

        auto checkMinMax = [&](double lat, double lon) {
            if (lat > maxLat)
                maxLat = lat;
            if (lat < minLat)
                minLat = lat;
            if (lon > maxLon)
                maxLon = lon;
            if (lon < minLon)
                minLon = lon;
        };

        for (const auto& [stopName, stopStat] : stops) 
            checkMinMax(stopStat.coords.lat, stopStat.coords.lon);

        const auto& yp = db.yellow_pages();
        for (const auto& company: yp.companies()) 
            checkMinMax(company.address().coords().lat(), company.address().coords().lon());
        
        double widthZoomCoef = (renderSettings.maxWidth - 2 * renderSettings.padding) / (maxLon - minLon);
        double heightZoomCoef = (renderSettings.maxHeight - 2 * renderSettings.padding) / (maxLat - minLat);
        double coef;
        if (maxLat - minLat == 0 && maxLon - minLon == 0)
            coef = 0;
        else if (maxLon - minLon == 0)
            coef = heightZoomCoef;
        else if (maxLat - minLat == 0)
            coef = widthZoomCoef;
        else
            coef = std::min(widthZoomCoef, heightZoomCoef);

        for (const auto& [stopName, stopStat] : stops) {
            double x = (stopStat.coords.lon - minLon) * coef + renderSettings.padding;
            double y = (maxLat - stopStat.coords.lat) * coef + renderSettings.padding;
            stopCoordinates[stopName] = { x, y };
        }

        const auto& fullNames = parser.getCompanyFullNames();
        for (size_t i = 0; i < yp.companies_size(); ++i) {
            std::string companyFullName = fullNames[i];
            const auto& coords = yp.companies()[i].address().coords();
            double x = (coords.lon() - minLon) * coef + renderSettings.padding;
            double y = (maxLat - coords.lat()) * coef + renderSettings.padding;
            stopCoordinates[companyFullName] = { x, y };
        }
    }

    std::set<std::string> findNeighbours(const std::string& stopName) const { 
        const auto& stopStats = parser.getStopStats();
        const auto& companyNeighbors = parser.getCompanyNeighbors();
        if (stopStats.count(stopName)) 
            return stopStats.at(stopName).neighbors;
        else 
            return companyNeighbors.at(stopName);
    }


    size_t assignIdx(const std::vector<std::pair<double, std::string>>& coords, std::unordered_map<std::string, size_t>& assignedIdx) {
        size_t maxFoundIdx = 0;
        assignedIdx[coords[0].second] = 0;

        for (size_t i = 1; i < coords.size(); ++i) {
            const auto& stopName = coords[i].second;
            const auto& neighbours = findNeighbours(stopName);

            int maxIdx = -1;
            for (const auto& n : neighbours)
                if (assignedIdx.count(n))
                    if (maxIdx < static_cast<int>(assignedIdx[n]))
                        maxIdx = static_cast<int>(assignedIdx[n]);
            assignedIdx[stopName] = maxIdx + 1;
            if (maxFoundIdx < static_cast<size_t>(maxIdx + 1))
                maxFoundIdx = static_cast<size_t>(maxIdx + 1);
        }
        return maxFoundIdx;
    }


    void glueAndCompressCoordinates() {
        std::vector<std::pair<double, std::string>> xCoords, yCoords;
        xCoords.reserve(stopCoordinates.size());
        yCoords.reserve(stopCoordinates.size());
        for (const auto& [stopName, stopCoords] : stopCoordinates) {
            xCoords.push_back(make_pair(stopCoords.x, stopName ));
            yCoords.push_back(make_pair(stopCoords.y, stopName ));
        }
        if (xCoords.size() > 1) {
            sort(xCoords.begin(), xCoords.end(), [](const auto& lhs, const auto& rhs) { return lhs.first < rhs.first; });
            sort(yCoords.begin(), yCoords.end(), [](const auto& lhs, const auto& rhs) { return lhs.first > rhs.first; });

            std::unordered_map<std::string, size_t> xIdx, yIdx;
            size_t XbiggestIdx = assignIdx(xCoords, xIdx);
            size_t YbiggestIdx = assignIdx(yCoords, yIdx);

            double xStep = (renderSettings.maxWidth - 2 * renderSettings.padding) / XbiggestIdx; 
            double yStep = (renderSettings.maxHeight - 2 * renderSettings.padding) / YbiggestIdx; 

            for (size_t i = 0; i < xCoords.size(); ++i) {
                const auto& stopNameX = xCoords[i].second; 
                stopCoordinates[stopNameX].x = renderSettings.padding + xIdx[stopNameX] * xStep;
                const auto& stopNameY = yCoords[i].second; 
                stopCoordinates[stopNameY].y = renderSettings.maxHeight - renderSettings.padding - yIdx[stopNameY] * yStep;
            }
        }
        else if (xCoords.size() == 1) {
            stopCoordinates[xCoords[0].second].x = renderSettings.padding;
            stopCoordinates[xCoords[0].second].y = renderSettings.maxHeight - renderSettings.padding;
        }
    }


    void colorBuses() {
        const auto& routes = parser.getRoutes();
        size_t colorIndex = 0;
        for (const auto& [busName, _] : routes) {
            size_t idx = colorIndex % renderSettings.palette.size();
            busColors[busName] = renderSettings.palette[idx];
            ++colorIndex;
        }
    }


    void renderBusLabels(Svg::Document& doc) const {
        const auto& routes = parser.getRoutes();
        for (const auto& [busName, busRoute] : routes) {
            const auto& stops = busRoute.stops;
            if (stops.empty())
                continue;
            for (const auto& stopName : busRoute.endPoints) 
                addBusLabel(doc, busName, stopName);
        }
    }


    void renderStopLabels(Svg::Document& doc) const {
        const auto& stops = parser.getStopStats();
        for (const auto& [stopName, stopPoint] : stopCoordinates) {
            if (stops.count(stopName))
                addStopLabel(doc, stopName);
        }
    }


    void renderStopPoints(Svg::Document& doc) const {
        const auto& stops = parser.getStopStats();
        for (const auto& [stopName, stopPoint] : stopCoordinates) {
            if (stops.count(stopName))
                addStopPoint(doc, stopName);
        }
    }


    void renderBusLines(Svg::Document& doc) const {
        const auto& routes = parser.getRoutes();

        for (const auto& [busName, busRoute] : routes) {
            const auto& stops = busRoute.stops;
            if (stops.empty())
                continue;
            Svg::Polyline line;
            line.SetStrokeColor(busColors.at(busName))
                .SetStrokeWidth(renderSettings.lineWidth)
                .SetStrokeLineCap("round").SetStrokeLineJoin("round");
            for (const auto& stopName : stops)
                line.AddPoint(stopCoordinates.at(stopName));

            if (busRoute.isCyclic == false)
                for (int i = stops.size() - 2; i >= 0; --i)  //? возможно стоит сделать лайфак от авторов, привести сразу к одному виду 
                    line.AddPoint(stopCoordinates.at(stops[i]));

            doc.Add(line);
        }
    }


    void addStopPoint(Svg::Document& doc, const std::string& stopName) const {
        const auto& stopPoint = stopCoordinates.at(stopName);
        doc.Add(Svg::Circle{}
            .SetCenter(stopPoint)
            .SetRadius(renderSettings.stopRadius)
            .SetFillColor("white"));
    }


    void addStopLabel(Svg::Document& doc, const std::string& stopName) const {
        const auto& stopPoint = stopCoordinates.at(stopName);
        const auto text = Svg::Text{}.SetPoint(stopPoint)
            .SetOffset(renderSettings.stopLabelOffset)
            .SetFontSize(renderSettings.stopLabelFontSize)
            .SetFontFamily("Verdana")
            .SetData(stopName);
        doc.Add(
            Svg::Text(text)
            .SetFillColor(renderSettings.underlayerColor)
            .SetStrokeColor(renderSettings.underlayerColor)
            .SetStrokeWidth(renderSettings.underlayerWidth)
            .SetStrokeLineCap("round").SetStrokeLineJoin("round"));
        doc.Add(
            Svg::Text(text).SetFillColor("black"));
    }


    void addBusLabel(Svg::Document& doc, const std::string& busName, const std::string& stopName) const {
        const auto& point = stopCoordinates.at(stopName);
        const auto baseText = Svg::Text{}.SetPoint(point)
            .SetOffset(renderSettings.busLabelOffset)
            .SetFontSize(renderSettings.busLabelFontSize)
            .SetFontFamily("Verdana")
            .SetFontWeight("bold")
            .SetData(busName);
        doc.Add(
            Svg::Text(baseText).SetFillColor(renderSettings.underlayerColor)
            .SetStrokeColor(renderSettings.underlayerColor)
            .SetStrokeWidth(renderSettings.underlayerWidth)
            .SetStrokeLineCap("round")
            .SetStrokeLineJoin("round"));
        doc.Add(Svg::Text(baseText).SetFillColor(busColors.at(busName)));
    }


    using StopIt = std::vector<std::string>::const_iterator;


    void addBusLine(Svg::Document& doc, const std::string& busName,
        StopIt firstIt, StopIt lastIt) const { 

        Svg::Polyline line;
        line.SetStrokeColor(busColors.at(busName))
            .SetStrokeWidth(renderSettings.lineWidth)
            .SetStrokeLineCap("round").SetStrokeLineJoin("round");

        if (lastIt > firstIt) {
            for (auto& it = firstIt; ; ++it) {
                line.AddPoint(stopCoordinates.at(*it));
                if (it == lastIt)
                    break;
            }
        }
        else
            for (auto& it = firstIt;; --it) {
                line.AddPoint(stopCoordinates.at(*it));
                if (it == lastIt)
                    break;
            }

        doc.Add(line);
    }



    std::pair<StopIt,StopIt> findStopPair(const RouteAction& route, size_t i) const { //TODO возможно ошибка тут
        const auto& allRoutes = parser.getRoutes();

        const auto& stopName = route.actions.at(i).name;
        const auto& busName = route.actions.at(i + 1).name;
        const auto& busRoute = allRoutes.at(busName);
        std::string nextStopName;
        if (i < route.actions.size() - 2)
            nextStopName = route.actions.at(i + 2).name;
        else
            nextStopName = route.finalStop;

        std::vector<StopIt> firstCandidates;
        StopIt firstStop = find(busRoute.stops.begin(), busRoute.stops.end(), stopName);
        while (firstStop != busRoute.stops.end()) {
            firstCandidates.push_back(firstStop);
            firstStop = find(firstStop + 1, busRoute.stops.end(), stopName);
        }
        
        std::vector<StopIt> lastCandidates;
        std::vector<std::string>::const_iterator lastStop;
        if (busRoute.isCyclic == false)
            lastStop = find(busRoute.stops.begin(), busRoute.stops.end(), nextStopName);
        else
            lastStop = find(busRoute.stops.begin() + 1, busRoute.stops.end(), nextStopName);
        while (lastStop != busRoute.stops.end()) {
            lastCandidates.push_back(lastStop);
            lastStop = find(lastStop + 1, busRoute.stops.end(), nextStopName);
        }

        int spanCount = route.actions.at(i + 1).spans;
        size_t foundCandidate1 = 0, foundCandidate2 = 0;

        bool found = false;
        for (size_t i = 0; i < firstCandidates.size(); ++i) {
            for (size_t j = 0; j < lastCandidates.size(); ++j) 
                if (int diff = lastCandidates[j] - firstCandidates[i]; diff == spanCount) {
                    foundCandidate1 = i;
                    foundCandidate2 = j;
                    found = true;
                    break;
                }
            if (found)
                break;
        }
        if (busRoute.isCyclic == false && found == false)
            for (size_t i = 0; i < firstCandidates.size(); ++i) {
                for (size_t j = 0; j < lastCandidates.size(); ++j) 
                    if (int diff = lastCandidates[j] - firstCandidates[i]; diff == -spanCount) {
                        foundCandidate1 = i;
                        foundCandidate2 = j;
                        found = true;
                        break;
                    }
                if (found)
                    break;
        }

        firstStop = firstCandidates[foundCandidate1];
        lastStop = lastCandidates[foundCandidate2];
        return { firstStop, lastStop };
    }


    void renderRouteBusLines(Svg::Document& doc, const RouteAction& route) const { 
        for (size_t i = 0; i < route.actions.size() - 1; ++i) 
            if (route.actions[i].type == "WaitBus") {
                const auto& busName = route.actions[i + 1].name;
                const auto& p = findStopPair(route, i);
                addBusLine(doc, busName, p.first, p.second);
            }
    }


    void renderRouteStopCircles(Svg::Document& doc, const RouteAction& route) const {
        for (size_t i = 0; i < route.actions.size() - 1; ++i) 
            if (route.actions[i].type == "WaitBus") {
                const auto& p = findStopPair(route, i);

                if (p.second > p.first) {
                    for (auto it = p.first; it != (p.second + 1); ++it)
                        addStopPoint(doc, *it);
                }
                else
                    for (auto it = p.first;; --it) {
                        addStopPoint(doc, *it);
                        if (it == p.second)
                            break;
                    }
            }
    }



    void renderStopLabelsCommon(Svg::Document& doc, const RouteAction& route) const {
        for (size_t i = 0; i < route.actions.size() - 1; ++i) 
            if (route.actions[i].type == "WaitBus") {
                const auto& stopName = route.actions[i].name;
                addStopLabel(doc, stopName);
            }
    }

    void renderRouteStopLabels(Svg::Document& doc, const RouteAction& route) const {
        renderStopLabelsCommon(doc, route);
        addStopLabel(doc, route.finalStop);
    }


    void renderCompanyRouteStopLabels(Svg::Document& doc, const RouteAction& route) const { 
        renderStopLabelsCommon(doc, route);
        const auto& finalStop = route.actions.back().name;
        addStopLabel(doc, finalStop);
    }   


    void renderEndpointsCommon(Svg::Document& doc, const RouteAction& route) const {
        const auto& allRoutes = parser.getRoutes();
        for (size_t i = 0; i < route.actions.size() - 1; ++i) {
            if (route.actions[i].type == "WaitBus") {
                const auto& stopName = route.actions[i].name;
                const auto& busName = route.actions[i + 1].name;
                const auto& busRoute = allRoutes.at(busName);
                if (i > 0) {
                    const auto& prevBusName = route.actions[i - 1].name;
                    const auto& prevRoute = allRoutes.at(prevBusName);
                    if (find(prevRoute.endPoints.begin(), prevRoute.endPoints.end(), stopName) != prevRoute.endPoints.end())
                        addBusLabel(doc, prevBusName, stopName);
                }
                if (find(busRoute.endPoints.begin(), busRoute.endPoints.end(), stopName) != busRoute.endPoints.end())
                    addBusLabel(doc, busName, stopName);
            }
        }
    }

    void renderRouteEndpoints(Svg::Document& doc, const RouteAction& route) const {
        renderEndpointsCommon(doc, route);
        const auto& allRoutes = parser.getRoutes();
        const auto& lastBusName = route.actions.back().name;
        const auto& busRoute = allRoutes.at(lastBusName);
        if (find(busRoute.endPoints.begin(), busRoute.endPoints.end(), route.finalStop) != busRoute.endPoints.end())
            addBusLabel(doc, lastBusName, route.finalStop);
    }

    void renderCompanyRouteEndpoints(Svg::Document& doc, const RouteAction& route) const { 
        renderEndpointsCommon(doc, route);
        const auto& allRoutes = parser.getRoutes();
        if (route.actions.size() > 1) {
            size_t idx = route.actions.size() - 2;
            const auto& lastBusName = route.actions[idx].name;
            const auto& busRoute = allRoutes.at(lastBusName);
            const auto& finalStop = route.actions.back().name;
            if (find(busRoute.endPoints.begin(), busRoute.endPoints.end(), finalStop) != busRoute.endPoints.end())
                addBusLabel(doc, lastBusName, finalStop);
        }
    }


    void renderCompanyLines(Svg::Document& doc, const RouteAction& route) const { 
        const auto& lastStop = route.actions.back().name;
        const auto& company = route.actions.back().companyName;
        const auto& companyIdx = parser.getCompanyIdx();
        const auto& fullNames = parser.getCompanyFullNames();
        const auto& fullName = fullNames[companyIdx.at(company)];
        const auto& stopPoint =  stopCoordinates.at(lastStop);
        const auto& companyPoint =  stopCoordinates.at(fullName);

        auto line = Svg::Polyline{}.SetStrokeColor("black")
                        .SetStrokeWidth(renderSettings.companyLineWidth)
                        .SetStrokeLineCap("round")
                        .SetStrokeLineJoin("round");
        line.AddPoint({stopPoint.x, stopPoint.y});
        line.AddPoint({companyPoint.x, companyPoint.y});            
        doc.Add(line);
    }

    void renderCompanyPoints(Svg::Document& doc, const RouteAction& route) const { 
        const auto& companyIdx = parser.getCompanyIdx();
        const auto& fullNames = parser.getCompanyFullNames();
        const auto& companyName = route.actions.back().companyName;
        const auto& fullName = fullNames[companyIdx.at(companyName)];
        const auto& stopPoint = stopCoordinates.at(fullName);
        doc.Add(Svg::Circle{}
            .SetCenter(stopPoint)
            .SetRadius(renderSettings.companyRadius)
            .SetFillColor("black")); //Add point with param? on refact
    }

    void renderCompanyLabels(Svg::Document& doc, const RouteAction& route) const { 
        const auto& companyIdx = parser.getCompanyIdx();
        const auto& fullNames = parser.getCompanyFullNames();
        const auto& companyName = route.actions.back().companyName;
        const auto& fullName = fullNames[companyIdx.at(companyName)];
        addStopLabel(doc, fullName);
    }


private:
    const Parser& parser; 

    RendringSettings renderSettings;
    std::map<std::string, Svg::Point> stopCoordinates;
    std::unordered_map<std::string, Svg::Color> busColors;
    Svg::Document mapDoc;

    std::unordered_map<std::string, void (MapRender::*)(Svg::Document& doc) const> renderFunctions = {
        {"bus_lines", &MapRender::renderBusLines}, 
        {"bus_labels", &MapRender::renderBusLabels},
        {"stop_points", &MapRender::renderStopPoints},
        {"stop_labels", &MapRender::renderStopLabels},
        {"company_lines", nullptr},
        {"company_points", nullptr},
        {"company_labels", nullptr}
    };


    std::unordered_map<std::string, void (MapRender::*)(Svg::Document& doc, const RouteAction& route) const> routeRenderFunctions = {
        {"bus_lines", &MapRender::renderRouteBusLines},
        {"bus_labels", &MapRender::renderRouteEndpoints},
        {"stop_points", &MapRender::renderRouteStopCircles},
        {"stop_labels", &MapRender::renderRouteStopLabels},
        {"company_lines", nullptr},
        {"company_points", nullptr}, 
        {"company_labels", nullptr}
    };

    std::unordered_map<std::string, void (MapRender::*)(Svg::Document& doc, const RouteAction& route) const> routeCompanyRenderFunctions = {
        {"bus_lines", &MapRender::renderRouteBusLines},
        {"bus_labels", &MapRender::renderCompanyRouteEndpoints},
        {"stop_points", &MapRender::renderRouteStopCircles},
        {"stop_labels", &MapRender::renderCompanyRouteStopLabels},
        {"company_lines", &MapRender::renderCompanyLines},
        {"company_points", &MapRender::renderCompanyPoints},
        {"company_labels", &MapRender::renderCompanyLabels}
    };


public:

    const Svg::Document& getMap() const { return mapDoc;  }
};