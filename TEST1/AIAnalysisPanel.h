#pragma once

#include <wx/wx.h>
#include <wx/panel.h>
#include <wx/textctrl.h>
#include <wx/button.h>
#include <wx/checklst.h>
#include <wx/combobox.h>
#include <unordered_map>
#include <vector>

#include "vcd.h"

class WaveformPanel;

class AIAnalysisPanel : public wxPanel
{
public:
    AIAnalysisPanel(wxWindow* parent, WaveformPanel* wavePanel);
    ~AIAnalysisPanel();

    void SetApiKey(const wxString& apiKey);
    wxString GetApiKey() const;

    /** Signals currently shown in the wave panel (preferred). */
    void SetDisplayedSignals(const std::vector<signal_t*>& signals, bool checkAll = true);

    /** @deprecated Use SetDisplayedSignals; kept for callers passing full file list. */
    void SetSignalInfo(const std::vector<signal_t*>& signals);

private:
    WaveformPanel* m_wavePanel;
    wxTextCtrl* m_apiKeyText;
    wxButton* m_setApiKeyBtn;
    wxComboBox* m_templateCombo;
    wxTextCtrl* m_analysisInput;
    wxButton* m_analyzeBtn;
    wxTextCtrl* m_analysisOutput;
    wxCheckListBox* m_signalList;
    wxButton* m_selectAllBtn;
    wxButton* m_selectNoneBtn;
    wxButton* m_refreshSignalsBtn;
    wxButton* m_exportBtn;
    wxStaticText* m_hintLabel;

    wxString m_apiKey;
    std::unordered_map<int, signal_t*> m_indexToSignal;

    void RefreshSignalList(const std::vector<signal_t*>& signals, bool checkAll);
    std::vector<signal_t*> GetSelectedSignals() const;

    void OnAnalyze(wxCommandEvent& event);
    void OnExport(wxCommandEvent& event);
    void OnSetApiKey(wxCommandEvent& event);
    void OnSelectAll(wxCommandEvent& event);
    void OnSelectNone(wxCommandEvent& event);
    void OnRefreshSignals(wxCommandEvent& event);
    void OnTemplatePick(wxCommandEvent& event);

    wxDECLARE_EVENT_TABLE();
};
