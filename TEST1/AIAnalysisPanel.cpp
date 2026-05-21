#include "AIAnalysisPanel.h"

#include "waveform_analysis.h"
#include "panels/WaveformPanel.hpp"

#include <wx/filedlg.h>
#include <fstream>
#include <ctime>
#include <exception>
#include <windows.h>
#include <wininet.h>

namespace {

static const char* kTemplatePrompts[] = {
    "Analyze the waveform in the context below. Identify timing anomalies, reset issues, and suspicious glitches. Cite specific timestamps.",
    "Check clock/reset sequencing: when does reset release, and do data signals only change after clocks are stable?",
    "Look for bus stability and protocol handshaking (valid/ready, enable pairs). Flag violations with timestamps.",
    "Summarize activity per signal in the viewport. Note constant signals, unexpected toggles, and X/Z if present.",
};

} // namespace

wxBEGIN_EVENT_TABLE(AIAnalysisPanel, wxPanel)
    EVT_BUTTON(1, AIAnalysisPanel::OnAnalyze)
    EVT_BUTTON(2, AIAnalysisPanel::OnExport)
    EVT_BUTTON(3, AIAnalysisPanel::OnSetApiKey)
    EVT_BUTTON(5, AIAnalysisPanel::OnSelectAll)
    EVT_BUTTON(6, AIAnalysisPanel::OnSelectNone)
    EVT_BUTTON(7, AIAnalysisPanel::OnRefreshSignals)
    EVT_COMBOBOX(8, AIAnalysisPanel::OnTemplatePick)
wxEND_EVENT_TABLE()

