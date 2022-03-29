#include <stdlib.h>
#include <format>
#include "trinterp.hpp"
#include "imgui.h"

using namespace std::complex_literals;

static bool AngleEditor(const char* label, float* p_value, float v_min, float v_max)
{
	const ImGuiIO& io = ImGui::GetIO();
	const ImGuiStyle& style = ImGui::GetStyle();
	constexpr float radius_outer = 20.0f;
	constexpr float radius_inner = radius_outer * 0.40f;

	const ImVec2 pos = ImGui::GetCursorScreenPos();
	const float line_height = ImGui::GetTextLineHeight();

	const ImVec2 center = ImVec2(pos.x + radius_outer, pos.y + radius_outer + line_height + style.ItemInnerSpacing.y);
	float angle = 0.f;
	ImDrawList* draw_list = ImGui::GetWindowDrawList();
	draw_list->AddText(ImVec2(pos.x, pos.y), ImGui::GetColorU32(ImGuiCol_Text), label);

	ImGui::InvisibleButton(label, ImVec2(radius_outer * 2, radius_outer * 2 + line_height + style.ItemInnerSpacing.y));
	bool value_changed = false;
	const bool is_active = ImGui::IsItemActive();
	if (is_active)
	{
		angle = std::atan2f(io.MousePos.x - center.x, io.MousePos.y - center.y);
		*p_value = (1.0f - (angle + 3.1415f) / 3.1415f / 2.0f) * (v_max - v_min)  + v_min;
		value_changed = true;
	}
	else
	{
		angle = 2 * 3.1415f * (1.0f - (*p_value - v_min) / (v_max - v_min)) - 3.1415f;
	}

	draw_list->AddCircleFilled(center, radius_outer, ImGui::GetColorU32(ImGuiCol_FrameBg), 16);
	const float angle_cos = sinf(angle);
	const float angle_sin = cosf(angle);

	draw_list->AddLine(
		ImVec2(
			center.x + angle_cos * radius_inner,
			center.y + angle_sin * radius_inner
		),
		ImVec2(
			center.x + angle_cos * (radius_outer - 2),
			center.y + angle_sin * (radius_outer - 2)
		),
		ImGui::GetColorU32(ImGuiCol_SliderGrabActive), 2.0f);
	
	draw_list->AddCircleFilled(
		center,
		radius_inner,
		ImGui::GetColorU32(is_active ? ImGuiCol_FrameBgActive : ImGuiCol_FrameBg)
		, 16);
	
	if (is_active)
	{
		ImGui::SetNextWindowPos(ImVec2(pos.x - style.WindowPadding.x, pos.y - line_height - style.ItemInnerSpacing.y - style.WindowPadding.y));
		ImGui::BeginTooltip();
		ImGui::Text("%.3f", *p_value);
		ImGui::EndTooltip();
	}

	return value_changed;
}

namespace fourtd
{
	template<> inline complex_double fourier::make_complex<const ImVec2&> [[nodiscard]] (const ImVec2& c)
	{
		return { c.x,c.y };
	}

	template<> inline ImVec2 fourier::make_value<ImVec2> [[nodiscard]] (const complex_double& z)
	{
		return { static_cast<float>(z.real()), static_cast<float>(z.imag()) };
	}
}

using namespace fourtd;

namespace
{
	inline const std::vector<ImVec2> pi_symbol =
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

class CanvasWidget 
{
public:

	CanvasWidget() :
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

	const std::string getTitle() const
	{
		return title;
	}

