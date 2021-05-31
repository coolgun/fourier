#include <stdlib.h>
#include <wx/wx.h>
#include <wx/graphics.h>
#include "wxAngleEditor.h"
#include "trinterp.hpp"

using namespace std::complex_literals;

namespace fourtd
{
	template<> inline complex_double fourier::make_complex<const wxPoint2DDouble&> [[nodiscard]] (const wxPoint2DDouble& c)
	{
		return { c.m_x, c.m_y};
	}

	template<> inline wxPoint2DDouble fourier::make_value<wxPoint2DDouble> [[nodiscard]] (const complex_double& z)
	{
		return { z.real(), z.imag() };
	}
}

using namespace fourtd;

namespace
{
	const std::deque<wxPoint2DDouble> pi_symbol =
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
	constexpr double sel_tolerance = 7;
}

class CanvasWidget : public wxPanel
{
	auto find_point(const wxPoint2DDouble& test_pt)
	{
		return std::find_if(std::execution::par_unseq, pts.begin(), pts.end(),
			[&test_pt](auto& pt)
			{
				const auto d = pt - test_pt;
				return std::hypot(d.m_x, d.m_y) < sel_tolerance;
			}
		);
	}
public:

	CanvasWidget(wxWindow* parent)
		: wxPanel(parent, wxID_ANY, wxDefaultPosition, wxDefaultSize)
		,f(pts.cbegin(), pts.cend())
	{
		SetBackgroundStyle(wxBG_STYLE_PAINT);
		Connect(wxEVT_PAINT, wxPaintEventHandler(CanvasWidget::OnPaint));
		Connect(wxEVT_SIZE, wxSizeEventHandler(CanvasWidget::OnSize));
		Connect(wxEVT_MOTION, wxMouseEventHandler(CanvasWidget::OnMouseMove));
		Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(CanvasWidget::OnMouseDown));
		Connect(wxEVT_LEFT_UP, wxMouseEventHandler(CanvasWidget::OnMouseUp));
		Connect(wxEVT_LEFT_DCLICK, wxMouseEventHandler(CanvasWidget::OnMouseDoubleClick));

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
		cur_point = pts.end();
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
		blBitmap.reset(new wxBitmap(event.GetSize()));
		updateCanvas(false);
	}

	void OnPaint(wxPaintEvent& WXUNUSED(event))
	{
		if (dirty)
			renderCanvas();
		wxPaintDC pdc(this);
		pdc.DrawBitmap(*blBitmap.get(), 0, 0);
	}

	void OnMouseDoubleClick(wxMouseEvent& event)
	{
		const auto test = find_point(wxPoint2DDouble(event.GetX(), event.GetY()));

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
		const wxPoint2DDouble pt(event.GetX(), event.GetY());
		
		if (cur_point != pts.end())
		{
			*cur_point = pt;
			updateCoeff();
			updateCanvas();
		}
	}

	void OnMouseDown(wxMouseEvent& event)
	{
		const wxPoint2DDouble pt(event.GetX(), event.GetY());

		cur_point = find_point(pt);
		
		if (cur_point == pts.end())
		{
			const auto inter = f.lengthToPoint({ pt.m_x,pt.m_y });
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
		auto *ctx = wxGraphicsContext::Create(*blBitmap);
		onRenderB2D(ctx);
		delete ctx;
	}

	void updateCanvas(bool force = false)
	{
		if (force)
			renderCanvas();
		else
			dirty = true;

		Refresh();
		Update();
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

		auto square = std::async(std::launch::async, [this] { return f.square(); });
		auto length = std::async(std::launch::async, [this] { return f.length(0, 2 * fourtd::pi); });
	
		radii = std::move(rad_future.get());
		
		GetParent()->SetLabel(wxString::Format(wxT("fourier - S = %f, Len = %f"), square.get(), length.get()));
	}

	void onRenderB2D(wxGraphicsContext *ctx)
	{
		//ctx->SetPen({});
		ctx->SetBrush(wxColour(0xFF000000u));
		ctx->DrawRectangle(0, 0, GetSize().x, GetSize().y);
		wxPen pen;
		if (pts.size() > 1)
		{
			if (interp.empty())
				f.values<wxPoint2DDouble>(std::back_inserter(interp), 0, pts.size() - 1.0 + static_cast<int>(is_close), 0.01);

			pen.SetColour(wxColor(0xFF00FFFFu));
			pen.SetWidth(4);
			ctx->SetPen(pen);
			
			if (is_close)
				ctx->DrawLines( interp.size(), &interp[0], wxODDEVEN_RULE);
			else
				ctx->DrawLines(interp.size(), &interp[0]);

			if (show_circles || show_tangent || show_normal || show_broken_line)
			{
				std::vector<wxPoint2DDouble> lines;
				auto big_path = ctx->CreatePath();
				auto small_path = ctx->CreatePath();
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

				complex_double sum = f.firstCoeff();
				for (const auto& vec : vector_list)
				{
					big_path.AddCircle(sum.real(), sum.imag(), std::abs(vec));
					sum += vec;
					lines.emplace_back(sum.real(), sum.imag());
					small_path.AddCircle(sum.real(), sum.imag(), 2);
				}

				if (show_circles)
				{
					pen.SetColour(wxColor(0xF0B3B300u));
					pen.SetWidth(2);
					ctx->SetPen(pen);
					ctx->StrokePath(big_path);
				}

			
				if (show_broken_line)
				{
					pen.SetColour(wxColor(0xFFFFFFFFu));
					pen.SetWidth(2);
					ctx->SetPen(pen);
					ctx->StrokeLines(lines.size(), &lines[0]);
					ctx->SetBrush(wxBrush(0xFFFFFFFFu));
					ctx->FillPath(small_path);
				}

				pen.SetColour(wxColor(0xFF0000FFu));
				pen.SetWidth(1);
				ctx->SetPen(pen);


				if (show_tangent)
					ctx->StrokeLine(cur_pt.real() - der.real(), cur_pt.imag() - der.imag(), cur_pt.real() + der.real(), cur_pt.imag() + der.imag());

				if (show_normal)
					ctx->StrokeLine(cur_pt.real() - der.imag(), cur_pt.imag() + der.real(), cur_pt.real() + der.imag(), cur_pt.imag() - der.real());
				
				pen.SetWidth(3);
				ctx->SetPen(pen);

				ctx->DrawEllipse(lines.back().m_x - 4, lines.back().m_y - 4, 8 , 8);
			}
		}

		wxGraphicsPath path = ctx->CreatePath();

		for (const auto& pt : pts)
		{
			path.AddCircle(pt.m_x, pt.m_y, 3);
		}
		ctx->SetBrush(wxColor(0xFFFFFFFFu));
		ctx->FillPath(path);
	}

private:
	bool dirty = {};
	std::unique_ptr<wxBitmap> blBitmap;
	std::deque<wxPoint2DDouble> pts = pi_symbol;
	fourier f;
	std::deque<wxPoint2DDouble>::iterator cur_point = pts.end();
	std::vector<wxPoint2DDouble> interp;
	std::vector<std::pair<complex_double, complex_double>> radii;
	bool is_close = true;
	bool show_circles{};
	bool show_broken_line{};
	bool show_tangent{};
	bool show_normal{};
	double pos{};
};

