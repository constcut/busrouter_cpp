#pragma once

#include <cstdint>
#include <variant>
#include <string>
#include <iostream>
#include <memory>
#include <vector>
#include <optional>
#include <utility>

#include "profile.h"


namespace Svg {

	struct Rgb {
		uint8_t red;
		uint8_t green;
		uint8_t blue;
	};

	struct Rgba : Rgb {
		double opacity;
	};

	using Color = std::variant<std::monostate, std::string, Rgb, Rgba>;
	const Color NoneColor{};

	void RenderColor(std::ostream& os, std::monostate) {
		os << "none";
	}

	void RenderColor(std::ostream& os, std::string str) {
		os << str; 
	}

	void RenderColor(std::ostream& os, Rgb rgb) {
		os << "rgb(" << static_cast<int>(rgb.red) << ","
			<< static_cast<int>(rgb.green) << ","
			<< static_cast<int>(rgb.blue) << ")";
	}

	void RenderColor(std::ostream& os, Rgba rgba) {
		os << "rgba(" << static_cast<int>(rgba.red) << ","
			<< static_cast<int>(rgba.green) << ","
			<< static_cast<int>(rgba.blue) << ","
			<< rgba.opacity << ")";
	}

	void RenderColor(std::ostream& os, const Color& color) {
		std::visit([&os](const auto& value) {RenderColor(os, value); }, color);
	}

	class SvgObject {
	public: 
		virtual void Render(std::ostream& os) const = 0;
		virtual ~SvgObject() = default;
	};

	class Document : public SvgObject {
	public:
		template <typename ObjectType>
		void Add(ObjectType object) {
			objects.push_back(std::make_unique<ObjectType>(std::move(object)));
		}

		void Render(std::ostream& os) const override {
			LOG_PROFILE("Render"); //TODO найти способ оптимизации
			os << "<?xml version=\\\"1.0\\\" encoding=\\\"UTF-8\\\" ?>";
			os << "<svg xmlns=\\\"http://www.w3.org/2000/svg\\\" version=\\\"1.1\\\">"; 
			for (const auto& objectPtr : objects)
				objectPtr->Render(os);
			os << "</svg>";
		}

		void RenderNoStart(std::ostream& os) const {
			LOG_PROFILE("RenderNoStart");
			for (const auto& objectPtr : objects)
				objectPtr->Render(os);
			os << "</svg>";
		}

		void RenderNoEnd(std::ostream& os) const {
			LOG_PROFILE("RenderNoEnd");
			os << "<?xml version=\\\"1.0\\\" encoding=\\\"UTF-8\\\" ?>";
			os << "<svg xmlns=\\\"http://www.w3.org/2000/svg\\\" version=\\\"1.1\\\">"; 
			for (const auto& objectPtr : objects)
				objectPtr->Render(os);
		}


	private:
		std::vector<std::shared_ptr<SvgObject>> objects; //����� ���� unique_ptr
	};

	struct Point {
		double x, y;
		Point() : x(0), y(0) {}
		Point(double x, double y) : x(x), y(y) {}
	};

	template <typename Child>
	class ObjectProps { 
	public:

		Child& SetFillColor(const Color& color = NoneColor) {
			fillColor = color;
			return returnChild();
		}
		Child& SetStrokeColor(const Color& color = NoneColor) {
			strokeColor = color;
			return returnChild();
		}
		Child& SetStrokeWidth(double value = 1.0) {
			strokeWidth = value;
			return returnChild();
		}
		Child& SetStrokeLineCap(const std::string& value) {
			strokeLineCap = value;
			return returnChild();
		}
		Child& SetStrokeLineJoin(const std::string& value) {
			strokeLineJoin = value;
			return returnChild();
		}
		
		void RenderProperties(std::ostream& os) const {
			//LOG_PROFILE("RenderProperties");
			os << "fill=\\\"";
			RenderColor(os, fillColor);
			os << "\\\" ";
			os << "stroke=\\\"";
			RenderColor(os, strokeColor);
			os << "\\\" ";
			os << "stroke-width=\\\"" << strokeWidth << "\\\" ";
			if (strokeLineCap != std::nullopt)
				os << "stroke-linecap=\\\"" << strokeLineCap.value() << "\\\" ";
			if (strokeLineJoin != std::nullopt)
				os << "stroke-linejoin=\\\"" << strokeLineJoin.value() << "\\\" ";
		}

