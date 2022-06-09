#include <deque>
#include <list>
#include <CtrlLib/CtrlLib.h>
#include "trinterp.hpp"

using namespace std::complex_literals;
using namespace Upp;

#define LAYOUTFILE <fourier/main.lay>
#include <CtrlCore/lay.h>

namespace fourtd
{
	template<> inline complex_double fourier::make_complex<const Pointf&> [[nodiscard]] (const Pointf& c)
	{
		return { c.x, c.y };
	}

	template<> Pointf fourier::make_value<Pointf> [[nodiscard]] (const complex_double& z)
	{
		return { z.real(), z.imag() };
	}
}

using namespace fourtd;

namespace
{
	inline const std::deque<Pointf> pi_symbol =
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

class CanvasWidget :public Ctrl 
{
public:

	CanvasWidget() :
		f(pts.cbegin(), pts.cend())
	{
		
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
	
	void setParent(TopWindow *par)
	{
		parent = par;
		updateCoeff();
	}

private:


	void updateCanvas(bool force = false)
	{
		Refresh();
	}

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
		
		if (parent)
		{
			auto square = std::async(std::launch::async, [this] { return f.square();});
			auto length = std::async(std::launch::async, [this] { return f.length(0, 2 * fourtd::pi); });
			parent->Title(String("fourier - S = ") 
				+ FormatDouble(square.get())
				+ String(" , Len = ") 
				+ FormatDouble(length.get()));
		}
		radii = std::move(rad_future.get());
	}

	void Paint(Draw& w) override
	{
		ImageBuffer substr(GetSize());		
		BufferPainter sw(substr);
		sw.Clear(Black());
		onRenderB2D(sw);
		w.DrawImage(0, 0, substr);
	}

	auto find_point(const Pointf& test_pt)
	{
        return std::find_if(pts.begin(), pts.end(),
			[&test_pt](auto& pt)
			{
				return Length(pt - test_pt) < sel_tolerance;
			}
		);
	}
    
	void  LeftDouble(Point pos, dword keyflags) override
	{
		const auto test = find_point(pos);

		if (test != pts.end())
		{
			pts.erase(test);
			cur_point = pts.end();
			updateCoeff();
			updateCanvas();
		}
	}

	void LeftDown(Point pos, dword flags) override
	{
		cur_point = find_point(pos);
		if (cur_point == pts.end())
		{
			const auto inter = f.lengthToPoint(complex_double(pos.x, pos.y));
			if (std::get<2>(inter) < 5)
			{
				const auto index = static_cast<int>(std::ceil(f.angleToIndex(std::get<0>(inter))));
				pts.insert(pts.begin() + index, pos);
				cur_point = std::next(pts.begin(), index);
			}
			else
			{
				pts.push_back(pos);
				cur_point = std::prev(pts.end());
			}
		}
		updateCoeff();
		updateCanvas();
	}

	void LeftUp(Point pos, dword flags) override
	{
		cur_point = pts.end();
	}

	void MouseMove(Point pos, dword flags) override
	{
		if (cur_point != pts.end())
		{
			*cur_point = pos;
			updateCoeff();
			updateCanvas();
		}
	}
	
	void DrawPoly(Painter &ctx, const std::vector<Pointf> & points, bool close, int width, const Color &color , bool is_marker  = false)
	{
		if(points.size() <2 ) return;
		bool first = true;
		std::vector<double> length;
		double full_length{};
		Pointf prev{};
		for(const auto &pt: points)
		{
			if(first)
			{
				first = false;
				ctx.Move(pt);
				prev = pt;
				continue;
			}
			ctx.Line(pt);
			if(is_marker)
			{
				full_length += Length(pt - prev); 
				length.push_back(full_length);
				prev = pt;
			}
		}
		if(close)
			ctx.Close();
		
		ctx.Stroke(width, color);
		if(is_marker)
		{
			for(const auto len: length)
			{
				ctx.BeginOnPath((len - 0.1) / full_length );
				ctx.Move(-8, -3).Line(0, 0).Line(-8, 3).Close().Fill(White());
				ctx.End();
			}
		}
				
	}

