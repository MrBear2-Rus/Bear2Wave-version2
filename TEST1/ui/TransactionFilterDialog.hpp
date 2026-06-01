#pragma once

#include <wx/button.h>
#include <wx/dialog.h>
#include <wx/radiobox.h>
#include <wx/sizer.h>
#include <wx/stattext.h>

/** Options for Transaction Filter Process (FP-2). */
struct TransactionFilterOptions {
    enum class SignalScope {
        SelectedOnly,
        AllDisplayed
    };
    enum class TimeScope {
        VisibleWindow,
        FullFile
    };

    SignalScope signalScope = SignalScope::SelectedOnly;
    TimeScope timeScope = TimeScope::VisibleWindow;
};

class TransactionFilterDialog : public wxDialog
{
public:
    explicit TransactionFilterDialog(wxWindow* parent, bool hasSelectedSignal)
        : wxDialog(parent, wxID_ANY, wxT("Transaction Filter Process"),
            wxDefaultPosition, wxDefaultSize, wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    {
        auto* root = new wxBoxSizer(wxVERTICAL);
        root->Add(new wxStaticText(this, wxID_ANY,
            wxT("Export a minimal VCD to transaction_proc and insert decoded virtual traces below the selected row.")),
            0, wxALL | wxEXPAND, 10);

        wxArrayString sigChoices;
        sigChoices.Add(wxT("Selected signal only"));
        sigChoices.Add(wxT("All displayed signals"));
        m_signalScope = new wxRadioBox(this, wxID_ANY, wxT("Input signals"), wxDefaultPosition, wxDefaultSize,
            sigChoices, 1, wxRA_SPECIFY_ROWS);
        if (!hasSelectedSignal)
            m_signalScope->SetSelection(1);
        m_signalScope->Enable(0, hasSelectedSignal);
        root->Add(m_signalScope, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);

        wxArrayString timeChoices;
        timeChoices.Add(wxT("Visible time window"));
        timeChoices.Add(wxT("Full file"));
        m_timeScope = new wxRadioBox(this, wxID_ANY, wxT("Time range"), wxDefaultPosition, wxDefaultSize,
            timeChoices, 1, wxRA_SPECIFY_ROWS);
        root->Add(m_timeScope, 0, wxLEFT | wxRIGHT | wxBOTTOM | wxEXPAND, 10);

        auto* buttons = CreateButtonSizer(wxOK | wxCANCEL);
        root->Add(buttons, 0, wxALL | wxEXPAND, 10);
        SetSizerAndFit(root);
        CentreOnParent();
    }

    TransactionFilterOptions GetOptions() const
    {
        TransactionFilterOptions opts;
        opts.signalScope = m_signalScope->GetSelection() == 0
            ? TransactionFilterOptions::SignalScope::SelectedOnly
            : TransactionFilterOptions::SignalScope::AllDisplayed;
        opts.timeScope = m_timeScope->GetSelection() == 0
            ? TransactionFilterOptions::TimeScope::VisibleWindow
            : TransactionFilterOptions::TimeScope::FullFile;
        return opts;
    }

private:
    wxRadioBox* m_signalScope = nullptr;
    wxRadioBox* m_timeScope = nullptr;
};