	protected:
		Color fillColor;
		Color strokeColor;
		double strokeWidth = 1;

		std::optional<std::string> strokeLineCap;
		std::optional<std::string> strokeLineJoin;

		Child& returnChild() {
			return static_cast<Child&>(*this);
		}

	};

	
	class Circle : public SvgObject, public ObjectProps<Circle> {
	public:

		void Render(std::ostream& os) const override {
			//LOG_PROFILE("Render::Circle");
			os << "<circle ";
			os << "cx=\\\"" << center.x << "\\\" ";
			os << "cy=\\\"" << center.y << "\\\" ";
			os << "r=\\\"" << radius << "\\\" ";
			RenderProperties(os);
			os << "/>";
		}

		Circle& SetCenter(const Point& point) {
			center = point;
			return *this;
		}

		Circle& SetRadius(double value) {
			radius = value;
			return *this;
		}

	protected:
		Point center;
		double radius = 1.0;
	};


	
	class Polyline : public SvgObject, public ObjectProps<Polyline> {
	public:
		void Render(std::ostream& os) const override {
			//LOG_PROFILE("Render::Polyline");
			os << "<polyline ";
			os << "points=\\\"";
			for (const auto& p : points)
				os << p.x << "," << p.y << " ";
			os << "\\\" ";
			RenderProperties(os);
			os << "/>";
		}

		Polyline& AddPoint(const Point& point) {
			points.push_back(point);
			return *this;
		}

	protected:
		std::vector<Point> points;
	};




	class Text : public SvgObject, public ObjectProps<Text> {
	public:

		void Render(std::ostream& os) const override {
			//LOG_PROFILE("Render::Text");
			os << "<text ";
			os << "x=\\\"" << point.x << "\\\" ";
			os << "y=\\\"" << point.y << "\\\" ";
			os << "dx=\\\"" << offset.x << "\\\" ";
			os << "dy=\\\"" << offset.y << "\\\" ";
			os << "font-size=\\\"" << fontSize << "\\\" ";
			if (fontFamily)
				os << "font-family=\\\"" << fontFamily.value() << "\\\" ";
			if (fontWeight)
				os << "font-weight=\\\"" << fontWeight.value() << "\\\" ";
			RenderProperties(os);
			os << ">" << text << "</text>";
		}

		Text& SetPoint(Point point) {
			this->point = point;
			return *this;
		}
		Text& SetOffset(Point offset) {
			this->offset = offset;
			return *this;
		}
		Text& SetFontSize(uint32_t size) {
			fontSize = size;
			return *this;
		}
		Text& SetFontFamily(const std::string& value) {
			fontFamily = value;
			return *this;
		}
		Text& SetData(const std::string& value) {
			text = value;
			return *this;
		}
		Text& SetFontWeight(const std::string& value) {
			fontWeight = value;
			return *this;
		}

	protected:
		Point point;
		Point offset;
		uint32_t fontSize = 1;
		std::optional<std::string> fontFamily;
		std::string text;
		std::optional<std::string> fontWeight;
	};


	class Rectangle : public SvgObject, public ObjectProps<Text> {
	public:
		void Render(std::ostream& os) const override {
			//LOG_PROFILE("Render::Rectangle");
			os << "<rect ";
			os << "x=\\\"" << position.x << "\\\" ";
			os << "y=\\\"" << position.y << "\\\" ";
			os << "width=\\\"" << width << "\\\" ";
			os << "height=\\\"" << height << "\\\" ";
			RenderProperties(os);
			os << "/>";
		}

		Rectangle& SetPosition(Point pos) {
			position = pos;
			return *this;
		}

		Rectangle& SetWidth(double w) {
			width = w;
			return *this;
		}

		Rectangle& SetHeight(double h) {
			height = h;
			return *this;
		}

	protected:
		Point position;
		double width;
		double height;

	};

}