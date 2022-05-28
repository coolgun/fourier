#include <stdlib.h>
#include <FL/Fl.H>
#include <FL/Fl_Double_Window.H>
#include <FL/Fl_Box.H>
#include <FL/Fl_Button.H>
#include <FL/fl_draw.H>
#include <FL/Fl_Check_Button.H>
#include <FL/Fl_Dial.H>

#include "trinterp.hpp"

using namespace std::complex_literals;

namespace fourtd
{
	template<> inline complex_double fourier::make_complex<const complex_double&> [[nodiscard]] (const complex_double& c)
	{
		return c;
	}

	template<> inline complex_double fourier::make_value<complex_double> [[nodiscard]] (const complex_double& z)
	{
		return z;
	}
}

using namespace fourtd;

Fl_Double_Window* win{};

namespace
{
	inline const std::deque<complex_double> pi_symbol =
	{
		{408.0,130.0}
		,{503.0,132.0}
		,{503.0,173.0}
		,{449.0,180.0}
		,{395.0,258.0}
		,{404.0,386.0}
		,{418.0,442.0}
		,{403.0,481.0}
		,{370.0,483.0}
		,{342.0,444.0}
		,{340.0,331.0}
		,{351.0,218.0}
		,{320.0,182.0}
		,{293.0,186.0}
		,{268.0,299.0}
		,{214.0,484.0}
		,{140.0,467.0}
		,{175.0,399.0}
		,{227.0,277.0}
		,{252.0,179.0}
		,{206.0,175.0}
		,{167.0,190.0}
		,{126.0,213.0}
		,{105.0,193.0}
		,{144.0,147.0}
		,{188.0,131.0}
		,{243.0,131.0}
	};

	constexpr double sel_tolerance = 7.0;
}

class FLCanvasWidget : public Fl_Widget
{

public:

	FLCanvasWidget(int x, int y, int cx, int cy) :
		Fl_Widget(x, y, cx, cy, ""),
		f(pts.cbegin(), pts.cend())
	{
		updateCoeff();
		color(FL_BLACK);
	}

	void clear()
	{
		pts.clear();
		cur_point = pts.end();
		updateCoeff();
		redraw();
	}

	void setPi()
	{
		pts = pi_symbol;
		cur_point = pts.end();
		updateCoeff();
		redraw();
	}

	void setPos(double value)
	{
		pos = value / (2 * fourtd::pi) * pts.size();
		redraw();
	}

	void setShowCircles(bool value)
	{
		show_circles = value;
		redraw();
	}

	void setShowBrokenLine(bool value)
	{
		show_broken_line = value;
		redraw();
	}

	void setShowTangent(bool value)
	{
		show_tangent = value;
		redraw();
	}

	void setShowNormal(bool value)
	{
		show_normal = value;
		redraw();
	}


	void setIsClose(bool value)
	{
		interp.clear();
		is_close = value;
		redraw();
	}

private:

	void updateCoeff()
	{
		interp.clear();
		f.calcul_coeff(pts.cbegin(), pts.cend());

		auto rad_future = std::async
		(	
			std::launch::async,
			[this]()
			{
				const auto& coeff = f.coeffs();
				decltype(radii) rad;
				rad.reserve(coeff.size());
				for (const auto& c : coeff)
				{
					// A*cos(w)+B*sin(w) ->  Z1*e^iw+Z2*^-iw
					rad.emplace_back
					(
						std::piecewise_construct,
						std::forward_as_tuple((c.first.real() + c.second.imag()) / 2.0, (c.first.imag() - c.second.real()) / 2.0),
						std::forward_as_tuple((c.first.real() - c.second.imag()) / 2.0, (c.first.imag() + c.second.real()) / 2.0)
					);
				}
				return rad;
			}
		);
		auto square = std::async(std::launch::async, [this] { return f.square(); });
		auto length = std::async(std::launch::async, [this] { return f.length(0, 2 * fourtd::pi); });
		const auto title = std::string("fourier - S = ") +
			std::to_string(square.get()) +
			std::string(" , Len = ") +
			std::to_string(length.get());
		win->label(title.c_str());
		radii = std::move(rad_future.get());
	}

	auto find_point(const complex_double & test_pt)
	{
		return std::find_if(std::execution::par_unseq, pts.begin(), pts.end(),
			[&test_pt](auto& pt)
			{
				return std::abs(pt - test_pt) < sel_tolerance;
			}
		);
	}

	void mouseDoubleClickEvent(const complex_double &pos)
	{
		const auto test = find_point(pos);

		if (test != pts.end())
		{
			pts.erase(test);
			cur_point = pts.end();
			updateCoeff();
			redraw();
		}
	}

