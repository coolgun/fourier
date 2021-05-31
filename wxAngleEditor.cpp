#include "wxAngleEditor.h"
#include <wx/graphics.h>

bool wxAngleEditor::Create(wxWindow* parent,
    wxWindowID winid,
    const wxPoint& pos,
    const wxSize& size,
    long style,
    const wxValidator& val,
    const wxString& name)
{
    SetBackgroundStyle(wxBG_STYLE_PAINT);
    Connect(wxEVT_PAINT, wxPaintEventHandler(wxAngleEditor::OnPaint));
    Connect(wxEVT_SIZE, wxSizeEventHandler(wxAngleEditor::OnSize));
    Connect(wxEVT_MOTION, wxMouseEventHandler(wxAngleEditor::OnMouseMove));
    Connect(wxEVT_LEFT_DOWN, wxMouseEventHandler(wxAngleEditor::OnMouseDown));
    Connect(wxEVT_LEFT_UP, wxMouseEventHandler(wxAngleEditor::OnMouseUp));
    return true;
}

void wxAngleEditor::setAngle(double value)
{
    m_value = std::fmod(value, 360.);
    wxCommandEvent event(wxEVT_SLIDER); 
    event.SetExtraLong(std::round(m_value));
    wxPostEvent(this, event); 

    Refresh();
    Update();
}

void wxAngleEditor::OnPaint(wxPaintEvent&)
{

    const auto r1 = m_center - 5.0f;
    const auto r0 = r1 * .7f;
    wxPaintDC dc(this);
    dc.Clear();
    dc.DrawLabel(wxString::Format(wxT("%.0f"), m_value), wxRect(0, 0, 2 * m_center, 2 * m_center), wxALIGN_CENTER | wxALIGN_CENTER);
    auto* ctx = wxGraphicsContext::Create(dc);
    
    wxPen pen;

    pen.SetWidth(1);
    pen.SetColour(wxColor(0,0,0));
    ctx->SetPen(pen);
    wxGraphicsPath path = ctx->CreatePath();
    path.AddCircle(m_center, m_center, r0 - 0.5);
    path.AddCircle(m_center, m_center, r1 + 0.5);
    ctx->StrokePath(path);

    ctx->Translate(m_center, m_center);
    ctx->Rotate(m_value * 3.1415 * 2 / 360.);
    const auto u = std::min(std::max(r1 / 50, 1.5), 4.);
    
    ctx->SetBrush(wxBrush(wxColor( 0, 0, 0)));
    ctx->DrawRectangle(r0 - 3, -2 * u, r1 - r0 + 6, 4 * u);
    delete ctx;
}

void wxAngleEditor::OnMouseUp(wxMouseEvent&)
{
    m_down = false;
}

void wxAngleEditor::OnMouseMove(wxMouseEvent& event)
{
    if (!m_down) return;

    const auto pos = event.GetPosition();

    auto angle = std::atan2(pos.y - m_center, pos.x - m_center);

    angle /= 2 * 3.1415;

    if (angle < 0.0)
    {
        angle = 1.0 + angle;
    }
    
    angle *= 360;

    setAngle(angle);

}

void wxAngleEditor::OnMouseDown(wxMouseEvent& event)
{
    m_down = true;
    Refresh();
    Update();
}

void wxAngleEditor::OnSize(wxSizeEvent& event)
{
    m_center = std::min(event.m_size.GetWidth(), event.m_size.GetHeight()) / 2.0;
}