#include <stdlib.h>
#include <nanogui/formhelper.h>
#include <nanogui/opengl.h>
#include <nanogui/glutil.h>
#include <nanogui/screen.h>
#include <nanogui/window.h>
#include <nanogui/layout.h>
#include <nanogui/label.h>
#include <nanogui/checkbox.h>
#include <nanogui/button.h>
#include <nanogui/colorwheel.h>
#include "AngleEditor.h"
#include <iostream>
#include <string>

#include "trinterp.hpp"

using namespace std::complex_literals;

namespace fourtd
{
	template<> inline complex_double fourier::make_complex<const Eigen::Vector2d&> [[nodiscard]](const Eigen::Vector2d &c)
	{
		return { c.x(),c.y() };
	}

	template<> inline Eigen::Vector2d fourier::make_value<Eigen::Vector2d> [[nodiscard]] (const complex_double& z)
	{
		return { z.real(), z.imag() };
	}
}

using namespace fourtd;

namespace
{
	inline const std::deque<Eigen::Vector2d> pi_symbol =
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

	nanogui::Vector2i  PiBoundBox()
	{
		return std::accumulate(
			pi_symbol.cbegin(),
			pi_symbol.cend(),
			nanogui::Vector2i(0, 0),
			[](const nanogui::Vector2i& mm, const Eigen::Vector2d& p)->nanogui::Vector2i
			{
				return { std::max(mm.x(), static_cast<int>(p.x())), std::max(mm.y(), static_cast<int>(p.y()))};
			}
		);
	}
}


class CanvasWidget : public nanogui::Widget
{
public:


	CanvasWidget(nanogui::Widget* parent) :
		nanogui::Widget(parent),
		f(pts.cbegin(), pts.cend())
	{
		updateCoeff();
	}

	void clear()
	{
		pts.clear();
		cur_point = pts.end();
		updateCoeff();
	}

	void setPi()
	{
		pts = pi_symbol;
		cur_point = pts.begin();
		updateCoeff();
	}

	void setPos(double value)
	{
		pos = value / (2 * fourtd::pi) * pts.size();
	}

	void setShowCircles(bool value)
	{
		show_circles = value;
	}

	void setShowBrokenLine(bool value)
	{
		show_broken_line = value;
	}

	void setShowTangent(bool value)
	{
		show_tangent = value;
	}

	void setShowNormal(bool value)
	{
		show_normal = value;
	}

	void setIsClose(bool value)
	{
		interp.clear();
		is_close = value;
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

		if (parent())
		{
			auto square = std::async(std::launch::async, [this] { return f.square(); });
			auto length = std::async(std::launch::async, [this] { return f.length(0, 2 * fourtd::pi); });
			static_cast<nanogui::Window*>(parent())->setTitle(
				std::string("fourier - S = ") +
				std::to_string(square.get()) + 
				std::string(" , Len = ") +
				std::to_string(length.get()));
		}

		radii = std::move(rad_future.get());
	}

	void draw(NVGcontext* ctx) override
	{
		onRenderB2D(ctx);
	}

	auto find_point(const Eigen::Vector2d& test_pt)
	{
		return std::find_if(std::execution::par_unseq, pts.begin(), pts.end(),
			[&test_pt](auto& pt)
			{
				return std::hypot(pt.x() - test_pt.x(), pt.y() - test_pt.y()) < sel_tolerance;
			}
		);
	}

	/*void mouseDoubleClickEvent(QMouseEvent* event) override
	{
		const auto test = find_point(BLPoint(event->pos().x(), event->pos().y()));

		if (test != pts.end())
		{
			pts.erase(test);
			cur_point = pts.end();
			updateCoeff();
			updateCanvas();
		}
	}*/