	void mousePressEvent(const complex_double& pt)
	{
		cur_point = find_point(pt);
		if (cur_point == pts.end())
		{
			const auto inter = f.lengthToPoint(pt);
			if (std::get<2>(inter) < 5)
			{
				const auto index = static_cast<int>(std::ceil(f.angleToIndex(std::get<0>(inter))));
				pts.insert(pts.begin() + index, pt);
				cur_point = std::next(pts.begin(), index);
			}
			else
			{
				pts.push_back(pt);
				cur_point = std::prev(pts.end());
			}
		}

		updateCoeff();
		redraw();
	}

	void mouseReleaseEvent() 
	{
		cur_point = pts.end();
	}

	void mouseMoveEvent(const complex_double& pt) 
	{
		if (cur_point != pts.end())
		{
			*cur_point = pt;
			updateCoeff();
			redraw();
		}
	}

	int handle(int event) override
	{
		int result = Fl_Widget::handle(event);
		switch (event) 
		{
			case FL_PUSH:
			{
				result = 0;
				if(Fl::event_clicks() > 0)
				{
					mouseDoubleClickEvent(complex_double(Fl::event_x() - x(), Fl::event_y() - y()));
					return 1;
				}
				mousePressEvent(complex_double(Fl::event_x() - x(), Fl::event_y() - y()));
			}; 
			case FL_DRAG:
			{
				result = 1;
				mouseMoveEvent(complex_double(Fl::event_x() - x(), Fl::event_y() - y()));
			}; break;
		
			case FL_RELEASE:
			{
				result = 1;
				mouseReleaseEvent();
			}; break;

			default:
				break;
		}
		return result;
	}

	void draw() override
	{
		fl_rectf(x(), y(), w(), h(), 0);
		fl_push_clip(x(), y(), w(), h());
		fl_push_matrix();
		fl_translate(x(), y());
		if (pts.size() > 1)
		{
			if (interp.empty())
				f.values<complex_double>(std::back_inserter(interp), 0, pts.size() -1.0 + static_cast<int>(is_close), 0.01);

			fl_color(250,255,0);
			fl_line_style(FL_SOLID, 4);
			
			is_close ? fl_begin_loop() : fl_begin_line();
			for (const auto& pt : interp)
			{
				fl_vertex(pt.real(), pt.imag());
			}
			is_close ? fl_end_line() : fl_end_line();

			if (show_circles || show_tangent || show_normal || show_broken_line)
			{
				std::vector<complex_double> lines;
				std::vector<std::pair<complex_double, double>> big_path;
				complex_double der;

				std::list<complex_double> vector_list;

				const auto cur_pt = f.nativ_value(f.indexToAngle(pos),
					[&lines, &der, &vector_list, r_it = radii.cbegin()](const auto& gsum, const auto& coeff, const complex_double& sincos, size_t k)mutable
				{

					if (lines.empty())
						lines.emplace_back(gsum.real(), gsum.imag());

					auto sum = gsum;

					const auto z1 = r_it->first * sincos;
					const auto z2 = r_it->second * std::conj(sincos);

					vector_list.push_front(z2);
					vector_list.push_back(z1);

					der = fourier::derivative_step(der, coeff, sincos, k);
					++r_it;
				}
				);

				complex_double sum= f.firstCoeff();
				for (const auto &vec : vector_list)
				{
					big_path.emplace_back(sum, std::abs(vec));
					sum += vec;
					lines.emplace_back(sum.real(), sum.imag());
				}

				if (show_circles)
				{
					fl_line_style(FL_SOLID, 2);
					fl_color(0x00, 0xB3, 0xB3);
					for (const auto &circle: big_path)
					{
						fl_begin_loop();
						fl_circle(circle.first.real(), circle.first.imag(), circle.second);
						fl_end_loop();
					}
				}
			
				if (show_broken_line)
				{
					fl_line_style(FL_SOLID, 2);
					fl_color(255, 255, 255);

					fl_begin_line();
					for (const auto& pt : lines)
					{
						fl_vertex(pt.real(), pt.imag());
					}
					fl_end_line();

					fl_begin_polygon();

					for (const auto& pt : lines)
					{
						fl_circle(std::round(pt.real()), std::round(pt.imag()), 2);
					}

					fl_end_polygon();
				}

				fl_color(255,0,0 );
				fl_line_style(FL_SOLID, 1);

				if (show_tangent)
					fl_line(
						cur_pt.real() - der.real() + x(),
						cur_pt.imag() - der.imag() + y(),
						cur_pt.real() + der.real() + x(),
						cur_pt.imag() + der.imag() + y());

				if (show_normal)
					fl_line(
						cur_pt.real() - der.imag() + x(),
						cur_pt.imag() + der.real() + y(),
						cur_pt.real() + der.imag() + x(),
						cur_pt.imag() - der.real() + y());
				
				fl_line_style(FL_SOLID, 3);
				fl_begin_loop();
				fl_circle(lines.back().real(), lines.back().imag(), 4);
				fl_end_loop();
			}
		}

		fl_color(FL_WHITE);
		fl_begin_polygon();
		
		for (const auto& pt : pts)
		{
			fl_circle(std::round(pt.real()), std::round(pt.imag()), 4);
		}

		fl_end_polygon();
		fl_pop_matrix();
		fl_pop_clip();
	}


private:
	std::deque<complex_double> pts = pi_symbol;
	fourier f;
	std::deque<complex_double>::iterator cur_point = pts.end();
	std::vector<complex_double> interp;
	std::vector<std::pair<complex_double, complex_double>> radii;
	bool is_close = true;
	bool show_circles{};
	bool show_broken_line{};
	bool show_tangent{};
	bool show_normal{};
	double pos{};
};

