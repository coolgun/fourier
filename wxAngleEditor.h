#pragma once
#include <wx/wx.h>

class wxAngleEditor : public wxControl
{
public:
    
    wxAngleEditor(const wxAngleEditor&) = delete;
    
    wxAngleEditor operator=(const wxAngleEditor&) = delete;

    wxAngleEditor():
        wxControl()
    { 
        
    }
    
    wxAngleEditor(wxWindow* parent,
        wxWindowID winid,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = 0,
        const wxValidator& val = wxDefaultValidator,
        const wxString& name = "wxAngleEditor"):
        wxControl(parent, winid, pos, size, style, val, name)
    {
        Create(parent, winid, pos, size, style, val, name);
    }
    
    bool Create(wxWindow* parent,
        wxWindowID winid,
        const wxPoint& pos = wxDefaultPosition,
        const wxSize& size = wxDefaultSize,
        long style = 0,
        const wxValidator& val = wxDefaultValidator,
        const wxString& name = "wxAngleEditor");
    
    double angle() const
    {
        return m_value;
    }

    void setAngle(double value);

protected:

    void OnPaint(wxPaintEvent& ctx);
    void OnMouseUp(wxMouseEvent&);
    void OnMouseMove(wxMouseEvent& event);
    void OnMouseDown(wxMouseEvent& event);
    void OnSize(wxSizeEvent& event);
   
private:
    double m_value = {};
    bool m_down = {};
    double m_center = {};

};