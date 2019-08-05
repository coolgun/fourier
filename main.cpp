#include <stdlib.h>
#include <QtGui>
#include <QtWidgets>
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
	const QList<BLPoint> pi_symbol =
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

class QCanvasWidget : public QWidget
{
public:

	QCanvasWidget() :
		f(pts.cbegin(), pts.cend())
	{

		createInfo.threadCount = std::thread::hardware_concurrency();
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
		cur_point = pts.begin();
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
		repaint();
	}

	void resizeCanvas()
	{
		const auto sz = size();
		if (substr.size() == sz)
			return;
		substr = QImage(sz, QImage::Format_ARGB32_Premultiplied);
		blImage.createFromData(substr.width(), substr.height(), BL_FORMAT_PRGB32, substr.bits(), substr.bytesPerLine());
		updateCanvas(false);
	}

	bool dirty = {};

	void resizeEvent(QResizeEvent*) override
	{
		resizeCanvas();
	}

	void updateCoeff()
	{
		interp.clear();
		rad.clear();
		f.calcul_coeff(pts.cbegin(), pts.cend());
		for (const auto& c : f.coeffs())
		{
			const auto r1 = (c.first.real() + c.second.imag()) / 2.0;
			const auto r2 = (c.first.imag() - c.second.real()) / 2.0;
			const auto r3 = (c.first.real() - c.second.imag()) / 2.0;
			const auto r4 = (c.first.imag() + c.second.real()) / 2.0;
			rad.emplace_back(r1, r2, r3, r4, std::sqrt(r1 * r1 + r2 * r2), std::sqrt(r3 * r3 + r4 * r4));
		}

		if (parentWidget())
		{
			parentWidget()->setWindowTitle(QString("fourier - S=%1 , Len=%2").arg(f.square()).arg(f.length(0, 2 * fourtd::pi)));
		}

	}

	void paintEvent(QPaintEvent*) override
	{
		QPainter painter(this);
		if (dirty)
			renderCanvas();
		painter.drawImage(QPoint{ 0, 0 }, substr);
	}

	auto find_point(const BLPoint& test_pt)
	{
		return std::find_if(pts.begin(), pts.end(),
			[&test_pt](auto& pt)
			{
				const auto d = pt - test_pt;
				return (d.x * d.x + d.y * d.y) < sel_tolerance;
			}
		);
	}

	void mouseDoubleClickEvent(QMouseEvent* event) override
	{
		const auto test = find_point(BLPoint(event->pos().x(), event->pos().y()));

		if (test != pts.end())
		{
			pts.erase(test);
			cur_point = pts.end();
			updateCoeff();
			updateCanvas();
		}
	}

