#include "json.h"

using namespace std;

namespace Json {

    Document::Document(Node root) : root(move(root)) {
    }

    const Node& Document::GetRoot() const {
        return root;
    }

    Node LoadNode(istream& input);


    Node LoadArray(istream& input) {
        vector<Node> result;

        for (char c; input >> c && c != ']'; ) {
            if (c != ',') {
                input.putback(c);
            }
            result.push_back(LoadNode(input));
        }

        return Node(move(result));
    }


    Node LoadNumber(istream& input) {
        
        int signValue = 1; //������ ���� � �������� ����������, � �������� ��������� ��������� �� �������� -1 ��� 1
        if (input.peek() == '-') {
            input.ignore();
            signValue = -1;
        }

        int intValue = 0; //�� ������� ���� ����, ������� ������� :)
        while (isdigit(input.peek())) {
            intValue *= 10;
            intValue += input.get() - '0';
        }

        if (input.peek() != '.') 
            return Node(signValue * intValue); //������������� �����, ������������� ������
        
        input.ignore(); //���������� �����������

        double doubleValue = intValue; //�������� ����� ������������� �����
        double coef = 0.1; 
        while (isdigit(input.peek())) {
            doubleValue += (input.get() - '0') * coef;
            coef *= 0.1; //������ ����� ������ ���� � 10 ��� ������ ��������������
        }
        return Node(signValue * doubleValue);
    }


    Node LoadString(istream& input) {
        string line;
        getline(input, line, '"');
        return Node(move(line));
    }


    Node LoadDict(istream& input) {
        map<string, Node> result;

        for (char c; input >> c && c != '}'; ) {
            if (c == ',') {
                input >> c;
            }

            string key = LoadString(input).AsString();
            input >> c;
            result.emplace(move(key), LoadNode(input));
        }

        return Node(move(result));
    }


    Node LoadBool(istream& input) {
        bool result = (input.peek() == 't'); //���� ������ true, ������ ����� �������������� t ��� f - ��� ���������� ���� �� ����� �������
        while (isalpha(input.peek()))  
            input.get();    //���������� ����������� ����� ��� ���� true, ���� false
        return Node(result);
    }


    Node LoadNode(istream& input) {
        char c;
        input >> c;

        if (c == '[') {
            return LoadArray(input);
        }
        else if (c == '{') {
            return LoadDict(input);
        }
        else if (c == '"') {
            return LoadString(input);
        }
        else if (c == 't' || c == 'f') {
            input.putback(c);
            return LoadBool(input);
        }
        else {
            input.putback(c);
            return LoadNumber(input);
        }
    }


    Document Load(istream& input) {
        return Document{ LoadNode(input) };
    }


    std::ostream& Node::PushToStream(std::ostream& os) const {
        if (holds_alternative<int>(*this)) 
            os << AsInt();
        else if (holds_alternative<double>(*this)) 
            os  << AsDouble();
        else if (holds_alternative<bool>(*this)) 
            os << (AsBool() ? "true" : "false");
        else if (holds_alternative<string>(*this)) 
            os << '"' << AsString() << '"';
        else if (holds_alternative<vector<Node>>(*this)) {
            const auto& nodes = AsArray();
            os << "[\n";
            bool firstElement = true;
            for (const auto& node : nodes) {
                if (!firstElement) 
                    os << ",\n";
                firstElement = false;
                os << node;
            }
            os << "\n]";
        }
        else if (holds_alternative<map<string, Node>>(*this)) {
            const auto& nodes_map = AsMap();
            os << "{\n";
            bool firstElement = true;
            for (const auto& [key, node] : nodes_map) {
                if (!firstElement) 
                    os << ",\n";
                firstElement = false;
                os << '"' << key << "\":" << node;
            }
            os << "\n}";
        }
        return os;
    }

}


ostream& operator<<(ostream& os, const Json::Node& node) {
    return node.PushToStream(os);
}