	bool mouseButtonEvent(const Eigen::Vector2i& p, int button, bool down, int modifiers) override
	{
		Widget::mouseButtonEvent(p, button, down, modifiers);
		
		if (!down)
		{
			cur_point = pts.end();
			return false;
		}
		
		const Eigen::Vector2d pt(p.x(), p.y());

		cur_point = find_point(pt);
		if (cur_point == pts.end())
		{
			const auto inter = f.lengthToPoint({pt.x(), pt.y()});
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
		return true;
	}

	
	bool mouseMotionEvent(const Eigen::Vector2i& p, const Eigen::Vector2i& rel, int button, int modifiers)  override
	{

		if (cur_point != pts.end())
		{
			const Eigen::Vector2d pt(p.x(), p.y());
			*cur_point = pt;
			updateCoeff();
			return true;
		}
		return false;
	}

	nanogui::Vector2i preferredSize(NVGcontext* ctx) const override
	{
		return PiBoundBox();
	}

	void onRenderB2D(NVGcontext* ctx)
	{
		Widget::draw(ctx);
		
		if (pts.size() > 1)
		{
			if (interp.empty())
				f.values<Eigen::Vector2d>(std::back_inserter(interp), 0, pts.size() -1.0 + static_cast<int>(is_close), 0.01);

			nvgStrokeWidth(ctx, 4);
			nvgStrokeColor(ctx, nvgRGB(0xFF, 0xFF, 0x00));
			nvgPolygone(ctx, interp, is_close);
			
			if (show_circles || show_tangent || show_normal || show_broken_line)
			{
				std::vector<Eigen::Vector2d> lines;
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
				nvgBeginPath(ctx);
				complex_double sum= f.firstCoeff();
				for (const auto &vec : vector_list)
				{
					if(show_circles)
						nvgCircle(ctx, sum.real(), sum.imag(), std::abs(vec));
					sum += vec;
					lines.emplace_back(sum.real(), sum.imag());
				}

				if (show_circles)
				{
					nvgStrokeWidth(ctx, 2);
					nvgStrokeColor(ctx, nvgRGBA(0x00, 0xB3, 0xB3, 0xF0));
					nvgStroke(ctx);
				}

				nvgStrokeWidth(ctx, 1);

				if (show_broken_line)
				{
					nvgStrokeWidth(ctx, 2);
					nvgLineJoin(ctx, NVG_ROUND);
					nvgStrokeColor(ctx, nvgRGB(0xFF, 0xFF, 0xFF));
					nvgPolygone(ctx, lines, false);
					nvgBeginPath(ctx);
					for (const auto& centre : lines)
					{
						nvgCircle(ctx, centre.x(), centre.y(), 2);
					}
					nvgFillColor(ctx, nvgRGB(0xFF, 0xFF, 0xFF));
					nvgFill(ctx);
				}

				nvgStrokeWidth(ctx, 1);
				nvgStrokeColor(ctx, nvgRGB(0xFF, 0x00, 0x00));

				if (show_tangent)
				{
					nvgBeginPath(ctx);
					nvgMoveTo(ctx, cur_pt.real() - der.real(), cur_pt.imag() - der.imag());
					nvgLineTo(ctx, cur_pt.real() + der.real(), cur_pt.imag() + der.imag());
					nvgStroke(ctx);
				}
				if (show_normal)
				{
					nvgBeginPath(ctx);
					nvgMoveTo(ctx, cur_pt.real() - der.imag(), cur_pt.imag() + der.real());
					nvgLineTo(ctx, cur_pt.real() + der.imag(), cur_pt.imag() - der.real());
					nvgStroke(ctx);
				}

				nvgStrokeWidth(ctx, 3);
				nvgBeginPath(ctx);
				nvgCircle(ctx, lines.back().x(), lines.back().y(), 4);
				nvgStroke(ctx);

			}
		}

		nvgBeginPath(ctx);

		for (const auto& pt : pts)
		{
			nvgCircle(ctx,pt.x(), pt.y(), 3);
		}

		nvgFillColor(ctx, nanogui::Color(255, 255, 255, 255));
		nvgFill(ctx);
	}


private:
	std::deque<Eigen::Vector2d> pts = pi_symbol;
	fourier f;
	std::deque<Eigen::Vector2d>::iterator cur_point = pts.end();
	std::vector<Eigen::Vector2d> interp;
	std::vector<std::pair<complex_double, complex_double>> radii;
	bool is_close = true;
	bool show_circles{};
	bool show_broken_line{};
	bool show_tangent{};
	bool show_normal{};
	double pos{};
};


class FourierApplication : public nanogui::Screen
{
	public:
		FourierApplication():
			nanogui::Screen(nanogui::Vector2i(800, 600), "fourier"),
			m_animation(false),
			m_delay(50)
		{
			auto* screen = this;
			auto* window = new nanogui::Window(screen, "fourier");
			window->setSize({ 800, 600 });
			window->setPosition({ 0,0 });
			auto* layout = new nanogui::GridLayout(nanogui::Orientation::Horizontal);
			layout->setColAlignment({ nanogui::Alignment::Minimum, nanogui::Alignment::Fill });
			layout->setRowAlignment({ nanogui::Alignment::Fill});
			window->setLayout(layout);
			auto* tool_widget = new nanogui::Widget(window);
			auto* v = new nanogui::BoxLayout(nanogui::Orientation::Vertical, nanogui::Alignment::Fill);
			tool_widget->setLayout(v);

			auto* canvas = new CanvasWidget(window);

			auto* set_pi = new nanogui::Button(tool_widget, "Pi");
			set_pi->setCallback([canvas]() { canvas->setPi(); });

			auto* clear = new nanogui::Button(tool_widget, "Clear");
			clear->setCallback([canvas]() { canvas->clear(); });

			auto* is_closed = new nanogui::CheckBox(tool_widget, "Closed");
			is_closed->setChecked(true);
			is_closed->setCallback([canvas](bool value) { canvas->setIsClose(value); });

			auto* show_circles = new nanogui::CheckBox(tool_widget, "Circles");
			show_circles->setCallback([canvas](bool value) { canvas->setShowCircles(value); });

			auto* show_broken_line = new nanogui::CheckBox(tool_widget, "Zigzag lines");
			show_broken_line->setCallback([canvas](bool value) { canvas->setShowBrokenLine(value); });

			auto* show_tangent = new nanogui::CheckBox(tool_widget, "Tangent");
			show_tangent->setCallback([canvas](bool value) { canvas->setShowTangent(value); });

			auto* show_normal = new nanogui::CheckBox(tool_widget, "Normal");
			show_normal->setCallback([canvas](bool value) { canvas->setShowNormal(value); });

			auto* anima = new nanogui::CheckBox(tool_widget, "Animation");
			anima->setCallback([this](bool value) { setAnimation(value); });

			angle = new  nanogui::AngleEditor(tool_widget);
			angle->setCallback([canvas](int value) { canvas->setPos(2 * fourtd::pi * value / 360.0); });

			//canvas->setSize({ 400, 300 });
			screen->setVisible(true);
			screen->performLayout();
		}

		void draw(NVGcontext* ctx) override
		{
			if (m_animation)
			{
				const auto t_now = std::chrono::high_resolution_clock::now();
				const auto elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(t_now - t_start);
				if (m_delay <= elapsed)
				{
					angle->setAngle(angle->angle() + 1.0);
					t_start = t_now;
				}
			}
			Screen::draw(ctx);
		}

		void setAnimation(bool anim)
		{
			if (anim == m_animation) return;
			m_animation = anim;
			if (m_animation)
			{
				t_start = std::chrono::high_resolution_clock::now();
			}

		}
 
	private:
		nanogui::AngleEditor* angle;
		bool m_animation;
		const std::chrono::milliseconds m_delay;
		std::chrono::steady_clock::time_point t_start;
	

};

int main(int argc, char* argv[])
{
	
	nanogui::init();
	nanogui::ref<FourierApplication> app = new FourierApplication;
	nanogui::mainloop();
	nanogui::shutdown();
	return 0;
}
