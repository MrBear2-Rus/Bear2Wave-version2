#pragma once

#include "core/pattern_search.h"
#include "panels/WaveformPanel.hpp"

#include <wx/button.h>
#include <wx/choice.h>
#include <wx/dialog.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>

#include <vector>

class PatternSearchDialog : public wxDialog
{
public:
    PatternSearchDialog(wxWindow* parent, WaveformPanel* panel, const std::vector<signal_t*>& signals);

private:
    struct RowWidgets {
        signal_t* sig = nullptr;
        wxChoice* kind = nullptr;
        wxTextCtrl* arg = nullptr;
    };

    WaveformPanel* m_panel = nullptr;
    std::vector<RowWidgets> m_rows;
    wxStaticText* m_markCountLabel = nullptr;

    PatternSearchSpec BuildSpec() const;
    void OnMark(wxCommandEvent&);
    void OnClear(wxCommandEvent&);
    void OnFindNext(wxCommandEvent&);
    void OnFindPrev(wxCommandEvent&);
};
