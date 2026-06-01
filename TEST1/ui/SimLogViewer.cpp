#include "ui/SimLogViewer.h"

#include <wx/button.h>
#include <wx/filename.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

SimLogViewer::SimLogViewer(wxWindow* parent)
    : wxFrame(parent, wxID_ANY, wxT("Simulation Log"),
          wxDefaultPosition, wxSize(900, 360), wxDEFAULT_FRAME_STYLE)
{
    auto* root = new wxBoxSizer(wxVERTICAL);
    m_list = new wxListCtrl(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxLC_REPORT | wxLC_SINGLE_SEL);
    m_list->AppendColumn(wxT("#"), wxLIST_FORMAT_RIGHT, 56);
    m_list->AppendColumn(wxT("Time"), wxLIST_FORMAT_RIGHT, 90);
    m_list->AppendColumn(wxT("Message"), wxLIST_FORMAT_LEFT, 720);
    root->Add(m_list, 1, wxEXPAND | wxALL, 6);

    auto* footer = new wxBoxSizer(wxHORIZONTAL);
    footer->Add(new wxStaticText(this, wxID_ANY,
        wxT("Double-click a timestamped line to jump the waveform cursor.")),
        1, wxALIGN_CENTER_VERTICAL | wxLEFT, 4);
    auto* closeBtn = new wxButton(this, wxID_CLOSE, wxT("Close"));
    footer->Add(closeBtn, 0, wxALL, 4);
    root->Add(footer, 0, wxEXPAND);

    SetSizer(root);
    Bind(wxEVT_LIST_ITEM_ACTIVATED, &SimLogViewer::OnItemActivated, this);
    closeBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { Close(); });
}

bool SimLogViewer::LoadFile(const wxString& path, wxString* error)
{
    std::string err;
    m_lines = sim_log_parse_file(path.utf8_string(), &err);
    if (m_lines.empty()) {
        if (error)
            *error = wxString::FromUTF8(err.empty() ? "failed to load log" : err);
        return false;
    }
    m_path = path;
    SetTitle(wxString::Format(wxT("Simulation Log — %s"), wxFileName(path).GetFullName()));
    RebuildList();
    return true;
}

void SimLogViewer::RebuildList()
{
    m_list->DeleteAllItems();
    for (size_t i = 0; i < m_lines.size(); ++i) {
        const SimLogLine& line = m_lines[i];
        const long idx = m_list->InsertItem(m_list->GetItemCount(),
            wxString::Format(wxT("%zu"), line.lineNumber));
        if (line.hasTimestamp)
            m_list->SetItem(idx, 1, wxString::Format(wxT("%lld"), line.timestamp));
        m_list->SetItem(idx, 2, wxString::FromUTF8(line.text));
    }
}

void SimLogViewer::OnItemActivated(wxListEvent& event)
{
    const long idx = event.GetIndex();
    if (idx < 0 || idx >= (long)m_lines.size())
        return;
    const SimLogLine& line = m_lines[(size_t)idx];
    if (!line.hasTimestamp || !m_onJump)
        return;
    m_onJump(line.timestamp);
}
