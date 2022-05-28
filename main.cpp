#include <stdlib.h>
#include <nana/gui.hpp>
#include <nana/gui/widgets/button.hpp>
#include <nana/gui/widgets/label.hpp>
#include <nana/gui/widgets/checkbox.hpp>
#include <nana/gui/widgets/panel.hpp>
#include <nana/gui/widgets/slider.hpp>
#include <nana/gui/drawing.hpp>
#include <nana/gui/place.hpp>
#include <nana/gui/timer.hpp>

#include <nana/paint/pixel_buffer.hpp>
#include <blend2d.h>

#include "trinterp.hpp"

using namespace std::complex_literals;

namespace fourtd
{
	template<> inline complex_double fourier::make_complex<const BLPoint&> [[nodiscard]] (const BLPoint& c)
	{
		return { c.x,c.y };
	}

	template<> inline BLPoint fourier::make_value<BLPoint> [[nodiscard]] (const complex_double& z)
	{
		return { z.real(), z.imag() };
	}
}

using namespace fourtd;

namespace
{
	inline const std::deque<BLPoint> pi_symbol =
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
	constexpr double sel_tolerance = 7.0 * 7.0;
}

class CanvasPanel : public nana::panel<true>
{

public:

	CanvasPanel(nana::form& parent_form) :
		nana::panel<true>(parent_form),
		parent_form(parent_form),
		f(pts.cbegin(), pts.cend()),
		drawer(*this),
		substr(size().width, size().height)
	{
		createInfo.threadCount = std::thread::hardware_concurrency();
		updateCoeff();
		events().mouse_move([this](const nana::arg_mouse& am)
			{
				mouseMoveEvent(am.pos);
			}
		);
		
		events().mouse_down([this](const nana::arg_mouse& am)
			{
				mousePressEvent(am.pos);
			}
		);

		events().mouse_up([this](const nana::arg_mouse&)
			{
				mouseReleaseEvent();
			}
		);

		events().dbl_click([this](const nana::arg_mouse& am)
			{
				mouseDoubleClickEvent(am.pos);
			}
		);

		events().resized([this](const nana::arg_resized &ar)
			{
				resizeCanvas({ar.width, ar.height});
			});
		drawer.draw([this](nana::paint::graphics& graph)
			{
				if (dirty)
					renderCanvas();
				substr.paste(graph.handle(), {0, 0});
			});
	}

	void clear()
	{
		pts.clear();
		cur_point = pts.end();
		updateCoeff();
		updateCanvas();
	}

	void setPi()
	{
		pts = pi_symbol;
		cur_point = pts.end();
		updateCoeff();
		updateCanvas();
	}

	void setPos(double value)
	{
		pos = value / (2 * fourtd::pi) * pts.size();
		updateCanvas(true);
	}

	void setShowCircles(bool value)
	{
		show_circles = value;
		updateCanvas();
	}

	void setShowBrokenLine(bool value)
	{
		show_broken_line = value;
		updateCanvas();
	}

	void setShowTangent(bool value)
	{
		show_tangent = value;
		updateCanvas();
	}

	void setShowNormal(bool value)
	{
		show_normal = value;
		updateCanvas();
	}


	void setIsClose(bool value)
	{
		interp.clear();
		is_close = value;
		updateCanvas();
	}

private:
	void renderCanvas()
	{
		BLContext ctx(blImage, createInfo);
		onRenderB2D(ctx);
		ctx.end();
	}

	void updateCanvas(bool force = false)
	{
		if (force)
			renderCanvas();
		else
			dirty = true;
		drawer.update();
	}

	void resizeCanvas(const nana::size &sz)
	{
		substr.open(sz.width, sz.height);
		blImage.createFromData(
			sz.width,
			sz.height,
			BL_FORMAT_PRGB32,
			static_cast<void*>(substr.raw_ptr(0)),
			substr.bytes_per_line());
		updateCanvas(false);
	}

