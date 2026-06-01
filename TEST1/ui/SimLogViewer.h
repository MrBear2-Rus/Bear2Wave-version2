#pragma once

#include "core/sim_log.h"

#include <functional>
#include <vector>

#include <wx/frame.h>
#include <wx/listctrl.h>

class SimLogViewer : public wxFrame
{
public:
    explicit SimLogViewer(wxWindow* parent);
    bool LoadFile(const wxString& path, wxString* error = nullptr);
    void SetJumpCallback(std::function<void(long long)> cb) { m_onJump = std::move(cb); }

private:
    wxListCtrl* m_list = nullptr;
    std::vector<SimLogLine> m_lines;
    wxString m_path;
    std::function<void(long long)> m_onJump;

    void RebuildList();
    void OnItemActivated(wxListEvent& event);
};