class FourierFrame : public wxFrame
{
public:
	FourierFrame(const wxString& title, const wxPoint& pos, const wxSize& size) :
		wxFrame((wxFrame*)NULL, wxID_ANY, title, pos, size,
			wxDEFAULT_FRAME_STYLE | wxNO_FULL_REPAINT_ON_RESIZE)
	{
		m_canvas = new CanvasWidget(this);

		timer.SetOwner(this);
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
			anima->Bind(wxEVT_CHECKBOX, &FourierFrame::setAnimation, this);

			auto* angle = new wxAngleEditor(panel,0);
			angle->Bind(wxEVT_SLIDER, [this, angle](wxCommandEvent&)
				{
					m_canvas->setPos(fourtd::pi * angle->angle() / 180.);
				}
			);

			this->Bind(wxEVT_TIMER, [angle](wxTimerEvent&)
				{
					angle->setAngle(angle->angle() + 1.0);
				}
			);

			auto* vbox = new wxBoxSizer(wxVERTICAL);
			vbox->Add(clear);
			vbox->Add(pi);
			vbox->Add(is_closed);
			vbox->Add(show_circles);
			vbox->Add(show_broken_line);
			vbox->Add(show_tangent);
			vbox->Add(show_normal);
			vbox->Add(anima);
			vbox->Add(angle);

			panel->SetSizer(vbox);
		}
		
		auto *hbox = new wxBoxSizer(wxHORIZONTAL);
		hbox->Add(panel, 0, wxEXPAND);
		hbox->Add(m_canvas, 1, wxEXPAND | wxALL);
		SetSizer(hbox);
	}

private:
	
	void setAnimation(wxCommandEvent& evt)
	{
		if (evt.IsChecked())
			timer.Start(100);
		else
			timer.Stop();
	}

	CanvasWidget *m_canvas;
	wxTimer timer;
};

class FourierApp : public wxApp
{
public:
	bool OnInit() wxOVERRIDE
	{
		if(!wxApp::OnInit()) return false;

		auto* frame = new FourierFrame(wxT("Fourier sample"), wxDefaultPosition, wxSize(800, 600));
		frame->Show(true);
		return true;
	}
};

IMPLEMENT_APP(FourierApp)