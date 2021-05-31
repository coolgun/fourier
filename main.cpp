#include <stdlib.h>
#include <wx/wx.h>
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

inline int qRed(unsigned int rgb)
{
	return ((rgb >> 16) & 0xff);
}

inline int qGreen(unsigned int rgb)
{
	return ((rgb >> 8) & 0xff);
}

inline int qBlue(unsigned int rgb)
{
	return (rgb & 0xff);
}

inline int qAlpha(unsigned int rgb)
{
	return rgb >> 24;
}


namespace
{
	const std::deque<BLPoint> pi_symbol =
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
	constexpr double sel_tolerance = 7 * 7;
}

class CanvasWidget : public wxPanel
{
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
public:

	CanvasWidget(wxWindow* parent)
		: wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
		,f(pts.cbegin(), pts.cend())
	{
		//SetDoubleBuffered(true);
		SetBackgroundStyle(wxBG_STYLE_PAINT);
		createInfo.threadCount = std::thread::hardware_concurrency();
		updateCoeff();
	}

	void clear(wxCommandEvent&)
	{
		pts.clear();
		cur_point = pts.end();
		updateCoeff();
		updateCanvas();
	}

	void setPi(wxCommandEvent&)
	{
		pts = pi_symbol;
		cur_point = pts.begin();
		updateCoeff();
		updateCanvas();
	}

	void setPos(double value)
	{
		pos = value / (2 * fourtd::pi) * pts.size();
		updateCanvas(true);
	}

	void setShowCircles(wxCommandEvent& evt)
	{
		show_circles = evt.IsChecked();
		updateCanvas();
	}

	void setShowBrokenLine(wxCommandEvent& evt)
	{
		show_broken_line = evt.IsChecked();
		updateCanvas();
	}

	void setShowTangent(wxCommandEvent& evt)
	{
		show_tangent = evt.IsChecked();
		updateCanvas();
	}

	void setShowNormal(wxCommandEvent& evt)
	{
		show_normal = evt.IsChecked();
		updateCanvas();
	}

	void setIsClose(wxCommandEvent& evt)
	{
		interp.clear();
		is_close = evt.IsChecked();
		updateCanvas();
	}

	void OnSize(wxSizeEvent& event)
	{
		const auto sz = event.GetSize();
		blImage.create(sz.x, sz.y, BL_FORMAT_PRGB32);
		updateCanvas(false);
	}

	void OnPaint(wxPaintEvent& WXUNUSED(event))
	{
		if (dirty)
			renderCanvas();
		wxPaintDC pdc(this);
		const auto img = ConvertImage();
		pdc.DrawBitmap(wxBitmap(img), 0, 0);
	}

	void OnMouseDoubleClick(wxMouseEvent& event)
	{
		const auto test = find_point(BLPoint(event.GetX(), event.GetY()));

		if (test != pts.end())
		{
			pts.erase(test);
			cur_point = pts.end();
			updateCoeff();
			updateCanvas();
		}
	}

	void OnMouseUp(wxMouseEvent&)
	{
		cur_point = pts.end();
	}

	void OnMouseMove(wxMouseEvent& event)
	{
		const BLPoint pt(event.GetX(), event.GetY());
		if (cur_point != pts.end())
		{
			*cur_point = pt;
			updateCoeff();
			updateCanvas();
		}
	}