	void mousePressEvent(QMouseEvent* event) override
	{
		const BLPoint pt(event->pos().x(), event->pos().y());
		cur_point = find_point(pt);
		if (cur_point == pts.end())
		{
			const auto inter = f.lengthToPoint({ pt.x,pt.y });
			if (std::get<2>(inter) < 5)
			{
				const auto index = static_cast<int>(std::ceil(f.angleToIndex(std::get<0>(inter))));
				pts.insert(index, pt);
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

	void mouseReleaseEvent(QMouseEvent*) override
	{
		cur_point = pts.end();
	}

	void mouseMoveEvent(QMouseEvent* event) override
	{
		const BLPoint pt(event->pos().x(), event->pos().y());
		if (cur_point != pts.end())
		{
			*cur_point = pt;
			updateCoeff();
			updateCanvas();
		}
	}

	void showEvent(QShowEvent* event) override
	{
		updateCoeff();
		QWidget::showEvent(event);
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
				f.values<BLPoint>(std::back_inserter(interp), 0, pts.size() - 1 + static_cast<int>(is_close), 0.01);

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
					const auto z2 = sincos * 1i * std::get<1>(rad);// mul i 
					const auto co = std::conj(sincos);
					const auto z3 = std::get<2>(rad) * co;
					const auto z4 = std::get<3>(rad) * co * 1i;// mul i 

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
					ctx.strokePolyline(&std::vector<BLPoint>({ {cur_pt.real() - der.real(),cur_pt.imag() - der.imag()},{cur_pt.real() + der.real(),cur_pt.imag() + der.imag()} })[0], 2);

				if (show_normal)
					ctx.strokePolyline(&std::vector<BLPoint>({ {cur_pt.real() - der.imag(),cur_pt.imag() + der.real()},{cur_pt.real() + der.imag(),cur_pt.imag() - der.real()} })[0], 2);
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
	QImage substr;
	QList<BLPoint> pts = pi_symbol;
	fourier f;
	QList<BLPoint>::iterator	cur_point = pts.end();
	std::vector<BLPoint> interp;
	std::vector<std::tuple<double, double, double, double, double, double>> rad;
	bool is_close = true;
	bool show_circles{};
	bool show_broken_line{};
	bool show_tangent{};
	bool show_normal{};
	BLContextCreateInfo createInfo{};
	double pos{};
};

int main(int argc, char* argv[])
{
	std::vector<int>a;
	QApplication app(argc, argv);
	QWidget win;

	auto* h = new QHBoxLayout;
	auto* l = new QVBoxLayout;
	h->setContentsMargins(1, 1, 1, 1);
	l->setContentsMargins(1, 1, 1, 1);
	auto* clear = new QPushButton("Clear");
	auto* pi = new QPushButton("Pi");
	auto* is_closed = new QCheckBox("Closed");
	is_closed->setChecked(true);
	auto* show_circles = new QCheckBox("Circles");
	auto* show_broken_line = new QCheckBox("Zigzag lines");
	auto* show_tangent = new QCheckBox("Tangent");
	auto* show_normal = new QCheckBox("Normal");

	auto* anima = new QCheckBox("Animation");
	auto* positin = new QDial();
	positin->setWrapping(true);
	auto* canvas = new QCanvasWidget;

	QTimer timer;
	QObject::connect(&timer, &QTimer::timeout, [positin]
		{
			positin->setValue(positin->value() + 1);
		}
	);
	timer.setInterval(100);

	l->addWidget(pi);
	l->addWidget(clear);
	l->addWidget(is_closed);
	l->addWidget(show_circles);
	l->addWidget(show_broken_line);
	l->addWidget(show_tangent);
	l->addWidget(show_normal);
	l->addWidget(anima);
	l->addWidget(positin);
	l->addStretch();
	h->addLayout(l);
	h->addWidget(canvas, 1);
	positin->setMaximum(100);

	QObject::connect(pi, &QPushButton::pressed, canvas, &QCanvasWidget::setPi);
	QObject::connect(clear, &QPushButton::pressed, canvas, &QCanvasWidget::clear);
	QObject::connect(is_closed, &QCheckBox::toggled, canvas, &QCanvasWidget::setIsClose);
	QObject::connect(show_circles, &QCheckBox::toggled, canvas, &QCanvasWidget::setShowCircles);
	QObject::connect(show_broken_line, &QCheckBox::toggled, canvas, &QCanvasWidget::setShowBrokenLine);
	QObject::connect(show_tangent, &QCheckBox::toggled, canvas, &QCanvasWidget::setShowTangent);
	QObject::connect(show_normal, &QCheckBox::toggled, canvas, &QCanvasWidget::setShowNormal);

	QObject::connect(anima, &QCheckBox::toggled, [&timer](bool value)
		{
			if (value)
				timer.start();
			else
				timer.stop();
		}
	);

	QObject::connect(positin, &QDial::valueChanged, [canvas, positin](int value)
		{
			canvas->setPos(2 * fourtd::pi * value / positin->maximum());
		}
	);

	win.setLayout(h);

	win.resize(QSize(800, 600));
	win.show();
	return app.exec();
}