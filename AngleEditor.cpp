#include "AngleEditor.h"
#include <nanogui/theme.h>
#include <nanogui/opengl.h>
#include <nanogui/layout.h>
#include <nanogui/serializer/core.h>
#include <Eigen/QR>
#include <Eigen/Geometry>

NAMESPACE_BEGIN(nanogui)

AngleEditor::AngleEditor(Widget* parent, double value)
    : Widget(parent), 
    m_intbox(new IntBox<int>(this)),
    m_down(false)
{
    auto* layout = new GridLayout(Orientation::Horizontal, 1, Alignment::Middle);
    layout->setColAlignment({ Alignment::Middle, Alignment::Middle });
    setLayout(layout);
    m_intbox->setEditable(true);
    m_intbox->setFormat("[1-9][0-9]*");
    m_intbox->setSpinnable(true);
    m_intbox->setFixedSize(Vector2i(60, 20));
    m_intbox->setAlignment(TextBox::Alignment::Right);
    m_intbox->setMinValue(0);
    m_intbox->setMaxValue(360);
    m_intbox->setValueIncrement(1);
    m_intbox->setCallback(
        [this](int value) 
        {
            setAngle(value);
          
        });
    setAngle(value);
}

Vector2i AngleEditor::preferredSize(NVGcontext*) const
{
    return { 100, 100. };
}

void AngleEditor::draw(NVGcontext* ctx) 
{
    if (!mVisible)
        return;

    const float x = mPos.x(),
        y = mPos.y(),
        w = mSize.x(),
        h = mSize.y();


    const auto cx = x + w * 0.5f;
    const auto cy = y + h * 0.5f;
    const auto r1 = std::min(w , h) * 0.5f - 5.0f;
    const auto r0 = r1 * .8f;


    nvgBeginPath(ctx);
    nvgCircle(ctx, cx, cy, r0 - 0.5f);
    nvgCircle(ctx, cx, cy, r1 + 0.5f);
    nvgStrokeColor(ctx, nvgRGB(0, 0, 0));
    nvgStrokeWidth(ctx, 1.0f);
    nvgStroke(ctx);

    nvgSave(ctx);
    nvgTranslate(ctx, cx, cy);
    nvgRotate(ctx, m_value * NVG_PI * 2 /360.);

    const auto u = std::min(std::max(r1 / 50, 1.5f), 4.f);
    nvgStrokeWidth(ctx, u);
    nvgBeginPath(ctx);
    nvgRect(ctx, r0 - 3, -2 * u, r1 - r0 + 6, 4 * u);
    nvgFillColor(ctx, nvgRGB(0, 0, 0));
    nvgFill(ctx);
 
    nvgRestore(ctx);

    Widget::draw(ctx);

    performLayout(ctx);
    
}

bool AngleEditor::mouseButtonEvent(const Vector2i& p, int button, bool down, int modifiers) 
{
    Widget::mouseButtonEvent(p, button, down, modifiers);
    
    if (!mEnabled || button != GLFW_MOUSE_BUTTON_1)
        return false;
    requestFocus();
    m_down = down;
    
    return true;

}

bool AngleEditor::mouseMotionEvent(const Eigen::Vector2i& p, const Eigen::Vector2i& rel, int button, int modifiers)
{
    Widget::mouseMotionEvent(p,rel,button,modifiers);
    if (!m_down) return false;

    float x = p.x() - mPos.x(),
        y = p.y() - mPos.y(),
        w = mSize.x(),
        h = mSize.y();

    float cx = w * 0.5f;
    float cy = h * 0.5f;
  
    x -= cx;
    y -= cy;

    auto angle = std::atan2(y , x);
    
    angle /= 2 * NVG_PI;

    if (angle < 0.0)
    {
        angle = 1.0 + angle;
    }
    angle *= 360;

    setAngle(angle);

    return true;
}

double AngleEditor::angle() const
{
    return m_value;
}

void AngleEditor::setAngle(double value)
{
    m_value = std::fmod(value, 360.);
    m_intbox->setValue(m_value);
    if (mCallback)
        mCallback(value);
}

void AngleEditor::save(Serializer& s) const 
{
    Widget::save(s);
    s.set("angle", m_value);
}

bool AngleEditor::load(Serializer& s) 
{
    if (!Widget::load(s)) return false;
    if (!s.get("angle", m_value)) return false;
    return true;
}

NAMESPACE_END(nanogui)