static constexpr double delta = 1/100.0;

FLCanvasWidget* canvas{};
Fl_Dial* positin{};

void setPos(Fl_Widget* w, void* u)
{
	auto* positin = static_cast<Fl_Dial*>(w);
	canvas->setPos(2 * fourtd::pi * positin->value() / 360);
}

static void tick(void* v)
{
	//positin->value(positin->value() > 1 ? 0: positin->value(positin->value() + positin->step()));
	positin->value(positin->value() + 1);
	setPos(positin, nullptr);
	Fl::repeat_timeout(delta, tick, v);
}


int main(int argc, char* argv[])
{
	static const int form_w = 800;
	static const int form_h = 600;
	static const int tool_panel_width = 100;
	
	win = new Fl_Double_Window(form_w, form_h, "fourier");
	win->begin();
		Fl_Group* h = new Fl_Group(0, 0, form_w, form_h);
		h->begin();
			Fl_Group* v = new Fl_Group(0, 0, tool_panel_width, form_h);
			v->begin();
				auto* clear = new Fl_Button(0, 0, tool_panel_width, 20, "Clear");
				clear->callback([](Fl_Widget* w, void* u) {canvas->clear();});

				auto* pi = new Fl_Button(0, 20, tool_panel_width, 20, ("Pi"));
				pi->callback([](Fl_Widget* w, void* u) {canvas->setPi();});

				auto* is_closed = new Fl_Check_Button(0, 40, tool_panel_width, 20, ("Closed"));
				is_closed->value(true);
				is_closed->callback([](Fl_Widget* w, void* u) {canvas->setIsClose(static_cast<Fl_Check_Button*>(w)->value());});

				auto* show_circles = new Fl_Check_Button(0, 60, tool_panel_width, 20, ("Circles"));
				show_circles->callback([](Fl_Widget* w, void* u) {canvas->setShowCircles(static_cast<Fl_Check_Button*>(w)->value());});

				auto* show_broken_line = new Fl_Check_Button(0, 80, tool_panel_width, 20, ("Zigzag lines"));
				show_broken_line->callback([](Fl_Widget* w, void* u) {canvas->setShowBrokenLine(static_cast<Fl_Check_Button*>(w)->value());});

				auto* show_tangent = new Fl_Check_Button(0, 100, tool_panel_width, 20, ("Tangent"));
				show_tangent->callback([](Fl_Widget* w, void* u) {canvas->setShowTangent(static_cast<Fl_Check_Button*>(w)->value());});

				auto* show_normal = new Fl_Check_Button(0, 120, tool_panel_width, 20, ("Normal"));
				show_normal->callback([](Fl_Widget* w, void* u) {canvas->setShowNormal(static_cast<Fl_Check_Button*>(w)->value());});

				auto* anima = new Fl_Check_Button(0, 140, tool_panel_width, 20, ("Animation"));
				anima->callback([](Fl_Widget* w, void* u) 
					{
						const auto anim = static_cast<Fl_Check_Button*>(w)->value();
						Fl::remove_timeout(tick, nullptr);
						if (anim)
						{
							Fl::add_timeout(delta, tick, 0);
						}
					});
				positin = new Fl_Dial(0, 160, tool_panel_width/2, tool_panel_width/2, ("Angle"));
				positin->angles(0, 360);
				positin->range(0, 360);
				positin->callback(setPos);
				auto *spacer = new Fl_Box(0, 160 + tool_panel_width / 2, tool_panel_width, tool_panel_width - 160 - tool_panel_width / 2);   // "invisible" box "R"
				spacer->hide();                       
				v->resizable(spacer);
			v->end();
		
			canvas = new FLCanvasWidget(tool_panel_width,0, form_w - tool_panel_width, form_h);
			h->resizable(canvas);
		h->end();
	win->resizable(h);
	win->end();
	win->show(argc, argv);
	return Fl::run();
}