	void onRender()
	{
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		const auto window_pos = ImGui::GetWindowPos();

		const auto translate = [&window_pos](const complex_double& pt)
		{
			return ImVec2{ static_cast<float>(pt.real() + window_pos.x), static_cast<float>(pt.imag() + window_pos.y) };
		};

		if (pts.size() > 1)
		{
			if (interp.empty())
				f.values<ImVec2>(std::back_inserter(interp), 0, pts.size() - 1.0 + static_cast<int>(is_close), 0.01);
			
			auto local_interp = interp;
			std::for_each(local_interp.begin(), local_interp.end(), [&window_pos](ImVec2& pt) { pt.x += window_pos.x; pt.y += window_pos.y; });

			draw_list->AddPolyline(&local_interp[0], static_cast<int>(local_interp.size()), IM_COL32(255, 255, 0, 255), is_close ? ImDrawFlags_Closed : 0, 4.0f);

			
			if (show_circles || show_tangent || show_normal || show_broken_line)
			{
				const auto& first_coeff = f.firstCoeff();

				std::vector<ImVec2> lines{ translate(first_coeff) };
				complex_double der;

				std::list<complex_double> vector_list;
				const auto cur_pt = f.nativ_value(f.indexToAngle(pos),
					[&der, &vector_list, r_it = radii.cbegin()](const auto& gsum, const auto& coeff, const complex_double& sincos, size_t k)mutable
				{
					vector_list.push_front(r_it->second* std::conj(sincos));
					vector_list.push_back(r_it->first* sincos);
					der = fourier::derivative_step(der, coeff, sincos, k);
					++r_it;
				}
				);

				complex_double sum = first_coeff;
				for (const auto& vec : vector_list)
				{
					if (show_circles)
						draw_list->AddCircle(translate(sum), static_cast<float>(std::abs(vec)), 0xF0B3B300u, 0, 2.0f);
					sum += vec;
					if (show_broken_line)
					{
						const auto trans_pt = translate(sum);
						lines.emplace_back(translate(sum));
						draw_list->AddCircle(translate(sum), 2, IM_COL32(255, 255, 255, 255));
					}
				}


				if (show_broken_line)
				{
					draw_list->AddPolyline(&lines[0],static_cast<int>(lines.size()), IM_COL32(255, 255, 255, 255), 0, 2.0f);
				}

				if (show_tangent)
				{
					draw_list->AddLine(
						translate({ cur_pt.real() - der.real(), cur_pt.imag() - der.imag() }),
						translate({ cur_pt.real() + der.real(), cur_pt.imag() + der.imag() }),
						IM_COL32(255, 0, 0, 255)
					);
				}

				if (show_normal)
				{
					draw_list->AddLine(
						translate({ cur_pt.real() - der.imag(), cur_pt.imag() + der.real() }),
						translate({ cur_pt.real() + der.imag(), cur_pt.imag() - der.real() }),
						IM_COL32(255, 0, 0, 255)
					);
				}
				draw_list->AddCircle(translate(cur_pt), 4, IM_COL32(255, 0, 0, 255), 0, 3);
			}
		}

		for (const auto& pt : pts)
		{
			draw_list->AddCircleFilled({pt.x + window_pos.x, pt.y + window_pos.y}, 3, IM_COL32(255, 255, 255, 255));
		}
	}

	void OnMouseWorks()
	{
		static ImVector<ImVec2> points;
		static bool adding_line = false;
		const auto window_pos = ImGui::GetWindowPos();

		ImVec2 origin = ImGui::GetCursorScreenPos();      // ImDrawList API uses screen coordinates!
		ImVec2 canvas_sz = ImGui::GetContentRegionAvail();   // Resize canvas to what's available
		ImGuiIO& io = ImGui::GetIO();
		ImDrawList* draw_list = ImGui::GetWindowDrawList();
		ImGui::InvisibleButton("canvas", canvas_sz, ImGuiButtonFlags_MouseButtonLeft | ImGuiButtonFlags_MouseButtonRight);
		const bool is_hovered = ImGui::IsItemHovered(); // Hovered
		const ImVec2 mouse_pos_in_canvas(io.MousePos.x - window_pos.x, io.MousePos.y - window_pos.y);

		if (is_hovered && ImGui::IsMouseDoubleClicked(ImGuiMouseButton_Left))
		{
			mouseDoubleClickEvent(mouse_pos_in_canvas);
			return;
		}
		
		if (is_hovered && !adding_line && ImGui::IsMouseClicked(ImGuiMouseButton_Left))
		{
			mousePressEvent(mouse_pos_in_canvas);
			adding_line = true;
		}
		if (adding_line)
		{
			mouseMoveEvent(mouse_pos_in_canvas);
			if (!ImGui::IsMouseDown(ImGuiMouseButton_Left))
			{
				adding_line = false;
				mouseReleaseEvent();
			}
		}


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

				decltype(radii) rad;
				for (const auto& c : f.coeffs())
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
		title = std::format("fourier - S={} , Len={}", square.get(), length.get());
		radii = std::move(rad_future.get());
	}