AIAnalysisPanel::AIAnalysisPanel(wxWindow* parent, WaveformPanel* wavePanel)
    : wxPanel(parent)
    , m_wavePanel(wavePanel)
{
    m_apiKey.clear();

    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    wxStaticText* apiKeyLabel = new wxStaticText(this, wxID_ANY, wxT("API Key (DeepSeek):"));
    m_apiKeyText = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxDefaultSize, wxTE_PASSWORD);
    m_setApiKeyBtn = new wxButton(this, 3, wxT("Save Key"));

    wxBoxSizer* apiKeySizer = new wxBoxSizer(wxHORIZONTAL);
    apiKeySizer->Add(apiKeyLabel, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    apiKeySizer->Add(m_apiKeyText, 1, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    apiKeySizer->Add(m_setApiKeyBtn, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    wxStaticText* tplLabel = new wxStaticText(this, wxID_ANY, wxT("Template:"));
    m_templateCombo = new wxComboBox(this, 8, wxT("General anomaly scan"),
        wxDefaultPosition, wxDefaultSize, 0, nullptr, wxCB_READONLY);
    m_templateCombo->Append(wxT("General anomaly scan"));
    m_templateCombo->Append(wxT("Reset & clock sequencing"));
    m_templateCombo->Append(wxT("Bus / handshake"));
    m_templateCombo->Append(wxT("Per-signal activity summary"));
    m_templateCombo->SetSelection(0);

    wxStaticText* inputLabel = new wxStaticText(this, wxID_ANY, wxT("Analysis request:"));
    m_analysisInput = new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(kTemplatePrompts[0]),
        wxDefaultPosition, wxSize(400, 90), wxTE_MULTILINE);

    m_hintLabel = new wxStaticText(this, wxID_ANY,
        wxT("Signals: wave panel only. Add traces from the tree, then Refresh."));

    wxStaticText* signalSelectLabel = new wxStaticText(this, wxID_ANY, wxT("Signals to analyze:"));
    m_signalList = new wxCheckListBox(this, wxID_ANY, wxDefaultPosition, wxSize(400, 140));

    wxBoxSizer* selectButtonsSizer = new wxBoxSizer(wxHORIZONTAL);
    m_selectAllBtn = new wxButton(this, 5, wxT("All"));
    m_selectNoneBtn = new wxButton(this, 6, wxT("None"));
    m_refreshSignalsBtn = new wxButton(this, 7, wxT("Refresh from wave"));
    selectButtonsSizer->Add(m_selectAllBtn, 0, wxALL, 5);
    selectButtonsSizer->Add(m_selectNoneBtn, 0, wxALL, 5);
    selectButtonsSizer->Add(m_refreshSignalsBtn, 0, wxALL, 5);

    m_analyzeBtn = new wxButton(this, 1, wxT("Analyze"));

    wxStaticText* outputLabel = new wxStaticText(this, wxID_ANY, wxT("Result:"));
    m_analysisOutput = new wxTextCtrl(this, wxID_ANY, wxT(""), wxDefaultPosition, wxSize(400, 220),
        wxTE_MULTILINE | wxTE_READONLY | wxTE_WORDWRAP);

    m_exportBtn = new wxButton(this, 2, wxT("Export report"));

    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    buttonSizer->Add(m_analyzeBtn, 0, wxALL, 5);

    mainSizer->Add(apiKeySizer, 0, wxEXPAND | wxALL, 5);
    mainSizer->Add(tplLabel, 0, wxALL, 5);
    mainSizer->Add(m_templateCombo, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 5);
    mainSizer->Add(inputLabel, 0, wxALL, 5);
    mainSizer->Add(m_analysisInput, 0, wxEXPAND | wxALL, 5);
    mainSizer->Add(m_hintLabel, 0, wxLEFT | wxRIGHT | wxBOTTOM, 5);
    mainSizer->Add(signalSelectLabel, 0, wxALL, 5);
    mainSizer->Add(m_signalList, 0, wxEXPAND | wxALL, 5);
    mainSizer->Add(selectButtonsSizer, 0, wxALL, 5);
    mainSizer->Add(buttonSizer, 0, wxALL, 5);
    mainSizer->Add(outputLabel, 0, wxALL, 5);
    mainSizer->Add(m_analysisOutput, 1, wxEXPAND | wxALL, 5);
    mainSizer->Add(m_exportBtn, 0, wxALL, 5);

    SetSizer(mainSizer);
    Layout();
}

AIAnalysisPanel::~AIAnalysisPanel() = default;

void AIAnalysisPanel::SetApiKey(const wxString& apiKey)
{
    m_apiKey = apiKey;
    m_apiKeyText->SetValue(apiKey);
}

wxString AIAnalysisPanel::GetApiKey() const
{
    return m_apiKeyText->GetValue();
}

void AIAnalysisPanel::RefreshSignalList(const std::vector<signal_t*>& signals, bool checkAll)
{
    m_signalList->Clear();
    m_indexToSignal.clear();

    int index = 0;
    for (signal_t* sig : signals) {
        if (!sig)
            continue;
        const wxString name = wxString::Format(
            wxT("%s  [%s, %u bits]"),
            wxString::FromUTF8(sig->full_name[0] ? sig->full_name : sig->name),
            wxString::FromUTF8(sig->module_path),
            (unsigned)sig->size);
        m_signalList->Append(name);
        m_indexToSignal[index++] = sig;
        if (checkAll)
            m_signalList->Check(m_signalList->GetCount() - 1);
    }

    if (m_signalList->GetCount() == 0) {
        m_hintLabel->SetLabel(wxT("No signals in wave panel — double-click signals in the tree to add them."));
    } else {
        m_hintLabel->SetLabel(wxString::Format(
            wxT("%d signal(s) from wave panel. Uncheck any you want to exclude."),
            (int)m_signalList->GetCount()));
    }
}

void AIAnalysisPanel::SetDisplayedSignals(const std::vector<signal_t*>& signals, bool checkAll)
{
    RefreshSignalList(signals, checkAll);
}

void AIAnalysisPanel::SetSignalInfo(const std::vector<signal_t*>& signals)
{
    SetDisplayedSignals(signals, false);
}

std::vector<signal_t*> AIAnalysisPanel::GetSelectedSignals() const
{
    std::vector<signal_t*> out;
    for (int i = 0; i < m_signalList->GetCount(); ++i) {
        if (!m_signalList->IsChecked(i))
            continue;
        auto it = m_indexToSignal.find(i);
        if (it != m_indexToSignal.end() && it->second)
            out.push_back(it->second);
    }
    return out;
}

void AIAnalysisPanel::OnTemplatePick(wxCommandEvent&)
{
    const int sel = m_templateCombo->GetSelection();
    if (sel >= 0 && sel < 4)
        m_analysisInput->SetValue(wxString::FromUTF8(kTemplatePrompts[sel]));
}

void AIAnalysisPanel::OnSelectAll(wxCommandEvent&)
{
    for (unsigned i = 0; i < m_signalList->GetCount(); ++i)
        m_signalList->Check(i);
}

void AIAnalysisPanel::OnSelectNone(wxCommandEvent&)
{
    for (unsigned i = 0; i < m_signalList->GetCount(); ++i)
        m_signalList->Check(i, false);
}

void AIAnalysisPanel::OnRefreshSignals(wxCommandEvent&)
{
    if (!m_wavePanel) {
        RefreshSignalList({}, true);
        return;
    }
    SetDisplayedSignals(m_wavePanel->m_displayedSignals2, true);
}

void AIAnalysisPanel::OnAnalyze(wxCommandEvent&)
{
    m_analysisOutput->SetValue(wxT("Analyzing… please wait."));
    m_analyzeBtn->Disable();

    wxString analysisRequest = m_analysisInput->GetValue();
    const wxString apiKeyWx = m_apiKeyText->GetValue();
    const std::string apiKey(apiKeyWx.utf8_string());

    if (WaveformAnalysis::IsPlaceholderApiKey(apiKey)) {
        m_analysisOutput->SetValue(wxT("Error: enter a valid DeepSeek API key and click Save Key."));
        m_analyzeBtn->Enable();
        return;
    }

    std::vector<signal_t*> selected = GetSelectedSignals();
    if (selected.empty()) {
        m_analysisOutput->SetValue(wxT("Error: select at least one signal (or Refresh from wave panel)."));
        m_analyzeBtn->Enable();
        return;
    }

    if (!m_wavePanel || !m_wavePanel->m_vcdData) {
        m_analysisOutput->SetValue(wxT("Error: open a trace file in the main window first."));
        m_analyzeBtn->Enable();
        return;
    }

    WaveformAnalysis::PrepareSignals(m_wavePanel, selected);
    const std::string ctx = WaveformAnalysis::BuildContext(m_wavePanel, selected);

    wxString fullRequest = analysisRequest + wxT("\n\n") + wxString::FromUTF8(ctx);

    wxString url = wxT("https://api.deepseek.com/v1/chat/completions");
    wxString escapedContent = fullRequest;
    escapedContent.Replace("\\", "\\\\");
    escapedContent.Replace("\"", "\\\"");
    escapedContent.Replace("\n", "\\n");
    escapedContent.Replace("\r", "\\r");
    escapedContent.Replace("\t", "\\t");

    wxString cleanContent;
    for (size_t i = 0; i < escapedContent.length(); i++) {
        const wchar_t ch = escapedContent[i];
        if (ch >= 32 || ch == '\n' || ch == '\r' || ch == '\t')
            cleanContent += ch;
    }

    wxString payload = wxString::Format(
        wxT("{\"model\":\"deepseek-chat\",\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}],\"temperature\":0.4}"),
        cleanContent);

    HINTERNET hInternet = nullptr;
    HINTERNET hConnect = nullptr;
    HINTERNET hRequest = nullptr;
    DWORD dwBytesRead = 0;
    char buffer[4096] = {};
    wxString responseContent;

    try {
        hInternet = InternetOpen(L"Bear2Wave", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
        if (!hInternet)
            throw std::runtime_error("WinINet init failed");

        hConnect = InternetConnect(hInternet, L"api.deepseek.com", INTERNET_DEFAULT_HTTPS_PORT,
            nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
        if (!hConnect)
            throw std::runtime_error("connect failed");

        hRequest = HttpOpenRequest(hConnect, L"POST", L"/v1/chat/completions", nullptr, nullptr, nullptr,
            INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD, 0);
        if (!hRequest)
            throw std::runtime_error("request failed");

        const wxString headers = wxString::Format(
            wxT("Content-Type: application/json\r\nAuthorization: Bearer %s\r\n"),
            apiKeyWx);
        if (!HttpAddRequestHeaders(hRequest, headers.wc_str(), (DWORD)-1L, HTTP_ADDREQ_FLAG_ADD))
            throw std::runtime_error("headers failed");

        const std::string payloadUtf8(payload.utf8_string());
        if (!HttpSendRequestA(hRequest, nullptr, 0, (LPVOID)payloadUtf8.data(), (DWORD)payloadUtf8.size()))
            throw std::runtime_error("send failed");

        while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &dwBytesRead) && dwBytesRead > 0) {
            buffer[dwBytesRead] = '\0';
            responseContent += wxString::FromUTF8(buffer);
        }
    } catch (const std::exception& e) {
        if (hRequest) InternetCloseHandle(hRequest);
        if (hConnect) InternetCloseHandle(hConnect);
        if (hInternet) InternetCloseHandle(hInternet);
        m_analysisOutput->SetValue(wxString::Format(wxT("Error: %s"), wxString::FromUTF8(e.what())));
        m_analyzeBtn->Enable();
        return;
    }

    if (hRequest) InternetCloseHandle(hRequest);
    if (hConnect) InternetCloseHandle(hConnect);
    if (hInternet) InternetCloseHandle(hInternet);

    if (responseContent.IsEmpty()) {
        m_analysisOutput->SetValue(wxT("Error: empty response from API."));
        m_analyzeBtn->Enable();
        return;
    }

    if (responseContent.Contains(wxT("\"error\""))) {
        m_analysisOutput->SetValue(wxT("API error:\n\n") + responseContent);
        m_analyzeBtn->Enable();
        return;
    }

    wxString content;
    size_t contentStart = responseContent.Find(wxT("\"content\":"));
    if (contentStart != wxString::npos) {
        contentStart = responseContent.Find('"', contentStart + 10);
        if (contentStart != wxString::npos) {
            ++contentStart;
            size_t contentEnd = responseContent.length();
            for (int i = (int)contentEnd - 1; i >= (int)contentStart; --i) {
                if (responseContent[(size_t)i] == '"') {
                    contentEnd = (size_t)i;
                    break;
                }
            }
            if (contentEnd > contentStart) {
                content = responseContent.Mid(contentStart, contentEnd - contentStart);
                content.Replace("\\n", "\n");
                content.Replace("\\\"", "\"");
                content.Replace("\\r", "\r");
                content.Replace("\\t", "\t");
            }
        }
    }

    if (content.IsEmpty())
        m_analysisOutput->SetValue(wxT("Could not parse response:\n\n") + responseContent);
    else
        m_analysisOutput->SetValue(content);

    m_analyzeBtn->Enable();
}

void AIAnalysisPanel::OnExport(wxCommandEvent&)
{
    wxFileDialog saveDialog(this, wxT("Export analysis"), wxT(""), wxT("analysis.txt"),
        wxT("Text files (*.txt)|*.txt"), wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (saveDialog.ShowModal() == wxID_CANCEL)
        return;

    std::ofstream file(saveDialog.GetPath().ToStdString());
    if (!file.is_open()) {
        wxMessageBox(wxT("Failed to open file for writing."));
        return;
    }

    time_t now = time(nullptr);
    struct tm timeinfo_buf;
#ifdef _WIN32
    localtime_s(&timeinfo_buf, &now);
#else
    localtime_r(&now, &timeinfo_buf);
#endif
    char timestamp[80];
    strftime(timestamp, sizeof(timestamp), "%Y-%m-%d %H:%M:%S", &timeinfo_buf);

    file << "Bear2Wave AI Analysis Report\n";
    file << "Generated: " << timestamp << "\n\n";
    file << "Request:\n" << m_analysisInput->GetValue().ToUTF8().data() << "\n\n";
    file << "Signals:\n";
    for (int i = 0; i < m_signalList->GetCount(); ++i) {
        if (!m_signalList->IsChecked(i))
            continue;
        auto it = m_indexToSignal.find(i);
        if (it != m_indexToSignal.end() && it->second) {
            file << "  - " << it->second->full_name << " (" << it->second->module_path << ")\n";
        }
    }
    file << "\nResult:\n" << m_analysisOutput->GetValue().ToUTF8().data() << "\n";
    file.close();
    wxMessageBox(wxT("Report exported (API key not included)."));
}

void AIAnalysisPanel::OnSetApiKey(wxCommandEvent&)
{
    m_apiKey = m_apiKeyText->GetValue();
    wxMessageBox(wxT("API key saved for this session."));
}
