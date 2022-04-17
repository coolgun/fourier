#include <stdlib.h>
#include <Wt/WApplication.h>
#include <Wt/WContainerWidget.h>
#include <Wt/WPaintDevice.h>
#include <Wt/WPaintedWidget.h>
#include <Wt/WPainter.h>
#include <Wt/WSpinBox.h>
#include <Wt/WEvent.h>
#include <Wt/WPushButton.h>
#include <Wt/WHBoxLayout.h>
#include <Wt/WVBoxLayout.h>
#include <Wt/WPushButton.h>
#include <Wt/WPen.h>
#include <Wt/WSlider.h>
#include <Wt/WCheckBox.h>
#include <Wt/WLocale.h>
#include <Wt/WTimer.h>
#include "trinterp.hpp"


using namespace std::complex_literals;

namespace fourtd
{
	template<> inline complex_double fourier::make_complex<const Wt::WPointF&> [[nodiscard]] (const Wt::WPointF& c)
	{
		return { c.x(), c.y() };
	}

	template<> inline Wt::WPointF fourier::make_value<Wt::WPointF> [[nodiscard]] (const complex_double& z)
	{
		return { z.real(), z.imag() };
	}
}

using namespace fourtd;

namespace
{
	inline const std::vector<Wt::WPointF> pi_symbol =
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

class QCanvasWidget : public Wt::WPaintedWidget
{
public:

	QCanvasWidget(Wt::WApplication *app) :
		app(app),
		f(pts.cbegin(), pts.cend())
	{
		resize(800, 600);
		mouseWentDown().connect(this, &QCanvasWidget::mousePressEvent);
		mouseWentUp().connect(this, &QCanvasWidget::mouseReleaseEvent);
		mouseMoved().connect(this, &QCanvasWidget::mouseMoveEvent);
		doubleClicked().connect(this, &QCanvasWidget::mouseDoubleClickEvent);
		updateCoeff();
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

	void setPosC(double value)
	{
		setPos(value * fourtd::pi / 180.0);
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
	
	void updateCanvas(bool force = false)
	{
		update();
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

		if (parent())
		{
			auto square = std::async(std::launch::async, [this] { return f.square(); });
			auto length = std::async(std::launch::async, [this] { return f.length(0, 2 * fourtd::pi); });
			app->setTitle( Wt::WString("fourier S={1},Len={2}")
				.arg(Wt::WLocale::currentLocale().toFixedString(square.get(), 2))
				.arg(Wt::WLocale::currentLocale().toFixedString(length.get(), 2)));
		}

		radii = std::move(rad_future.get());
	}

	void paintEvent(Wt::WPaintDevice* paintDevice) override
	{
		onRenderB2D(paintDevice);
	}

	auto find_point(const Wt::WPointF& test_pt)
	{
		return std::find_if(std::execution::par_unseq, pts.begin(), pts.end(),
			[&test_pt](auto& pt)
			{
				return hypot(pt.x() - test_pt.x(), pt.y() - test_pt.y()) < sel_tolerance;
			}
		);
	}

	void mouseDoubleClickEvent(const Wt::WMouseEvent& event)
	{
		const auto pos = static_cast<Wt::WPointF>(event.widget());
		const auto test = find_point({pos.x(), pos.y()});

		if (test != pts.end())
		{
			pts.erase(test);
			cur_point = pts.end();
			updateCoeff();
			updateCanvas();
		}
	}

	void mousePressEvent(const Wt::WMouseEvent& event)
	{
		const auto pt = static_cast<Wt::WPointF>(event.widget());
		cur_point = find_point(pt);
		if (cur_point == pts.end())
		{
			const auto inter = f.lengthToPoint({ pt.x(), pt.y() });
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

	void mouseReleaseEvent(const Wt::WMouseEvent&)
	{
		cur_point = pts.end();
	}

	void mouseMoveEvent(const Wt::WMouseEvent& event)
	{
		const auto pt = static_cast<Wt::WPointF>(event.widget());
		if (cur_point != pts.end())
		{
			*cur_point = pt;
			updateCoeff();
			updateCanvas();
		}
	}

	void onRenderB2D(Wt::WPaintDevice* paintDevice)
	{
		Wt::WPainter painter(paintDevice);
		painter.setBrush(Wt::WBrush(Wt::StandardColor::Black));
		painter.drawRect(0.0, 0.0, width().value(), height().value());

		if (pts.size() > 1)
		{
			if (interp.empty())
				f.values<Wt::WPointF>(std::back_inserter(interp), 0, pts.size() -1.0 + static_cast<int>(is_close), 0.01);

			Wt::WPen pen(Wt::StandardColor::Blue);
			pen.setWidth(4);
			painter.setPen(pen);
			
			if (is_close)
				painter.drawPolygon(&interp[0], interp.size());
			else
				painter.drawPolyline(&interp[0], interp.size());

			if (show_circles || show_tangent || show_normal || show_broken_line)
			{
				std::vector< Wt::WPointF> lines;
				Wt::WPainterPath big_path;
				Wt::WPainterPath small_path;
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

				auto sum= f.firstCoeff();
				for (const auto &vec : vector_list)
				{
					const auto r = std::abs(vec);
					big_path.addEllipse(sum.real() - r, sum.imag() -r , 2 * r, 2 * r);
					sum += vec;
					lines.emplace_back(sum.real(), sum.imag());
					small_path.addEllipse(sum.real() -  2, sum.imag() -  2 , 4 , 4);
				}

				if (show_circles)
				{
					pen.setWidth(2);
					pen.setColor(Wt::WColor(0,0xB3,0xB3, 0xF0));
					painter.setPen(pen);
					painter.setBrush(Wt::BrushStyle::None);
					painter.drawPath(big_path);
				}


				if (show_broken_line)
				{
					pen.setWidth(2);
					pen.setColor(Wt::StandardColor::White);
					painter.setPen(pen);
					painter.drawPolyline(&lines[0], lines.size());
					painter.fillPath(small_path, Wt::WBrush(Wt::StandardColor::White));
				}

				pen.setWidth(1);
				pen.setColor(Wt::StandardColor::Red);
				painter.setPen(pen);

				if (show_tangent)
					painter.drawLine(cur_pt.real() - der.real(),cur_pt.imag() - der.imag(),cur_pt.real() + der.real(),cur_pt.imag() + der.imag());

				if (show_normal)
					painter.drawLine(cur_pt.real() - der.imag(),cur_pt.imag() + der.real(), cur_pt.real() + der.imag(),cur_pt.imag() - der.real());
				
				pen.setWidth(3);
				painter.setPen(pen);
				painter.setBrush(Wt::BrushStyle::None);
				painter.drawEllipse(lines.back().x() - 4, lines.back().y() - 4, 8, 8);
			}
		}

		painter.setPen(Wt::PenStyle::None);
		painter.setBrush(Wt::WBrush(Wt::StandardColor::White));
		
		for (const auto& pt : pts)
		{
			painter.drawEllipse(pt.x() - 3 , pt.y() - 3, 6, 6);
		}
	}


private:
	Wt::WApplication* app;
	std::vector<Wt::WPointF> pts = pi_symbol;
	fourier f;
	std::vector<Wt::WPointF>::iterator cur_point = pts.end();
	std::vector<Wt::WPointF> interp;
	std::vector<std::pair<complex_double, complex_double>> radii;
	bool is_close = true;
	bool show_circles{};
	bool show_broken_line{};
	bool show_tangent{};
	bool show_normal{};
	double pos{};
};


std::unique_ptr<Wt::WApplication> createApplication(const Wt::WEnvironment& env)
{
	auto app = std::make_unique<Wt::WApplication>(env);
	auto container = std::make_unique<Wt::WContainerWidget>();

	auto painting = std::make_unique<QCanvasWidget>(app.get());
	auto clear = std::make_unique<Wt::WPushButton>("Clear");
	clear->clicked().connect([painting = painting.get()]{ painting->clear(); });

	auto pi = std::make_unique<Wt::WPushButton>("Pi");
	pi->clicked().connect([painting = painting.get()]{ painting->setPi(); });

	auto is_closed = std::make_unique<Wt::WCheckBox>("Closed");
	is_closed->setChecked(true);
	is_closed->changed().connect([is_closed = is_closed.get(), painting = painting.get()]
		{ 
			painting->setIsClose(is_closed->isChecked()); 
		});

	auto show_circles = std::make_unique<Wt::WCheckBox>("Circles");
	show_circles->changed().connect([show_circles = show_circles.get(), painting = painting.get()]
		{
			painting->setShowCircles(show_circles->isChecked());
		});

	
	auto show_broken_line = std::make_unique<Wt::WCheckBox>("Zigzag lines");
	show_broken_line->changed().connect([show_broken_line = show_broken_line.get(), painting = painting.get()]
		{
			painting->setShowBrokenLine(show_broken_line->isChecked());
		});

	auto show_tangent = std::make_unique<Wt::WCheckBox>("Tangent");
	show_tangent->changed().connect([show_tangent = show_tangent.get(), painting = painting.get()]
		{
			painting->setShowTangent(show_tangent->isChecked());
		});


	auto show_normal = std::make_unique<Wt::WCheckBox>("Normal");
	show_normal->changed().connect([show_normal = show_normal.get(), painting = painting.get()]
		{
			painting->setShowNormal(show_normal->isChecked());
		});

	auto *timer = app->root()->addChild(std::make_unique<Wt::WTimer>());
	timer->setInterval(std::chrono::milliseconds(100));
	auto anima = std::make_unique<Wt::WCheckBox>("Animation");
	anima->changed().connect([anima = anima.get(), timer]
		{
			if (anima->isChecked())
				timer->start();
			else
				timer->stop();
		});

	auto sb = std::make_unique<Wt::WSpinBox>();
	sb->setRange(0, 360);
	sb->setValue(0);

	auto slider = std::make_unique<Wt::WSlider>();
	slider->setTickPosition(Wt::WSlider::TickPosition::TicksBelow);
	slider->setTickInterval(60);
	slider->setRange(0, 360);

	sb->valueChanged().connect(
		[slider = slider.get(), painting = painting.get(), sb = sb.get()]
		{
			slider->setValue(sb->value());
			painting->setPosC(sb->value());
		});

	slider->valueChanged().connect(
		[slider = slider.get(), painting = painting.get(), sb = sb.get()]
		{
			sb->setValue(slider->value());
			painting->setPosC(slider->value());
		});
	
	timer->timeout().connect([slider = slider.get(), painting = painting.get(), sb = sb.get()]
		{
			slider->setValue((slider->value() + 1) % 360 );
			sb->setValue(slider->value());
			painting->setPosC(slider->value());
		});
	

	auto vbox = std::make_unique<Wt::WVBoxLayout>();

	auto *hbox = container->setLayout(std::make_unique<Wt::WHBoxLayout>());
	
	vbox->addWidget(std::move(clear));
	vbox->addWidget(std::move(pi));
	vbox->addWidget(std::move(is_closed));
	vbox->addWidget(std::move(show_circles));
	vbox->addWidget(std::move(show_broken_line));
	vbox->addWidget(std::move(show_tangent));
	vbox->addWidget(std::move(show_normal));
	vbox->addWidget(std::move(anima));
	vbox->addWidget(std::move(slider));
	vbox->addWidget(std::make_unique<Wt::WBreak>());
	vbox->addWidget(std::move(sb));
	vbox->addStretch(1);

	hbox->addLayout(std::move(vbox));
	hbox->addWidget(std::move(painting), 1);

	app->root()->addWidget(std::move(container));
	return app;
}

int main(int argc, char** argv)
{
	return WRun(argc, argv, &createApplication);
}