	void onRenderB2D(Painter &ctx)
	{
		if (pts.size() > 1)
		{
			if (interp.empty())
				f.values<Pointf>(std::back_inserter(interp), 0, pts.size() -1.0 + static_cast<int>(is_close), 0.01);

			DrawPoly(ctx, interp, is_close, 4, SYellow);
		
			if (show_circles || show_tangent || show_normal || show_broken_line)
			{
				std::vector<Pointf> lines;
				complex_double der;

				std::list<complex_double> vector_list;
				
				const auto cur_pt = f.nativ_value(f.indexToAngle(pos),
					[&der, &vector_list, r_it = radii.cbegin()](const auto& gsum, const auto& coeff, const complex_double& sincos, size_t k)mutable
				{

					const auto z1 = r_it->first * sincos;
					const auto z2 = r_it->second * std::conj(sincos);
					vector_list.push_front(z2);
					vector_list.push_back(z1);
					der = fourier::derivative_step(der, coeff, sincos, k);
					++r_it;
				}
				);

				complex_double sum= f.firstCoeff();
				lines.emplace_back(sum.real(), sum.imag());
				for (const auto &vec : vector_list)
				{
					if(show_circles)
					{
						ctx.Circle(sum.real(), sum.imag(), std::abs(vec));
					}
					sum += vec;
					lines.emplace_back(sum.real(), sum.imag());
				}
		
				if (show_circles)
				{
					ctx.Opacity(0.5).Stroke(2,Color(0,0xB3u,0xB3u));
				}

				if (show_broken_line)
				{
					DrawPoly(ctx, lines, false, 2, SWhite, true);
				}

				if (show_tangent)
					ctx.DrawLine
					(
						cur_pt.real() - der.real(),
						cur_pt.imag() - der.imag(),
						cur_pt.real() + der.real(),
						cur_pt.imag() + der.imag(),
						1,
						SRed
					);

				if (show_normal)
					ctx.DrawLine
					(
						cur_pt.real() - der.imag(),
						cur_pt.imag() + der.real(),
						cur_pt.real() + der.imag(),
						cur_pt.imag() - der.real(),
						1,
						SRed
						);
				
				ctx.DrawEllipse(lines.back().x - 4, lines.back().y - 4, 8, 8, Null, 3, SRed); 
			}
		}
		
		for (const auto& pt : pts)
		{
			ctx.DrawEllipse(pt.x - 4 , pt.y - 4, 8 , 8, SWhite);
		}
	}


private:
	TopWindow *parent{};
	bool dirty = {};
	std::deque<Pointf> pts = pi_symbol;
	fourier f;
	std::deque<Pointf>::iterator cur_point = pts.end();
	std::vector<Pointf> interp;
	std::vector<std::pair<complex_double, complex_double>> radii;
	bool is_close = true;
	bool show_circles{};
	bool show_broken_line{};
	bool show_tangent{};
	bool show_normal{};
	double pos{};
};

class FourierView : public TopWindow
{

public:
	FourierView();

private:
	CanvasWidget canvas;
	Splitter splitter;
	FrameBottom<WithCtrlLayout<StaticRect>> ctrl;
};

FourierView::FourierView()
{
	canvas.setParent(this);
	splitter.Horz(ctrl, canvas);
	splitter.SetPos(270);
	Add(splitter.SizePos());
	CtrlLayout(ctrl);
	ctrl.bClear.WhenPush = [this]{canvas.clear();};
	ctrl.bPi.WhenPush = [this]{canvas.setPi();};
	ctrl.cbClosed = true;
	ctrl.cbClosed.WhenAction = [this](){canvas.setIsClose(ctrl.cbClosed);};
	ctrl.cbCircles.WhenAction = [this](){canvas.setShowCircles(ctrl.cbCircles);};
	ctrl.cbLines.WhenAction = [this](){canvas.setShowBrokenLine(ctrl.cbLines);};
	ctrl.cbTangent.WhenAction = [this](){canvas.setShowTangent(ctrl.cbTangent);};
	ctrl.cbNormal.WhenAction = [this](){canvas.setShowNormal(ctrl.cbNormal);};
	ctrl.scAngle.Range(360);
	ctrl.scAngle.WhenAction = [this]()
	{
		canvas.setPos(fourtd::pi * static_cast<int>(ctrl.scAngle.GetData()) / 180.0);
	};
	
	ctrl.cbAnim.WhenAction = [this]()
	{
		KillTimeCallback(1);
		SetTimeCallback(-100, [this]()
		{
			ctrl.scAngle.SetData(
			(static_cast<int>(ctrl.scAngle.GetData()) + 1) % ctrl.scAngle.GetMax());
			ctrl.scAngle.WhenAction();
		}
		);
	};

	Sizeable().Zoomable();

}

GUI_APP_MAIN
{
	FourierView x;
	LoadFromFile(x);
	x.Run();
	StoreToFile(x);
}