	auto find_point(const ImVec2& test_pt)
	{
		return std::find_if(std::execution::par_unseq, pts.begin(), pts.end(),
			[&test_pt](auto& pt)
			{
				const ImVec2 d(pt.x - test_pt.x, pt.y - test_pt.y);
				return (d.x * d.x + d.y * d.y) < sel_tolerance;
			}
		);
	}

	void mouseDoubleClickEvent(const ImVec2& pos)
	{
		const auto test = find_point(pos);

		if (test != pts.end())
		{
			pts.erase(test);
			cur_point = pts.end();
			updateCoeff();
		}
	}

	void mousePressEvent(const ImVec2 &pos)
	{
		cur_point = find_point(pos);
		if (cur_point == pts.end())
		{
			const auto inter = f.lengthToPoint({ pos.x,pos.y });
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
	}

	void mouseReleaseEvent()
	{
		cur_point = pts.end();
	}

	void mouseMoveEvent(const ImVec2& pos)
	{
		if (cur_point != pts.end())
		{
			*cur_point = pos;
			updateCoeff();
		}
	}

private:
	std::string title;
	std::vector<ImVec2> pts = pi_symbol;
	fourier f;
	std::vector<ImVec2>::iterator cur_point = pts.end();
	std::vector<ImVec2> interp;
	std::vector<std::pair<complex_double, complex_double>> radii;
	bool is_close = true;
	bool show_circles{};
	bool show_broken_line{};
	bool show_tangent{};
	bool show_normal{};
	double pos{};
};



void ShowFourtd()
{
	static CanvasWidget widget;
	
	if (!ImGui::Begin(!widget.getTitle().empty() ? widget.getTitle().c_str() : ""))
	{
		ImGui::End();
		return;
	}

	if (ImGui::Button("Clear")) 
		widget.clear();
	if (ImGui::Button("Pi"))
		widget.setPi();
	
	static bool is_closed = true;
	if (ImGui::Checkbox("Closed", &is_closed))
	{
		widget.setIsClose(is_closed);
	}

	static bool show_circles = false;
	if (ImGui::Checkbox("Circles", &show_circles))
	{
		widget.setShowCircles(show_circles);
	}

	static bool show_broken_line = false;
	if (ImGui::Checkbox("Zigzag lines", &show_broken_line))
	{
		widget.setShowBrokenLine(show_broken_line);
	}

	static bool show_tangent = false;
	if (ImGui::Checkbox("Tangent", &show_tangent))
	{
		widget.setShowTangent(show_tangent);
	}

	static bool show_normal = false;
	if (ImGui::Checkbox("Normal", &show_normal))
	{
		widget.setShowNormal(show_normal);
	}

	static float angle = 0.0f;
	if (AngleEditor("Angle", &angle, 0.0f, 360.f))
	{
		widget.setPos(2 * fourtd::pi * angle / 360.0);
	}

	static bool animate = false;
	ImGui::Checkbox("Animation", &animate);

	if (animate)
	{
		++angle ;
		if (angle >= 360.0f) 
		{ 
			angle = 0.0f; 
		}
		widget.setPos(2 * fourtd::pi * angle / 360.0);
	}


	if (ImGui::Begin("Canvas"))
	{
		widget.OnMouseWorks();
		widget.onRender();
		ImGui::End();
	}
	ImGui::End();
}