	bool dirty = {};


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
		parent_form.caption(
			std::string("fourier - S = ") +
			std::to_string(square.get()) +
			std::string(" , Len = ") +
			std::to_string(length.get()));
		radii = std::move(rad_future.get());
	}

	
	auto find_point(const BLPoint& test_pt)
	{
		return std::find_if(std::execution::par_unseq, pts.begin(), pts.end(),
			[&test_pt](auto& pt)
			{
				const auto d = pt - test_pt;
				return (d.x * d.x + d.y * d.y) < sel_tolerance;
			}
		);
	}

	void mouseDoubleClickEvent(const nana::point& pos)
	{
		const auto test = find_point(BLPoint(pos.x, pos.y));

		if (test != pts.end())
		{
			pts.erase(test);
			cur_point = pts.end();
			updateCoeff();
			updateCanvas();
		}
	}

	void mousePressEvent(const nana::point &pos)
	{
		const BLPoint pt(pos.x, pos.y);
		cur_point = find_point(pt);
		if (cur_point == pts.end())
		{
			const auto inter = f.lengthToPoint({ pt.x,pt.y });
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
		updateCanvas();
	}

	void mouseReleaseEvent() 
	{
		cur_point = pts.end();
	}

	void mouseMoveEvent(const nana::point& pos) 
	{
		const BLPoint pt(pos.x, pos.y);
		if (cur_point != pts.end())
		{
			*cur_point = pt;
			updateCoeff();
			updateCanvas();
		}
	}

	void onRenderB2D(BLContext& ctx)
	{

		ctx.setFillStyle(BLRgba32(0xFF000000u));
		ctx.fillAll();

		if (pts.size() > 1)
		{
			if (interp.empty())
				f.values<BLPoint>(std::back_inserter(interp), 0, pts.size() -1.0 + static_cast<int>(is_close), 0.01);

			ctx.setStrokeStyle(BLRgba32(0xFFFFFF00u));

			ctx.setStrokeWidth(4);
			if (is_close)
				ctx.strokePolygon(&interp[0], interp.size());
			else
				ctx.strokePolyline(&interp[0], interp.size());

			if (show_circles || show_tangent || show_normal || show_broken_line)
			{
				std::vector<BLPoint> lines;
				BLPath big_path;
				BLPath small_path;
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
					big_path.addCircle(BLCircle(sum.real(), sum.imag(), std::abs(vec)));
					sum += vec;
					lines.emplace_back(sum.real(), sum.imag());
					small_path.addCircle(BLCircle(sum.real(), sum.imag(), 2));
				}

				if (show_circles)
				{
					ctx.setStrokeWidth(2);
					ctx.setStrokeStyle(BLRgba32(0xF000B3B3u));
					ctx.strokePath(big_path);
				}

				ctx.setStrokeWidth(1);

				if (show_broken_line)
				{
					ctx.setStrokeWidth(2);
					ctx.setStrokeStyle(BLRgba32(0xFFFFFFFFu));
					ctx.setFillStyle(BLRgba32(0xFFFFFFFFu));
					ctx.strokePolyline(&lines[0], lines.size());
					ctx.fillPath(small_path);
				}

				ctx.setStrokeWidth(1);
				ctx.setStrokeStyle(BLRgba32(0xFFFF0000u));

				if (show_tangent)
					ctx.strokeLine(cur_pt.real() - der.real(),cur_pt.imag() - der.imag(),cur_pt.real() + der.real(),cur_pt.imag() + der.imag());

				if (show_normal)
					ctx.strokeLine(cur_pt.real() - der.imag(),cur_pt.imag() + der.real(), cur_pt.real() + der.imag(),cur_pt.imag() - der.real());
				ctx.setStrokeWidth(3);
				ctx.strokeCircle(lines.back().x, lines.back().y, 4);
			}
		}

		BLPath path;

		for (const auto& pt : pts)
		{
			path.addCircle(BLCircle(pt.x, pt.y, 3));
		}
		ctx.setFillStyle(BLRgba32(0xFFFFFFFFu));
		ctx.fillPath(path);
	}


private:
	BLImage blImage;
	nana::paint::pixel_buffer substr;
	std::deque<BLPoint> pts = pi_symbol;
	fourier f;
	std::deque<BLPoint>::iterator cur_point = pts.end();
	std::vector<BLPoint> interp;
	std::vector<std::pair<complex_double, complex_double>> radii;
	bool is_close = true;
	bool show_circles{};
	bool show_broken_line{};
	bool show_tangent{};
	bool show_normal{};
	BLContextCreateInfo createInfo{};
	double pos{};
	nana::drawing drawer;
	nana::form &parent_form;
};

int main(int argc, char* argv[])
{
	using namespace nana;
	form fm;
	CanvasPanel canvas(fm);
	panel<true> tool_widget(fm);
	place tool_place(tool_widget);
	tool_place.div("tool_place vert gap=2 margin=2 arrange=[20, repeated]");
	button clear(tool_widget, "Clear");
	clear.events().click([&canvas](const arg_click&) {canvas.clear();});
	button pi(tool_widget, "Pi");
	pi.events().click([&canvas](const arg_click&) {canvas.setPi();});

	checkbox is_closed(tool_widget, "Closed");
	is_closed.check(true);
	is_closed.events().checked([&canvas](const arg_checkbox &ac ) {canvas.setIsClose(ac.widget->checked());});

	checkbox show_circles(tool_widget, "Circles");
	show_circles.events().checked([&canvas](const arg_checkbox& ac) {canvas.setShowCircles(ac.widget->checked());});

	checkbox show_broken_line(tool_widget, "Zigzag lines");
	show_broken_line.events().checked([&canvas](const arg_checkbox& ac) {canvas.setShowBrokenLine(ac.widget->checked()); });
	
	checkbox show_tangent(tool_widget, "Tangent");
	show_tangent.events().checked([&canvas](const arg_checkbox& ac) {canvas.setShowTangent(ac.widget->checked()); });

	checkbox show_normal(tool_widget, "Normal");
	show_normal.events().checked([&canvas](const arg_checkbox& ac) {canvas.setShowNormal(ac.widget->checked()); });

	nana::timer timer {std::chrono::milliseconds{50}};

	slider positin(tool_widget);
	positin.maximum(360);
	positin.events().value_changed([&positin, &canvas](const arg_slider& sl)
		{
			canvas.setPos(2 * fourtd::pi * sl.widget.value() / sl.widget.maximum()); 
		});

	timer.elapse([&positin, &canvas]
		{
			positin.value((positin.value() + 1) % 360);
			canvas.setPos(2 * fourtd::pi * positin.value() / positin.maximum());

		});

	checkbox anima(tool_widget, "Animation");
	anima.events().checked([&canvas, &timer](const arg_checkbox& ac)
		{
			if (ac.widget->checked())
				timer.start();
			else
				timer.stop();
		});
	

	tool_place.field("tool_place")
		<< clear
		<< pi
		<< is_closed
		<< show_circles
		<< show_broken_line
		<< show_tangent
		<< show_normal
		<< anima
		<< positin;
	
	place main_place(fm);
	main_place.div("<main_place arrange=[20%,variable]>");
	main_place.field("main_place") << tool_widget << canvas;
	main_place.collocate();
	fm.size({ 800, 600 });
	fm.show();
	exec();
	return 0;
}