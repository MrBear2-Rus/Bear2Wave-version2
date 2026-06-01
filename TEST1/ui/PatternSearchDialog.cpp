#include "ui/PatternSearchDialog.h"

#include <wx/msgdlg.h>
#include <wx/scrolwin.h>
#include <wx/sizer.h>

namespace {

wxChoice* make_kind_choice(wxWindow* parent)
{
    auto* choice = new wxChoice(parent, wxID_ANY);
    choice->Append(wxT("(Don't care)"));
    choice->Append(wxT("Rising Edge"));
    choice->Append(wxT("Falling Edge"));
    choice->Append(wxT("Any Edge"));
    choice->Append(wxT("High"));
    choice->Append(wxT("Low"));
    choice->Append(wxT("Value"));
    choice->Append(wxT("String"));
    choice->SetSelection(1);
    return choice;
}

} // namespace

PatternSearchDialog::PatternSearchDialog(
    wxWindow* parent,
    WaveformPanel* panel,
    const std::vector<signal_t*>& signals)
    : wxDialog(parent, wxID_ANY, wxT("Pattern Search"),
          wxDefaultPosition, wxSize(720, 420),
          wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    , m_panel(panel)
{
    auto* root = new wxBoxSizer(wxVERTICAL);
    root->Add(new wxStaticText(this, wxID_ANY,
        wxT("Set a match criterion per signal. Mark finds all matching times; "
            "Find Next/Prev jumps from the primary cursor using Pattern Search Repeat Count.")),
        0, wxEXPAND | wxALL, 8);

    auto* scroll = new wxScrolledWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
        wxVSCROLL | wxBORDER_THEME);
    scroll->SetScrollRate(0, 20);
    auto* grid = new wxFlexGridSizer(3, 8, 6);
    grid->AddGrowableCol(1, 1);
    grid->AddGrowableCol(2, 1);
    grid->Add(new wxStaticText(scroll, wxID_ANY, wxT("Signal")), 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(new wxStaticText(scroll, wxID_ANY, wxT("Match")), 0, wxALIGN_CENTER_VERTICAL);
    grid->Add(new wxStaticText(scroll, wxID_ANY, wxT("Value / String")), 0, wxALIGN_CENTER_VERTICAL);

    for (signal_t* sig : signals) {
        if (!sig)
            continue;
        RowWidgets row;
        row.sig = sig;
        const wxString name = wxString::FromUTF8(
            sig->full_name[0] ? sig->full_name : sig->name);
        grid->Add(new wxStaticText(scroll, wxID_ANY, name), 0, wxALIGN_CENTER_VERTICAL);
        row.kind = make_kind_choice(scroll);
        row.arg = new wxTextCtrl(scroll, wxID_ANY, wxEmptyString);
        grid->Add(row.kind, 1, wxEXPAND);
        grid->Add(row.arg, 1, wxEXPAND);
        m_rows.push_back(row);
    }

    scroll->SetSizer(grid);
    root->Add(scroll, 1, wxEXPAND | wxLEFT | wxRIGHT, 8);

    m_markCountLabel = new wxStaticText(this, wxID_ANY, wxT("Mark count: 0"));
    root->Add(m_markCountLabel, 0, wxALL, 8);

    auto* buttons = new wxBoxSizer(wxHORIZONTAL);
    auto* markBtn = new wxButton(this, wxID_ANY, wxT("Mark"));
    auto* clearBtn = new wxButton(this, wxID_ANY, wxT("Clear"));
    auto* nextBtn = new wxButton(this, wxID_ANY, wxT("Find Next"));
    auto* prevBtn = new wxButton(this, wxID_ANY, wxT("Find Previous"));
    auto* closeBtn = new wxButton(this, wxID_CANCEL, wxT("Close"));
    buttons->Add(markBtn, 0, wxRIGHT, 6);
    buttons->Add(clearBtn, 0, wxRIGHT, 6);
    buttons->Add(nextBtn, 0, wxRIGHT, 6);
    buttons->Add(prevBtn, 0, wxRIGHT, 6);
    buttons->AddStretchSpacer();
    buttons->Add(closeBtn, 0);
    root->Add(buttons, 0, wxEXPAND | wxALL, 8);

    SetSizer(root);
    CentreOnParent();

    markBtn->Bind(wxEVT_BUTTON, &PatternSearchDialog::OnMark, this);
    clearBtn->Bind(wxEVT_BUTTON, &PatternSearchDialog::OnClear, this);
    nextBtn->Bind(wxEVT_BUTTON, &PatternSearchDialog::OnFindNext, this);
    prevBtn->Bind(wxEVT_BUTTON, &PatternSearchDialog::OnFindPrev, this);
}

PatternSearchSpec PatternSearchDialog::BuildSpec() const
{
    PatternSearchSpec spec;
    spec.reserve(m_rows.size());
    for (const RowWidgets& row : m_rows) {
        if (!row.sig || !row.kind)
            continue;
        PatternCriterion c;
        c.kind = pattern_match_kind_from_label(row.kind->GetStringSelection().utf8_string());
        if (row.arg)
            c.arg = row.arg->GetValue().utf8_string();
        spec.emplace_back(row.sig, c);
    }
    return spec;
}

void PatternSearchDialog::OnMark(wxCommandEvent&)
{
    if (!m_panel)
        return;
    const PatternSearchSpec spec = BuildSpec();
    const size_t count = m_panel->ApplyPatternMarks(spec);
    m_markCountLabel->SetLabel(wxString::Format(wxT("Mark count: %zu"), count));
}

void PatternSearchDialog::OnClear(wxCommandEvent&)
{
    if (!m_panel)
        return;
    m_panel->ClearPatternMarks();
    m_markCountLabel->SetLabel(wxT("Mark count: 0"));
}

void PatternSearchDialog::OnFindNext(wxCommandEvent&)
{
    if (!m_panel)
        return;
    m_panel->SetLastPatternSpec(BuildSpec());
    if (!m_panel->PatternFind(true))
        wxMessageBox(wxT("No forward pattern match found."), wxT("Pattern Search"), wxOK | wxICON_INFORMATION, this);
}

void PatternSearchDialog::OnFindPrev(wxCommandEvent&)
{
    if (!m_panel)
        return;
    m_panel->SetLastPatternSpec(BuildSpec());
    if (!m_panel->PatternFind(false))
        wxMessageBox(wxT("No backward pattern match found."), wxT("Pattern Search"), wxOK | wxICON_INFORMATION, this);
}
