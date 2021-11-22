#pragma once

#include <istream>
#include <map>
#include <string>
#include <variant>
#include <vector>

namespace Json {

    class Node : std::variant<std::vector<Node>,
        std::map<std::string, Node>,
        int,
        bool,
        double,
        std::string> {

    public:

        using variant::variant;

        bool hasString() const {
            return std::holds_alternative<std::string>(*this); //Подумать как улучшить потом
        }

        const auto& AsArray() const {
            return std::get<std::vector<Node>>(*this);
        }

        const auto& AsMap() const {
            return std::get<std::map<std::string, Node>>(*this);
        }

        int AsInt() const {
            return std::get<int>(*this);
        }

        bool AsBool() const {
            return std::get<bool>(*this);
        }

        double AsDouble() const {
            if (std::holds_alternative<double>(*this))
                return std::get<double>(*this);
            return std::get<int>(*this); //На случай плавующих значений
        }

        const auto& AsString() const {
            return std::get<std::string>(*this);
        }

        std::ostream& PushToStream(std::ostream& os) const;
    };

    class Document {
    public:
        explicit Document(Node root);

        const Node& GetRoot() const;

    private:
        Node root;
    };

    Document Load(std::istream& input);

}

std::ostream& operator<<(std::ostream& os, const Json::Node& node);