	void OnMouseDown(wxMouseEvent& event)
	{
		const BLPoint pt(event.GetX(), event.GetY());
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
				decltype(this->rad) rad;
				for (const auto& c : f.coeffs())
				{
					const auto r1 = (c.first.real() + c.second.imag()) / 2.0;
					const auto r2 = (c.first.imag() - c.second.real()) / 2.0;
					const auto r3 = (c.first.real() - c.second.imag()) / 2.0;
					const auto r4 = (c.first.imag() + c.second.real()) / 2.0;
					rad.emplace_back(r1, r2, r3, r4, std::sqrt(r1 * r1 + r2 * r2), std::sqrt(r3 * r3 + r4 * r4));
				}
				return rad;
			}
		);

		auto square = std::async(std::launch::async, [this] { return f.square(); });
		auto length = std::async(std::launch::async, [this] { return f.length(0, 2 * fourtd::pi); });

		GetParent()->SetLabel(wxString::Format(wxT("fourier - S = %f, Len = %f"), square.get(), length.get()));
		rad = std::move(rad_future.get());
	}

	void onRenderB2D(BLContext& ctx)
	{

		ctx.setFillStyle(BLRgba32(0xFF000000u));
		ctx.fillAll();

		ctx.setStrokeStyle(BLRgba32(0xFFFF0000u));
		ctx.setStrokeWidth(2);

		if (pts.size() > 1)
		{
			if (interp.empty())
				f.values<BLPoint>(std::back_inserter(interp), 0, pts.size() - 1 + static_cast<size_t>(is_close), 0.01);

			ctx.setStrokeStyle(BLRgba32(0x800000FFu));

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

				const auto cur_pt = f.nativ_value(f.indexToAngle(pos),
					[&big_path, &small_path, &lines, &der, r_it = rad.cbegin()](const auto& gsum, const auto& c, const complex_double& sincos, size_t k)mutable
					{

						if (lines.empty())
							lines.emplace_back(gsum.real(), gsum.imag());
						auto sum = gsum;
						const auto& rad = *r_it;
						const auto z1 = std::get<0>(rad) * sincos;
						const auto z2 = sincos * complex_double(0, 1) * std::get<1>(rad);// mul i 
						const auto co = std::conj(sincos);
						const auto z3 = std::get<2>(rad) * co;
						const auto z4 = std::get<3>(rad) * co * complex_double(0, 1);// mul i 

						big_path.addCircle(BLCircle(sum.real(), sum.imag(), std::get<4>(rad)));
						sum += z1 + z2;
						lines.emplace_back(sum.real(), sum.imag());
						small_path.addCircle(BLCircle(sum.real(), sum.imag(), 2));
						big_path.addCircle(BLCircle(sum.real(), sum.imag(), std::get<5>(rad)));
						sum += z3 + z4;
						lines.emplace_back(sum.real(), sum.imag());
						small_path.addCircle(BLCircle(sum.real(), sum.imag(), 2));
						fourier::derivative_step(der, c, sincos, k);
						++r_it;
					}
				);


				if (show_circles)
				{
					ctx.setStrokeWidth(2);
					ctx.setStrokeStyle(BLRgba32(0x32FFFFFFu));
					ctx.strokePath(big_path);
				}

				ctx.setStrokeWidth(1);

				if (show_broken_line)
				{
					ctx.setStrokeStyle(BLRgba32(0xFFFF0000u));
					ctx.setFillStyle(BLRgba32(0xFFFF0000u));
					ctx.strokePolyline(&lines[0], lines.size());
					ctx.fillPath(small_path);
				}

				ctx.setStrokeStyle(BLRgba32(0xFFFFFF00u));

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

	wxImage ConvertImage() const
	{

		const auto numPixels = blImage.size().w * blImage.size().h;

		if (startAlpha.size() < numPixels)
		{
			startAlpha.resize(numPixels);
			startData.resize(numPixels * 3);
		}

		unsigned char* data = &startData[0];
		unsigned char* alpha = &startAlpha[0];
		auto* pd = static_cast<unsigned int*>(blImage.impl->pixelData);
		
		std::vector<size_t> l(numPixels);
		std::iota(l.begin(), l.end(), 0);
		
		std::for_each(std::execution::par_unseq, l.cbegin(), l.cend(), [&](auto idx)
			{
				unsigned int colour = pd[idx];
				auto* p_data = data+3 * idx;
				p_data[0] = qRed(colour);
				p_data[1] = qGreen(colour);
				p_data[2] = qBlue(colour);
				alpha[idx] = qAlpha(colour);
			});
		return wxImage(blImage.size().w, blImage.size().h, &startData[0], &startAlpha[0], true);
	}


private:
	bool dirty = {};
	mutable std::vector<unsigned char> startData;
	mutable std::vector<unsigned char> startAlpha;
	BLImage blImage;
	std::deque<BLPoint> pts = pi_symbol;
	fourier f;
	std::deque<BLPoint>::iterator	cur_point = pts.end();
	std::vector<BLPoint> interp;
	std::vector<std::tuple<double, double, double, double, double, double>> rad;
	bool is_close = true;
	bool show_circles{};
	bool show_broken_line{};
	bool show_tangent{};
	bool show_normal{};
	BLContextCreateInfo createInfo{};
	double pos{};
	wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(CanvasWidget, wxScrolledWindow)
	EVT_PAINT(CanvasWidget::OnPaint)
	EVT_SIZE(CanvasWidget::OnSize)
	EVT_MOTION(CanvasWidget::OnMouseMove)
	EVT_LEFT_DOWN(CanvasWidget::OnMouseDown)
	EVT_LEFT_UP(CanvasWidget::OnMouseUp)
	EVT_LEFT_DCLICK(CanvasWidget::OnMouseDoubleClick)
wxEND_EVENT_TABLE()


class MyFrame : public wxFrame
{
public:
	MyFrame(const wxString& title, const wxPoint& pos, const wxSize& size) :
		wxFrame((wxFrame*)NULL, wxID_ANY, title, pos, size,
			wxDEFAULT_FRAME_STYLE | wxNO_FULL_REPAINT_ON_RESIZE)
	{
		m_canvas = new CanvasWidget(this);
		auto *panel = new wxPanel(this);
		{
			auto* clear = new wxButton(panel, -1, wxT("Clear"));
			clear->Bind(wxEVT_BUTTON, &CanvasWidget::clear, m_canvas);

			auto* pi = new wxButton(panel, -1, wxT("PI"));
			pi->Bind(wxEVT_BUTTON, &CanvasWidget::setPi, m_canvas);

			auto* is_closed = new wxCheckBox(panel, 0, wxT("Closed"));
			is_closed->SetValue(true);
			is_closed->Bind(wxEVT_CHECKBOX, &CanvasWidget::setIsClose, m_canvas);

			auto* show_circles = new wxCheckBox(panel, 0, wxT("Circles"));
			show_circles->Bind(wxEVT_CHECKBOX, &CanvasWidget::setShowCircles, m_canvas);

			auto* show_broken_line = new wxCheckBox(panel, 0, wxT("Zigzag lines"));
			show_broken_line->Bind(wxEVT_CHECKBOX, &CanvasWidget::setShowBrokenLine, m_canvas);

			auto* show_tangent = new wxCheckBox(panel, 0, wxT("Tangent"));
			show_tangent->Bind(wxEVT_CHECKBOX, &CanvasWidget::setShowTangent, m_canvas);

			auto* show_normal = new wxCheckBox(panel, 0, wxT("Normal"));
			show_normal->Bind(wxEVT_CHECKBOX, &CanvasWidget::setShowNormal, m_canvas);

			auto* anima = new wxCheckBox(panel, 0, wxT("Animation"));
			anima->Bind(wxEVT_CHECKBOX, &MyFrame::setAnimation, this);

			position = new wxSlider(panel,0,0,0,1000);
			position->Bind(wxEVT_SLIDER, &MyFrame::setPos, this);
		
			auto* vbox = new wxBoxSizer(wxVERTICAL);
			vbox->Add(clear);
			vbox->Add(pi);
			vbox->Add(is_closed);
			vbox->Add(show_circles);
			vbox->Add(show_broken_line);
			vbox->Add(show_tangent);
			vbox->Add(show_normal);
			vbox->Add(anima);
			vbox->Add(position);

			panel->SetSizer(vbox);
		}
		
		auto *hbox = new wxBoxSizer(wxHORIZONTAL);
		hbox->Add(panel, 0, wxEXPAND);
		hbox->Add(m_canvas, 1, wxEXPAND | wxALL);
		SetSizer(hbox);

		timer.SetOwner(this);
		this->Bind(wxEVT_TIMER, &MyFrame::OnTimer, this);
	}

private:
	
	void setAnimation(wxCommandEvent& evt)
	{
		if (evt.IsChecked())
			timer.Start(100);
		else
			timer.Stop();
	}

	void setPos(int value)
	{
		m_canvas->setPos(2 * fourtd::pi * value / position->GetMax());
	}

	void setPos(wxCommandEvent&)
	{
		setPos(position->GetValue());
	}

	void OnTimer(wxTimerEvent&)
	{
		position->SetValue((position->GetValue() + 1) % position->GetMax());
		setPos(position->GetValue());
	}

	CanvasWidget *m_canvas;
	wxSlider* position;
	wxTimer timer;
};

class MyApp : public wxApp
{
public:
	bool OnInit() wxOVERRIDE
	{
		if(!wxApp::OnInit()) return false;

		auto* frame = new MyFrame(wxT("Drawing sample"), wxDefaultPosition, wxSize(800, 600));
		frame->Show(true);
		return true;
	}
};

IMPLEMENT_APP(MyApp)