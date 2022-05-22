#pragma once
#include <nanogui/widget.h>
#include <nanogui/textbox.h>

template<class Poly>
void nvgPolygone(NVGcontext* ctx, const Poly& poly, bool close)
{
    nvgBeginPath(ctx);
    if (poly.size() < 2)
        return;
    bool first = true;
    for (const auto& p : poly)
    {
        if (first)
        {
            nvgMoveTo(ctx, p.x(), p.y());
            first = false;
            continue;
        }
        nvgLineTo(ctx, p.x(), p.y());
    }
    if (close)
        nvgClosePath(ctx);

    nvgStroke(ctx);
}

NAMESPACE_BEGIN(nanogui)

class  AngleEditor : public Widget 
{
public:
    AngleEditor(Widget* parent, double value = 0.0);

    std::function<void(double)> callback() const { return mCallback; }

    void setCallback(const std::function<void(double)>& callback) { mCallback = callback; }

    double angle() const;

    void setAngle(double value);

    Vector2i preferredSize(NVGcontext* ctx) const override;

    void draw(NVGcontext* ctx) override;

    bool mouseButtonEvent(const Vector2i& p, int button, bool down, int modifiers) override;

    bool mouseMotionEvent(const Eigen::Vector2i& p, const Eigen::Vector2i& rel, int button, int modifiers)  override;

    void save(Serializer& s) const override;

    bool load(Serializer& s) override;

private:
    
    double m_value;
    bool m_down;
    IntBox<int> *m_intbox;
protected:
    
    std::function<void(double)> mCallback;

public:
    EIGEN_MAKE_ALIGNED_OPERATOR_NEW
};

NAMESPACE_END(nanogui)