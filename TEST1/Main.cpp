#include <wx/wx.h>
#include <wx/filedlg.h>
#include <wx/listctrl.h>
#include <wx/timer.h>
#include <wx/tokenzr.h>
#include <wx/dc.h>
#include <wx/utils.h>
#include <wx/graphics.h>
#include <wx/dcbuffer.h>
#include <wx/treectrl.h>
#include <wx/sizer.h>
#include <wx/toolbar.h>
#include <wx/artprov.h>
#include <wx/treelist.h>
#include <wx/splitter.h>
#include <wx/valnum.h>
#include <wx/menu.h>
#include <wx/regex.h>
#include <wx/filename.h>
#include <wx/file.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/checkbox.h>
#include <windows.h>
#include <wininet.h>
#include <sstream>
#include <string>
#include <wx/glcanvas.h>
#include "csv.h"
#include "vcd.h"
#include <gl/GL.h>
#include <gl/GLU.h>

#include <cstring>
#include <string>
#include <unordered_set>
#include <algorithm>
#include <map>
#include <random>
#include <vector>
#include <thread>
#include <mutex>
#include <cmath>
#include <math.h>
#include <functional>
#include <climits>
#include <fstream>
#include <wx/filefn.h>
#include "ProjectStartWindow.h"
extern "C" {
#include "vcd.h"
}


#define SIGNAL_ROW_HEIGHT 60
#define LEFT_MARGIN 180
#define WAVE_PADDING 40

void split_module_path(const char* full_path, std::vector<std::string>& out_parts);
void FreeSignalTree(SignalGroup* root);

// 前向声明
class WaveformPanel;

class AIAnalysisPanel : public wxPanel
{
public:
    AIAnalysisPanel(wxWindow* parent, WaveformPanel* wavePanel);
    ~AIAnalysisPanel();

    void SetApiKey(const wxString& apiKey);
    wxString GetApiKey() const;
    void SetSignalInfo(const std::vector<signal_t*>& signals);

private:
    WaveformPanel* m_wavePanel;
    wxTextCtrl* m_apiKeyText;
    wxButton* m_setApiKeyBtn;
    wxTextCtrl* m_analysisInput;
    wxButton* m_analyzeBtn;
    wxTextCtrl* m_analysisOutput;
    wxCheckListBox* m_signalList;
    wxButton* m_selectAllBtn;
    wxButton* m_selectNoneBtn;
    wxButton* m_exportBtn;

    wxString m_apiKey;
    std::vector<signal_t*> m_signals;
    std::unordered_map<int, signal_t*> m_indexToSignal;

    void OnAnalyze(wxCommandEvent& event);
    void OnExport(wxCommandEvent& event);
    void OnSetApiKey(wxCommandEvent& event);
    void OnLoadVcdFile(wxCommandEvent& event);
    void GetSignalInfo();

    wxDECLARE_EVENT_TABLE();
};

wxBEGIN_EVENT_TABLE(AIAnalysisPanel, wxPanel)
    EVT_BUTTON(1, AIAnalysisPanel::OnAnalyze)
    EVT_BUTTON(2, AIAnalysisPanel::OnExport)
    EVT_BUTTON(3, AIAnalysisPanel::OnSetApiKey)
    EVT_BUTTON(4, AIAnalysisPanel::OnLoadVcdFile)
wxEND_EVENT_TABLE()

AIAnalysisPanel::AIAnalysisPanel(wxWindow* parent, WaveformPanel* wavePanel) : wxPanel(parent)
{
    m_wavePanel = wavePanel;
    m_apiKey = "sk-8801be45326a4776ac37f3b120ee1888";

    // 创建垂直sizer
    wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);

    // API Key 部分
    wxStaticText* apiKeyLabel = new wxStaticText(this, wxID_ANY, "API Key:");
    m_apiKeyText = new wxTextCtrl(this, wxID_ANY, m_apiKey, wxDefaultPosition, wxSize(300, 25));
    m_setApiKeyBtn = new wxButton(this, 3, "Set API Key");

    wxBoxSizer* apiKeySizer = new wxBoxSizer(wxHORIZONTAL);
    apiKeySizer->Add(apiKeyLabel, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    apiKeySizer->Add(m_apiKeyText, 1, wxALL | wxALIGN_CENTER_VERTICAL, 5);
    apiKeySizer->Add(m_setApiKeyBtn, 0, wxALL | wxALIGN_CENTER_VERTICAL, 5);

    // 分析输入部分
    wxStaticText* inputLabel = new wxStaticText(this, wxID_ANY, "Analysis Request:");
    m_analysisInput = new wxTextCtrl(this, wxID_ANY, "Analyze the waveform and identify any anomalies or patterns.", wxDefaultPosition, wxSize(400, 100), wxTE_MULTILINE);

    // 信号选择部分
    wxStaticText* signalSelectLabel = new wxStaticText(this, wxID_ANY, "Select Signals for Analysis:");
    m_signalList = new wxCheckListBox(this, wxID_ANY, wxDefaultPosition, wxSize(400, 150));

    // 全选和全不选按钮
    wxBoxSizer* selectButtonsSizer = new wxBoxSizer(wxHORIZONTAL);
    m_selectAllBtn = new wxButton(this, 5, "Select All");
    m_selectNoneBtn = new wxButton(this, 6, "Select None");
    selectButtonsSizer->Add(m_selectAllBtn, 0, wxALL, 5);
    selectButtonsSizer->Add(m_selectNoneBtn, 0, wxALL, 5);

    // 添加VCD文件按钮
    wxButton* loadVcdBtn = new wxButton(this, 4, "Load VCD File");

    // 分析按钮
    m_analyzeBtn = new wxButton(this, 1, "Analyze");

    // 分析输出部分
    wxStaticText* outputLabel = new wxStaticText(this, wxID_ANY, "Analysis Result:");
    m_analysisOutput = new wxTextCtrl(this, wxID_ANY, "", wxDefaultPosition, wxSize(400, 200), wxTE_MULTILINE | wxTE_READONLY);

    // 导出按钮
    m_exportBtn = new wxButton(this, 2, "Export Analysis");

    // 创建按钮sizer
    wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
    buttonSizer->Add(loadVcdBtn, 0, wxALL, 5);
    buttonSizer->Add(m_analyzeBtn, 0, wxALL, 5);

    // 添加所有控件到主sizer
    mainSizer->Add(apiKeySizer, 0, wxEXPAND | wxALL, 5);
    mainSizer->Add(inputLabel, 0, wxALL, 5);
    mainSizer->Add(m_analysisInput, 0, wxEXPAND | wxALL, 5);
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

AIAnalysisPanel::~AIAnalysisPanel()
{
}

void AIAnalysisPanel::SetApiKey(const wxString& apiKey)
{
    m_apiKey = apiKey;
    m_apiKeyText->SetValue(apiKey);
}

wxString AIAnalysisPanel::GetApiKey() const
{
    return m_apiKey;
}

void AIAnalysisPanel::SetSignalInfo(const std::vector<signal_t*>& signals)
{
    m_signals = signals;
    
    // 清空信号列表
    m_signalList->Clear();
    m_indexToSignal.clear();
    
    // 添加信号到列表
    int index = 0;
    for (const auto& signal : signals)
    {
        wxString signalName = wxString::Format("%s (Module: %s, Size: %u)", 
            wxString(signal->name), wxString(signal->module_path), (unsigned int)signal->size);
        m_signalList->Append(signalName);
        m_indexToSignal[index] = signal;
        index++;
    }
}

void AIAnalysisPanel::OnAnalyze(wxCommandEvent& event)
{
    // 显示加载状态
    m_analysisOutput->SetValue("AI Analysis Result:\n\nAnalyzing... Please wait...");
    
    // 获取分析请求和API Key
    wxString analysisRequest = m_analysisInput->GetValue();
    wxString apiKey = m_apiKeyText->GetValue();
    
    if (apiKey == "your-deepseek-api-key-here" || apiKey.IsEmpty())
    {
        m_analysisOutput->SetValue("AI Analysis Result:\n\nError: Please set your DeepSeek API Key first!");
        return;
    }
    
    // 获取选中的信号
    std::vector<signal_t*> selectedSignals;
    for (int i = 0; i < m_signalList->GetCount(); i++)
    {
        if (m_signalList->IsChecked(i))
        {
            auto it = m_indexToSignal.find(i);
            if (it != m_indexToSignal.end())
            {
                selectedSignals.push_back(it->second);
            }
        }
    }
    
    if (selectedSignals.empty())
    {
        m_analysisOutput->SetValue("AI Analysis Result:\n\nError: Please select at least one signal for analysis!");
        return;
    }
    
    // 构建信号信息
    wxString signalInfo;
    signalInfo += "Selected Signals:\n";
    for (const auto& signal : selectedSignals)
    {
        signalInfo += wxString::Format("- Signal: %s, Module: %s, Size: %u\n", 
            wxString(signal->name), wxString(signal->module_path), (unsigned int)signal->size);
    }
    
    // 构建完整的分析请求
    wxString fullRequest = analysisRequest + "\n\n" + signalInfo;
    
    // 构建API请求
    wxString url = "https://api.deepseek.com/v1/chat/completions";
    wxString escapedContent = fullRequest;
    // 更全面的字符转义处理
    escapedContent.Replace("\\", "\\\\"); // 首先转义反斜杠
    escapedContent.Replace("\"", "\\\""); // 转义双引号
    escapedContent.Replace("\n", "\\n"); // 转义换行符
    escapedContent.Replace("\r", "\\r"); // 转义回车符
    escapedContent.Replace("\t", "\\t"); // 转义制表符
    // 移除所有控制字符 (ASCII 0-31)
    wxString cleanContent;
    for (size_t i = 0; i < escapedContent.length(); i++)
    {
        wchar_t ch = escapedContent[i];
        if (ch >= 32 || ch == '\n' || ch == '\r' || ch == '\t')
        {
            cleanContent += ch;
        }
    }
    wxString payload = wxString::Format(
        "{\"model\":\"deepseek-chat\",\"messages\":[{\"role\":\"user\",\"content\":\"%s\"}],\"temperature\":0.7}",
        cleanContent.c_str()
    );
    
    // 使用WinINet API发送请求
    HINTERNET hInternet = NULL;
    HINTERNET hConnect = NULL;
    HINTERNET hRequest = NULL;
    DWORD dwBytesRead = 0;
    char buffer[4096] = {0};
    wxString responseContent;
    
    try
    {
        // 初始化WinINet
        hInternet = InternetOpen(L"WaveformAnalyzer", INTERNET_OPEN_TYPE_PRECONFIG, NULL, NULL, 0);
        if (hInternet == NULL)
        {
            throw std::exception("Failed to initialize WinINet");
        }
        
        // 打开连接
        hConnect = InternetConnect(hInternet, L"api.deepseek.com", INTERNET_DEFAULT_HTTPS_PORT, NULL, NULL, INTERNET_SERVICE_HTTP, 0, 0);
        if (hConnect == NULL)
        {
            throw std::exception("Failed to connect to server");
        }
        
        // 创建HTTP请求
        hRequest = HttpOpenRequest(hConnect, L"POST", L"/v1/chat/completions", NULL, NULL, NULL, INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD, 0);
        if (hRequest == NULL)
        {
            throw std::exception("Failed to create HTTP request");
        }
        
        // 设置请求头
        wxString headers = wxString::Format("Content-Type: application/json\r\nAuthorization: Bearer %s\r\nContent-Length: %zu\r\n", apiKey.c_str(), payload.length());
        if (!HttpAddRequestHeaders(hRequest, headers.wc_str(), headers.length(), HTTP_ADDREQ_FLAG_ADD))
        {
            throw std::exception("Failed to add request headers");
        }
        
        // 发送请求
        if (!HttpSendRequest(hRequest, NULL, 0, (LPVOID)(const char*)payload.c_str(), payload.length()))
        {
            throw std::exception("Failed to send request");
        }
        
        // 读取响应
        while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &dwBytesRead) && dwBytesRead > 0)
        {
            buffer[dwBytesRead] = '\0';
            responseContent += wxString::FromUTF8(buffer);
        }
        
        // 关闭连接
        if (hRequest) InternetCloseHandle(hRequest);
        if (hConnect) InternetCloseHandle(hConnect);
        if (hInternet) InternetCloseHandle(hInternet);
        
        // 处理响应
        if (responseContent.IsEmpty())
        {
            m_analysisOutput->SetValue("AI Analysis Result:\n\nError: No response received");
            return;
        }
        
        // 简单的字符串提取方法
        wxString content;
        wxString errorMessage;
        
        // 检查是否有错误
        if (responseContent.Contains("error"))
        {
            // 提取错误信息 - 尝试多种格式
            size_t errorStart = responseContent.Find("message:");
            if (errorStart == wxString::npos)
            {
                errorStart = responseContent.Find("\"message\":");
                if (errorStart != wxString::npos)
                {
                    errorStart += 10; // 跳过 "message":" 部分
                }
            }
            else
            {
                errorStart += 9; // 跳过 "message:" 部分
            }
            
            if (errorStart != wxString::npos)
            {
                // 找到下一个双引号
                size_t errorEnd = responseContent.Find('"', errorStart);
                if (errorEnd == wxString::npos)
                {
                    // 尝试找到下一个逗号或右花括号
                    size_t commaPos = responseContent.Find(',', errorStart);
                    size_t bracePos = responseContent.Find('}', errorStart);
                    if (commaPos != wxString::npos && bracePos != wxString::npos)
                    {
                        errorEnd = std::min(commaPos, bracePos);
                    }
                    else if (commaPos != wxString::npos)
                    {
                        errorEnd = commaPos;
                    }
                    else if (bracePos != wxString::npos)
                    {
                        errorEnd = bracePos;
                    }
                    else
                    {
                        errorEnd = responseContent.length();
                    }
                }
                
                if (errorEnd != wxString::npos && errorEnd > errorStart)
                {
                    errorMessage = responseContent.Mid(errorStart, errorEnd - errorStart);
                    // 去除前后空格
                    errorMessage.Trim(true).Trim(false);
                    m_analysisOutput->SetValue("AI Analysis Result:\n\nError: " + errorMessage);
                }
                else
                {
                    // 显示完整的错误响应以便调试
                    m_analysisOutput->SetValue("AI Analysis Result:\n\nError: Invalid error format\n\nResponse: " + responseContent);
                }
            }
            else
            {
                // 显示完整的错误响应以便调试
                m_analysisOutput->SetValue("AI Analysis Result:\n\nError: Error in response\n\nResponse: " + responseContent);
            }
        }
        else
        {
            // 提取content内容 - 尝试多种格式
            size_t contentStart = responseContent.Find("content:");
            if (contentStart == wxString::npos)
            {
                contentStart = responseContent.Find("\"content\":");
                if (contentStart != wxString::npos)
                {
                    contentStart += 10; // 跳过 "content":" 部分
                }
            }
            else
            {
                contentStart += 9; // 跳过 "content:" 部分
            }
            
            if (contentStart != wxString::npos)
            {
                // 从后向前查找最后一个双引号
                size_t contentEnd = responseContent.length();
                for (int i = contentEnd - 1; i >= contentStart; i--)
                {
                    if (responseContent[i] == '"')
                    {
                        contentEnd = i;
                        break;
                    }
                }
                
                if (contentEnd > contentStart)
                {
                    content = responseContent.Mid(contentStart, contentEnd - contentStart);
                    // 处理转义字符
                    content.Replace("\\n", "\n");
                    content.Replace("\\\"", "\"");
                    content.Replace("\\r", "\r");
                    content.Replace("\\t", "\t");
                    m_analysisOutput->SetValue("AI Analysis Result:\n\n" + content);
                }
                else
                {
                    // 显示完整的响应以便调试
                    m_analysisOutput->SetValue("AI Analysis Result:\n\nError: No content found in response\n\nResponse: " + responseContent);
                }
            }
            else
            {
                // 显示完整的响应以便调试
                m_analysisOutput->SetValue("AI Analysis Result:\n\nError: Invalid response format\n\nResponse: " + responseContent);
            }
        }
    }
    catch (const std::exception& e)
    {
        // 关闭连接
        if (hRequest) InternetCloseHandle(hRequest);
        if (hConnect) InternetCloseHandle(hConnect);
        if (hInternet) InternetCloseHandle(hInternet);
        
        m_analysisOutput->SetValue(wxString::Format("AI Analysis Result:\n\nError: %s", e.what()));
    }
}

void AIAnalysisPanel::OnExport(wxCommandEvent& event)
{
    wxFileDialog saveDialog(this, "Export Analysis", "", "analysis.txt", "Text files (*.txt)|*.txt|All files (*.*)|*.*", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (saveDialog.ShowModal() == wxID_CANCEL)
        return;

    wxString filePath = saveDialog.GetPath();
    std::ofstream file(filePath.ToStdString());
    if (file.is_open())
    {
        // 添加报告标题和时间
        time_t now = time(0);
        struct tm* timeinfo = localtime(&now);
        char timestamp[80];
        strftime(timestamp, 80, "%Y-%m-%d %H:%M:%S", timeinfo);
        file << "AI Analysis Report\n";
        file << "==================\n";
        file << "Generated on: " << timestamp << "\n\n";
        
        file << "API Key: " << m_apiKey.ToStdString() << "\n\n";
        file << "Analysis Request:\n" << m_analysisInput->GetValue().ToStdString() << "\n\n";
        
        // 添加选中的信号
        file << "Selected Signals:\n";
        file << "-----------------\n";
        bool hasSelectedSignals = false;
        for (int i = 0; i < m_signalList->GetCount(); i++)
        {
            if (m_signalList->IsChecked(i))
            {
                auto it = m_indexToSignal.find(i);
                if (it != m_indexToSignal.end())
                {
                    signal_t* signal = it->second;
                    file << "- Signal: " << signal->name << "\n";
                    file << "  Module: " << signal->module_path << "\n";
                    file << "  Size: " << signal->size << "\n\n";
                    hasSelectedSignals = true;
                }
            }
        }
        if (!hasSelectedSignals)
        {
            file << "No signals selected\n\n";
        }
        
        file << "Analysis Result:\n" << m_analysisOutput->GetValue().ToStdString() << "\n";
        file.close();
        wxMessageBox("Analysis exported successfully!");
    }
    else
    {
        wxMessageBox("Failed to export analysis!");
    }
}

void AIAnalysisPanel::OnSetApiKey(wxCommandEvent& event)
{
    m_apiKey = m_apiKeyText->GetValue();
    wxMessageBox("API Key set successfully!");
}

void AIAnalysisPanel::OnLoadVcdFile(wxCommandEvent& event)
{
    // 打开VCD文件选择对话框
    wxFileDialog openDialog(this, "Open VCD File", "", "", "VCD files (*.vcd)|*.vcd|All files (*.*)|*.*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
    if (openDialog.ShowModal() == wxID_CANCEL)
        return;

    wxString filePath = openDialog.GetPath();
    
    // 检查文件大小
    wxFile fileForSizeCheck(filePath);
    if (fileForSizeCheck.IsOpened())
    {
        // 限制文件大小为5MB
        wxFileOffset fileSize = fileForSizeCheck.Length();
        if (fileSize > 5 * 1024 * 1024)
        {
            fileForSizeCheck.Close();
            wxMessageBox("VCD file too large! Please use files smaller than 5MB.");
            return;
        }
        fileForSizeCheck.Close();
    }

    wxFile file(filePath);
    if (!file.IsOpened())
    {
        wxMessageBox("Failed to open VCD file!");
        return;
    }

    try
    {
        // 读取VCD文件内容
        wxString vcdContent;
        if (!file.ReadAll(&vcdContent))
        {
            wxMessageBox("Failed to read VCD file!");
            file.Close();
            return;
        }
        file.Close();

        // 限制内容长度
        const size_t MAX_CONTENT_LENGTH = 100000; // 100,000 characters
        if (vcdContent.length() > MAX_CONTENT_LENGTH)
        {
            vcdContent = vcdContent.SubString(0, MAX_CONTENT_LENGTH) + "\n... (truncated)";
        }

        // 将VCD内容添加到分析请求中
        wxString currentRequest = m_analysisInput->GetValue();
        wxString newRequest = currentRequest + "\n\nVCD File Content:\n" + vcdContent;
        
        // 限制总请求长度
        const size_t MAX_REQUEST_LENGTH = 120000; // 120,000 characters
        if (newRequest.length() > MAX_REQUEST_LENGTH)
        {
            newRequest = newRequest.SubString(0, MAX_REQUEST_LENGTH) + "\n... (truncated)";
        }
        
        m_analysisInput->SetValue(newRequest);

        wxMessageBox("VCD file loaded successfully!");
    }
    catch (const std::exception& e)
    {
        file.Close();
        wxMessageBox(wxString::Format("Error loading VCD file: %s", e.what()));
    }
    catch (...)
    {
        file.Close();
        wxMessageBox("Unknown error loading VCD file!");
    }
}

class WaveformPanel : public wxGLCanvas
{
public:
    struct DrawSegment
    {
        int x1, x2;
        int y;
        char value;
        std::string text;
    };

    enum MeasureMode
    {
        MEASURE_NONE,
        MEASURE_FREQ,
        MEASURE_DUTY
    };

    MeasureMode m_measureMode = MEASURE_NONE;

    std::vector<int> m_cursorPositions;
    double m_dragRemainder = 0.0;
    int m_timeOffset = 0;
    bool m_showCursorValue = false;
    bool m_isDragging = false;
    int m_lastMouseX = 0;
    bool m_isSelecting = false;
    int m_selectStartX = 0;
    int m_selectEndX = 0;
    int m_hoverSignal = -1;
    int m_hoverSegment = -1;
    int m_draggingMarkerIndex = -1;
    int m_hoverMarkerIndex = -1;
    int m_draggingMarkerStartX = 0;

    int m_markerA = -1;
    int m_markerB = -1;
    bool m_hasMarkerA = false;
    bool m_isMeasuring = false;

    bool m_isEditingMarker = false;
    int m_editingMarkerIndex = -1;

    wxRect m_minimapRect;
    bool m_draggingMinimap = false;

    int m_limitStart = 0;
    int m_limitEnd = -1;
    bool m_hasLimit = false;

    // 获取信号在指定时间的值
    std::string GetValueAt(signal_t* sig, int ts) const
    {
        if (!sig || sig->changes_count == 0 || !sig->value_changes) return "";
        bool isBus = (sig->size > 1);
        std::string lastText;
        char lastVal = '0';
        for (size_t c = 0; c < sig->changes_count; c++)
        {
            int t = sig->value_changes[c].timestamp;
            if (t > ts) break;
            if (isBus) lastText = ParseBusValue(sig->value_changes[c].value);
            else lastVal = ParseVcdValue(sig->value_changes[c].value);
        }
        return isBus ? lastText : std::string(1, lastVal);
    }

    std::unordered_set<std::string> m_searchMatchedSignals;
    std::string m_searchKeyword;

    wxString m_editingMarkerText;
    std::unordered_set<std::string> m_visibleSignals;

    std::mutex m_cacheMutex;
    std::vector<std::vector<DrawSegment>> m_cachedSegments;

    struct Marker
    {
        int timestamp;
        wxString label;
    };

    std::vector<Marker> m_markers;

    // 支持重复显示的信号列表（可以存同一个 signal_t* 多次）
    std::vector<signal_t*> m_displayedSignals2;
    
    // 剪贴板，用于存储复制/剪切的信号
    std::vector<signal_t*> m_clipboard;
    
    // 信号层次相关
    int m_traceMaxHier = 10; // 默认最大层次
    bool m_showTraceHier = true; // 默认显示层次
    
    // 注释结构
    struct Comment {
        int position; // 在信号列表中的位置
        wxString text; // 注释内容
    };
    
    // VCD数据
    vcd_data* m_vcdData;
    int m_currentTimestamp;
    int m_displayTimeRange;
    int m_maxTimestamp;
    
    // OpenGL上下文
    wxGLContext* m_glContext;
    
    // 随机数生成器
    std::mt19937 m_rng;
    std::vector<Comment> m_comments;
    
    // 选中的信号索引
    int m_selectedSignalIndex = -1;
    
    // 信号别名映射
    std::map<signal_t*, wxString> m_signalAliases;
    
    // 信号数据格式
    enum DataFormat {
        FORMAT_BINARY,
        FORMAT_OCTAL,
        FORMAT_DECIMAL,
        FORMAT_HEXADECIMAL,
        FORMAT_ASCII,
        FORMAT_SIGNED_DECIMAL,
        FORMAT_REAL
    };
    std::map<signal_t*, DataFormat> m_signalDataFormats;
    
    // 信号颜色格式
    enum ColorFormat {
        COLOR_DEFAULT,
        COLOR_SIGNAL_NAME,
        COLOR_VALUE,
        COLOR_MODULE
    };
    ColorFormat m_globalColorFormat = COLOR_DEFAULT;
    
    // 信号组合信息
    struct WaveformSignalGroup {
        std::vector<signal_t*> signals;
        wxString name;
        bool isExpanded;
    };
    std::vector<WaveformSignalGroup> m_signalGroups;
    
    // 高亮信号集合
    std::unordered_set<signal_t*> m_highlightedSignals;
    
    // 排除信号集合
    std::unordered_set<signal_t*> m_excludedSignals;
    
    // 时间扭曲设置
    bool m_timeWarpEnabled = false;
    double m_timeWarpFactor = 1.0;
    
    // 滚轮模式
    bool m_alternateWheelMode = false;
    
    // 波形滚动
    bool m_waveScrollingEnabled = false;
    
    // 模拟信号高度扩展
    int m_analogHeightExtension = 0;
    
    // 标记锁定
    bool m_markersLocked = false;
    
    // 获取当前时间
    int GetCurrentTime() const
    {
        return m_currentTimestamp;
    }

    // 添加信号（每次调用就加一次，允许重复）
    void AddDisplaySignal(signal_t* sig)
    {
        if (!sig) return;
        m_displayedSignals2.push_back(sig);
        AssignSignalColors();
        BuildDrawCacheAsync();
        Refresh();
    }

    // 清空显示列表
    void ClearDisplaySignals()
    {
        m_displayedSignals2.clear();
        m_signalColors.clear();
        {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            m_cachedSegments.clear();
        }
        Refresh();
    }

    void AssignSignalColors()
    {
        m_signalColors.clear();
        std::uniform_int_distribution<int> dist(0, 180);
        
        for (auto sig : m_allSignals)
        {
            wxColour color;
            switch (m_globalColorFormat)
            {
            case COLOR_DEFAULT:
                // 随机颜色
                color = wxColour(dist(m_rng), dist(m_rng), dist(m_rng));
                break;
            case COLOR_SIGNAL_NAME:
                // 根据信号名称生成颜色
                {   
                    int hash = 0;
                    for (const char* c = sig->name; *c; c++)
                        hash = hash * 31 + *c;
                    color = wxColour(abs(hash) % 180 + 75, abs(hash * 13) % 180 + 75, abs(hash * 7) % 180 + 75);
                }
                break;
            case COLOR_VALUE:
                // 根据信号值生成颜色
                color = wxColour(120, 120, 120); // 默认灰色
                if (sig->changes_count > 0)
                {
                    const char* value = sig->value_changes[0].value;
                    if (value[0] == '1')
                        color = wxColour(0, 180, 0); // 高电平绿色
                    else if (value[0] == '0')
                        color = wxColour(180, 0, 0); // 低电平红色
                }
                break;
            case COLOR_MODULE:
                // 根据模块路径生成颜色
                {   
                    int hash = 0;
                    for (const char* c = sig->module_path; *c; c++)
                        hash = hash * 31 + *c;
                    color = wxColour(abs(hash) % 180 + 75, abs(hash * 17) % 180 + 75, abs(hash * 11) % 180 + 75);
                }
                break;
            default:
                color = wxColour(120, 120, 120); // 默认灰色
                break;
            }
            m_signalColors[sig->signal_id] = color;
        }
    }

    void ClampViewToLimit()
    {
        if (!m_hasLimit) return;

        int limitRange = m_limitEnd - m_limitStart;

        if (m_displayTimeRange > limitRange)
            m_displayTimeRange = limitRange;

        if (m_timeOffset < m_limitStart)
            m_timeOffset = m_limitStart;

        if (m_timeOffset + m_displayTimeRange > m_limitEnd)
            m_timeOffset = m_limitEnd - m_displayTimeRange;

        if (m_timeOffset < m_limitStart)
            m_timeOffset = m_limitStart;
    }
    
    // 获取信号在指定时间的值
    const char* GetSignalValueAt(signal_t* sig, int timestamp)
    {
        if (!sig || sig->changes_count == 0)
            return "";
        
        return vcd_signal_get_value_at_timestamp(sig, timestamp);
    }

    int FindNextEdge(int t)
    {
        int best = m_maxTimestamp;
        for (auto sig : m_allSignals)
        {
            for (size_t i = 0; i < sig->changes_count; i++)
            {
                int ts = sig->value_changes[i].timestamp;
                if (ts > t && ts < best) best = ts;
            }
        }
        return best;
    }

    int FindPrevEdge(int t)
    {
        int best = 0;
        for (auto sig : m_allSignals)
        {
            for (size_t i = 0; i < sig->changes_count; i++)
            {
                int ts = sig->value_changes[i].timestamp;
                if (ts < t && ts > best) best = ts;
            }
        }
        return best;
    }

    void SearchSignals(const std::string& keyword)
    {
        m_searchKeyword = keyword;
        m_searchMatchedSignals.clear();
        if (keyword.empty()) { Refresh(); return; }

        std::string keyLower = keyword;
        std::transform(keyLower.begin(), keyLower.end(), keyLower.begin(), ::tolower);

        for (auto sig : m_allSignals)
        {
            std::string name = sig->full_name;
            std::string lower = name;
            std::transform(lower.begin(), lower.end(), lower.begin(), ::tolower);
            if (lower.find(keyLower) != std::string::npos)
            {
                m_searchMatchedSignals.insert(sig->signal_id);
            }
        }
        Refresh();
    }

    double ComputeFrequency(signal_t* sig)
    {
        if (!sig || sig->changes_count < 2) return 0;
        std::vector<int> risingEdges;
        for (size_t i = 1; i < sig->changes_count; i++)
        {
            char prev = ParseVcdValue(sig->value_changes[i - 1].value);
            char curr = ParseVcdValue(sig->value_changes[i].value);
            if (prev == '0' && curr == '1') risingEdges.push_back(sig->value_changes[i].timestamp);
        }
        if (risingEdges.size() < 2) return 0;
        double period = (double)(risingEdges.back() - risingEdges.front()) / (risingEdges.size() - 1);
        return period <= 0 ? 0 : 1.0 / period;
    }

    double ComputeDuty(signal_t* sig)
    {
        if (!sig || sig->changes_count < 2) return 0;
        double highTime = 0, totalTime = 0;
        for (size_t i = 1; i < sig->changes_count; i++)
        {
            int t1 = sig->value_changes[i - 1].timestamp;
            int t2 = sig->value_changes[i].timestamp;
            char val = ParseVcdValue(sig->value_changes[i - 1].value);
            if (val == '1') highTime += (t2 - t1);
            totalTime += (t2 - t1);
        }
        return totalTime <= 0 ? 0 : highTime / totalTime;
    }

    int FindNearestEdge(int targetTime)
    {
        if (!m_vcdData || m_allSignals.empty()) return targetTime;
        int best = targetTime;
        int minDist = INT_MAX;
        for (signal_t* sig : m_allSignals)
        {
            if (!sig || sig->changes_count == 0 || !sig->value_changes) continue;
            for (size_t c = 0; c < sig->changes_count; c++)
            {
                int t = sig->value_changes[c].timestamp;
                int d = abs(t - targetTime);
                if (d < minDist) { minDist = d; best = t; }
            }
        }
        return best;
    }

    void StartEditMarker(int index)
    {
        if (index < 0 || index >= (int)m_markers.size()) return;
        m_isEditingMarker = true;
        m_editingMarkerIndex = index;
        m_editingMarkerText = m_markers[index].label;
        Refresh();
    }

    void DeleteMarker(int index)
    {
        if (index < 0 || index >= (int)m_markers.size()) return;
        m_markers.erase(m_markers.begin() + index);
        m_hoverMarkerIndex = -1;
        Refresh();
    }

    void DrawMarkerMeasurementBar(wxAutoBufferedPaintDC& dc, wxSize& size, double scale)
    {
        if (m_markers.size() < 2) return;
        int yBar = 45, hBar = 22;
        dc.SetBrush(wxColour(240, 248, 255));
        dc.SetPen(wxPen(wxColour(100, 140, 200)));
        dc.DrawRectangle(LEFT_MARGIN, yBar - hBar / 2, size.x - LEFT_MARGIN - 20, hBar);

        wxFont font(9, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
        dc.SetFont(font);
        dc.SetTextForeground(wxColour(0, 80, 160));

        int lastT = m_markers[0].timestamp;
        for (size_t i = 1; i < m_markers.size(); i++)
        {
            int t = m_markers[i].timestamp;
            int delta = t - lastT;
            int x1 = TimeToX(lastT, scale);
            int x2 = TimeToX(t, scale);
            wxString s = wxString::Format("ΔT = %d", delta);
            wxSize sz = dc.GetTextExtent(s);
            int tx = (x1 + x2 - sz.x) / 2;
            dc.DrawText(s, tx, yBar - sz.y / 2);
            lastT = t;
        }
    }

    int TimeToX(int t, double scale) const
    {
        return LEFT_MARGIN + (int)((t - m_timeOffset) * scale);
    }

    void LoadVcdSignals(vcd_t* vcd) {
        m_vcdData = vcd;
        m_allSignals.clear();
        signal_node_t* node = vcd->signals_head;
        while (node) {
            m_allSignals.push_back(&node->signal);
            node = node->next;
        }
        m_maxTimestamp = vcd_get_max_timestamp(vcd);
        if (m_maxTimestamp <= 0) m_maxTimestamp = 1000;
        m_displayTimeRange = m_maxTimestamp;
        InitSignalTree();
        BuildDrawCacheAsync();
        Refresh();
    }

    void ToggleCursorValueDisplay()
    {
        m_showCursorValue = !m_showCursorValue;
        Refresh();
    }

    char ParseVcdValue(const char* v) const
    {
        if (!v || !v[0]) return '0';
        char c = tolower(v[0]);
        return (c == '0' || c == '1' || c == 'x' || c == 'z') ? c : '0';
    }

    std::string ParseBusValue(const char* v) const
    {
        if (!v) return "";
        return (v[0] == 'b' || v[0] == 'r') ? std::string(v + 1) : std::string(v);
    }

    void SetVisibleSignals(const std::unordered_set<std::string>& visible)
    {
        m_visibleSignals = visible;
        m_allSignals.clear();
        signal_node_t* node = m_vcdData->signals_head;
        while (node)
        {
            signal_t* sig = &node->signal;
            if (visible.empty() || visible.count(sig->signal_id))
                m_allSignals.push_back(sig);
            node = node->next;
        }
        AssignSignalColors();
        BuildDrawCacheAsync();
        Refresh();
    }

    WaveformPanel(wxWindow* parent)
        : wxGLCanvas(parent), m_vcdData(nullptr), m_currentTimestamp(0),
        m_displayTimeRange(1000), m_maxTimestamp(1000)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetDoubleBuffered(true);
        SetBackgroundColour(wxColour(245, 245, 220));
        m_rng.seed(std::random_device{}());

        Bind(wxEVT_PAINT, &WaveformPanel::OnPaint, this);
        Bind(wxEVT_SIZE, &WaveformPanel::OnResize, this);
        Bind(wxEVT_MOTION, &WaveformPanel::OnMouseMove, this);
        Bind(wxEVT_MOUSEWHEEL, &WaveformPanel::OnMouseWheel, this);
        Bind(wxEVT_LEFT_DOWN, &WaveformPanel::OnMouseDown, this);
        Bind(wxEVT_LEFT_UP, &WaveformPanel::OnMouseUp, this);
        Bind(wxEVT_RIGHT_DOWN, &WaveformPanel::OnRightDown, this);
        Bind(wxEVT_RIGHT_UP, &WaveformPanel::OnRightUp, this);
    }

    ~WaveformPanel()
    {
        if (HasCapture()) ReleaseMouse();
        if (m_signalTreeRoot) FreeSignalTree(m_signalTreeRoot);
    }

    void OnRightDown(wxMouseEvent& event)
    {
        int x = event.GetX();
        if (x < LEFT_MARGIN) return;
        if (event.ShiftDown())
        {
            if (!m_markers.empty()) { m_markers.pop_back(); Refresh(); }
            return;
        }
        m_isSelecting = true;
        m_selectStartX = x;
        m_selectEndX = x;
        CaptureMouse();
    }

    void OnRightUp(wxMouseEvent& event)
    {
        if (!m_isSelecting) return;
        m_isSelecting = false;
        if (HasCapture()) ReleaseMouse();

        int x1 = m_selectStartX, x2 = m_selectEndX;
        if (abs(x2 - x1) < 5) return;
        if (x1 > x2) std::swap(x1, x2);

        int viewW = GetSize().x - LEFT_MARGIN - WAVE_PADDING;
        if (viewW < 1) viewW = 1;
        double scale = (double)viewW / m_displayTimeRange;
        int t1 = m_timeOffset + (int)((x1 - LEFT_MARGIN) / scale);
        int t2 = m_timeOffset + (int)((x2 - LEFT_MARGIN) / scale);
        if (t2 <= t1) return;

        m_timeOffset = t1;
        m_displayTimeRange = t2 - t1;
        ClampViewToLimit();
        if (m_displayTimeRange < 10) m_displayTimeRange = 10;
        BuildDrawCacheAsync();
        Refresh();
    }
    
    // 添加标记
    void AddMarker(int timestamp, const wxString& label)
    {
        Marker mk;
        mk.timestamp = timestamp;
        mk.label = label;
        m_markers.push_back(mk);
        Refresh();
    }
    
    // 查找上一个边沿
    void FindPreviousEdge()
    {
        // 查找上一个边沿
        int currentTime = GetCurrentTime();
        int newTime = currentTime;
        
        // 实现真正的边沿检测
        if (!m_displayedSignals2.empty()) {
            // 选择第一个信号来检测边沿
            signal_t* sig = m_displayedSignals2[0];
            if (sig && sig->changes_count > 0) {
                // 向前搜索边沿
                for (int i = sig->changes_count - 1; i >= 0; i--) {
                    if (sig->value_changes[i].timestamp < currentTime) {
                        newTime = sig->value_changes[i].timestamp;
                        break;
                    }
                }
            }
        }
        
        // 确保时间在有效范围内
        newTime = std::max(0, newTime);
        
        // 更新当前时间
        SetCurrentTime(newTime);
        Refresh();
        
        // 显示成功消息
        wxMessageBox(wxString::Format("Found previous edge at time %d.", newTime));
    }
    
    // 查找下一个边沿
    void FindNextEdge()
    {
        // 查找下一个边沿
        int currentTime = GetCurrentTime();
        int newTime = currentTime;
        
        // 实现真正的边沿检测
        if (!m_displayedSignals2.empty()) {
            // 选择第一个信号来检测边沿
            signal_t* sig = m_displayedSignals2[0];
            if (sig && sig->changes_count > 0) {
                // 向后搜索边沿
                for (int i = 0; i < sig->changes_count; i++) {
                    if (sig->value_changes[i].timestamp > currentTime) {
                        newTime = sig->value_changes[i].timestamp;
                        break;
                    }
                }
            }
        }
        
        // 确保时间在有效范围内
        if (m_maxTimestamp > 0) {
            newTime = std::min(m_maxTimestamp, newTime);
        }
        
        // 更新当前时间
        SetCurrentTime(newTime);
        Refresh();
        
        // 显示成功消息
        wxMessageBox(wxString::Format("Found next edge at time %d.", newTime));
    }
    
    // 获取当前时间
    int GetCurrentTime()
    {
        return m_currentTimestamp;
    }
    
    // 设置当前时间
    void SetCurrentTime(int timestamp)
    {
        m_currentTimestamp = timestamp;
        // 确保时间在有效范围内
        m_currentTimestamp = std::max(0, m_currentTimestamp);
        if (m_maxTimestamp > 0) {
            m_currentTimestamp = std::min(m_maxTimestamp, m_currentTimestamp);
        }
    }

    void OnMouseDown(wxMouseEvent& event)
    {
        if (m_minimapRect.Contains(event.GetPosition()))
        {
            int mx = event.GetX();
            double ratio = (double)(mx - m_minimapRect.x) / m_minimapRect.width;
            int newCenterTime = (int)(ratio * m_maxTimestamp);
            m_timeOffset = newCenterTime - m_displayTimeRange / 2;
            ClampViewToLimit();
            if (m_timeOffset < 0) m_timeOffset = 0;
            if (m_timeOffset > m_maxTimestamp - m_displayTimeRange)
                m_timeOffset = m_maxTimestamp - m_displayTimeRange;
            BuildDrawCacheAsync();
            m_draggingMinimap = true;
            CaptureMouse();
            Refresh();
            return;
        }

        int x = event.GetX();
        int y = event.GetY();
        
        // 处理信号名称区域的点击
        if (x < LEFT_MARGIN)
        {
            int timeAxisY = 25;
            int yOffset = 30;
            int safeCount = std::min((int)m_displayedSignals2.size(), (int)m_cachedSegments.size());
            
            for (int i = 0; i < safeCount; i++)
            {
                int yBase = timeAxisY + i * SIGNAL_ROW_HEIGHT + yOffset;
                int yStart = yBase - 20;
                int yEnd = yBase + 20;
                
                if (y >= yStart && y <= yEnd)
                {
                    m_selectedSignalIndex = i;
                    Refresh();
                    return;
                }
            }
            return;
        }

        if (event.RightDown())
        {
            for (size_t i = 0; i < m_markers.size(); i++)
            {
                int cx = TimeToX(m_markers[i].timestamp, (double)(GetSize().x - LEFT_MARGIN - WAVE_PADDING) / m_displayTimeRange);
                if (abs(x - cx) < 10) { DeleteMarker(i); return; }
            }
            return;
        }

        if (event.LeftDClick())
        {
            for (size_t i = 0; i < m_markers.size(); i++)
            {
                int cx = TimeToX(m_markers[i].timestamp, (double)(GetSize().x - LEFT_MARGIN - WAVE_PADDING) / m_displayTimeRange);
                if (abs(x - cx) < 10) { StartEditMarker(i); return; }
            }
        }

        for (size_t i = 0; i < m_markers.size(); ++i) {
            int cursorX = TimeToX(m_markers[i].timestamp, (double)(GetSize().x - LEFT_MARGIN - WAVE_PADDING) / m_displayTimeRange);
            if (abs(x - cursorX) < 10) {
                if (!m_markersLocked) {
                    m_draggingMarkerIndex = i;
                    m_draggingMarkerStartX = x;
                    CaptureMouse();
                }
                return;
            }
        }

        if (event.ShiftDown())
        {
            double scale = (double)(GetSize().x - LEFT_MARGIN - WAVE_PADDING) / m_displayTimeRange;
            int timeAtCursor = m_timeOffset + (int)((x - LEFT_MARGIN) / scale);
            timeAtCursor = FindNearestEdge(timeAtCursor);
            Marker mk;
            mk.timestamp = timeAtCursor;
            mk.label = wxString::Format("M%d", (int)m_markers.size());
            m_markers.push_back(mk);
            Refresh();
        }
    }

    void OnMouseUp(wxMouseEvent&)
    {
        m_isDragging = false;
        m_draggingMarkerIndex = -1;
        m_draggingMinimap = false;
        if (HasCapture()) ReleaseMouse();
    }

    void OnMouseWheel(wxMouseEvent& event)
    {
        if (m_alternateWheelMode) {
            // 控制时间
            int currentTime = GetCurrentTime();
            int delta = event.GetWheelRotation() > 0 ? -10 : 10;
            int newTime = currentTime + delta;
            newTime = std::max(0, newTime);
            if (m_maxTimestamp > 0) {
                newTime = std::min(m_maxTimestamp, newTime);
            }
            SetCurrentTime(newTime);
            Refresh();
        } else {
            // 控制缩放
            int mouseX = event.GetX();
            int viewW = GetSize().x - LEFT_MARGIN - WAVE_PADDING;
            if (viewW < 1) viewW = 1;
            double scale = (double)viewW / m_displayTimeRange;
            int mouseTime = m_timeOffset + (int)((mouseX - LEFT_MARGIN) / scale);

            m_displayTimeRange *= (event.GetWheelRotation() > 0) ? 0.8 : 1.25;
            if (m_displayTimeRange < 10) m_displayTimeRange = 10;
            if (m_displayTimeRange > m_maxTimestamp) m_displayTimeRange = m_maxTimestamp;

            double newScale = (double)viewW / m_displayTimeRange;
            m_timeOffset = mouseTime - (int)((mouseX - LEFT_MARGIN) / newScale);
            ClampViewToLimit();
            if (m_timeOffset > m_maxTimestamp - m_displayTimeRange) m_timeOffset = m_maxTimestamp - m_displayTimeRange;
            if (m_timeOffset < 0) m_timeOffset = 0;

            BuildDrawCacheAsync();
            Refresh();
        }
    }

    void OnMouseMove(wxMouseEvent& event)
    {
        if (m_draggingMinimap)
        {
            int mx = event.GetX();
            double ratio = (double)(mx - m_minimapRect.x) / m_minimapRect.width;
            int newCenterTime = (int)(ratio * m_maxTimestamp);
            m_timeOffset = newCenterTime - m_displayTimeRange / 2;
            if (m_timeOffset < 0) m_timeOffset = 0;
            if (m_timeOffset > m_maxTimestamp - m_displayTimeRange)
                m_timeOffset = m_maxTimestamp - m_displayTimeRange;
            BuildDrawCacheAsync();
            Refresh();
            return;
        }

        int mx = event.GetX(), my = event.GetY();
        int viewW = GetSize().x - LEFT_MARGIN - WAVE_PADDING;
        if (viewW < 1) viewW = 1;
        double scale = (double)viewW / m_displayTimeRange;

        if (m_draggingMarkerIndex >= 0 && event.ShiftDown() && !m_markersLocked)
        {
            int newTime = m_timeOffset + (int)((mx - LEFT_MARGIN) / scale);
            newTime = FindNearestEdge(newTime);
            if (newTime < 0) newTime = 0;
            if (m_hasLimit)
            {
                if (newTime < m_limitStart) newTime = m_limitStart;
                if (newTime > m_limitEnd) newTime = m_limitEnd;
            }
            else
            {
                if (newTime < 0) newTime = 0;
                if (newTime > m_maxTimestamp) newTime = m_maxTimestamp;
            }
            m_markers[m_draggingMarkerIndex].timestamp = newTime;
            Refresh();
            return;
        }

        if (m_isSelecting) { m_selectEndX = mx; Refresh(); return; }

        if (event.ControlDown())
        {
            int timeAtCursor = m_timeOffset + (int)((mx - LEFT_MARGIN) / scale);
            if (event.LeftIsDown())
            {
                if (!m_hasMarkerA) { m_markerA = timeAtCursor; m_markerB = timeAtCursor; m_hasMarkerA = true; }
                else m_markerB = timeAtCursor;
                m_isMeasuring = true;
                Refresh();
                return;
            }
            if (m_hasMarkerA) { m_markerB = timeAtCursor; Refresh(); return; }
        }
        else
        {
            if (m_hasMarkerA || m_isMeasuring)
            {
                m_hasMarkerA = false; m_isMeasuring = false; m_markerA = -1; m_markerB = -1;
                Refresh();
            }
        }

        if (m_isDragging)
        {
            int dx = mx - m_lastMouseX;
            m_lastMouseX = mx;
            double dt = dx / scale + m_dragRemainder;
            int move = (int)dt;
            m_dragRemainder = dt - move;
            m_timeOffset -= move;
            ClampViewToLimit();
            if (m_timeOffset < 0) m_timeOffset = 0;
            if (m_timeOffset > m_maxTimestamp - m_displayTimeRange)
                m_timeOffset = m_maxTimestamp - m_displayTimeRange;
            BuildDrawCacheAsync();
            Refresh();
            return;
        }

        m_hoverMarkerIndex = -1;
        for (size_t i = 0; i < m_markers.size(); i++)
        {
            int x = TimeToX(m_markers[i].timestamp, scale);
            if (abs(mx - x) < 5) { m_hoverMarkerIndex = i; break; }
        }

        m_hoverSignal = -1; m_hoverSegment = -1;
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        int safeCount = std::min((int)m_displayedSignals2.size(), (int)m_cachedSegments.size());
        for (int i = 0; i < safeCount; i++)
        {
            auto& segments = m_cachedSegments[i];
            int yBase = 25 + i * SIGNAL_ROW_HEIGHT + 30;
            int yH = yBase - 15, yL = yBase + 15;
            for (int k = 0; k < (int)segments.size(); k++)
            {
                auto& seg = segments[k];
                wxRect rect;
                if (seg.value == 'b') rect = wxRect(seg.x1, seg.y - 9, seg.x2 - seg.x1, 18);
                else { int yCurr = (seg.value == '0') ? yL : yH; rect = wxRect(seg.x1, yCurr - 6, seg.x2 - seg.x1, 12); }
                if (rect.Contains(mx, my)) { m_hoverSignal = i; m_hoverSegment = k; Refresh(); return; }
            }
        }
        Refresh();
    }

    // 根据数据格式格式化信号值
    std::string FormatValueByDataFormat(const char* value, DataFormat format)
    {
        if (!value) return "";
        
        std::string val = ParseBusValue(value);
        
        switch (format)
        {
        case FORMAT_BINARY:
            return val;
        case FORMAT_OCTAL:
            // 简单实现，仅支持二进制转八进制
            if (val.find_first_not_of("01") == std::string::npos) {
                try {
                    unsigned long long num = std::stoull(val, nullptr, 2);
                    return std::to_string(num);
                } catch (...) {
                    return val;
                }
            }
            return val;
        case FORMAT_DECIMAL:
            // 简单实现，仅支持二进制转十进制
            if (val.find_first_not_of("01") == std::string::npos) {
                try {
                    unsigned long long num = std::stoull(val, nullptr, 2);
                    return std::to_string(num);
                } catch (...) {
                    return val;
                }
            }
            return val;
        case FORMAT_HEXADECIMAL:
            // 简单实现，仅支持二进制转十六进制
            if (val.find_first_not_of("01") == std::string::npos) {
                try {
                    unsigned long long num = std::stoull(val, nullptr, 2);
                    std::stringstream ss;
                    ss << std::hex << num;
                    return ss.str();
                } catch (...) {
                    return val;
                }
            }
            return val;
        case FORMAT_ASCII:
            // 简单实现，仅支持8位二进制转ASCII
            if (val.length() == 8 && val.find_first_not_of("01") == std::string::npos) {
                try {
                    unsigned char c = static_cast<unsigned char>(std::stoul(val, nullptr, 2));
                    if (isprint(c)) {
                        return std::string(1, c);
                    }
                } catch (...) {
                    return val;
                }
            }
            return val;
        case FORMAT_SIGNED_DECIMAL:
            // 简单实现，仅支持二进制转有符号十进制
            if (val.find_first_not_of("01") == std::string::npos && !val.empty()) {
                try {
                    if (val[0] == '1') {
                        // 二进制补码转有符号十进制
                        unsigned long long num = std::stoull(val, nullptr, 2);
                        unsigned long long mask = 1ULL << (val.length() - 1);
                        long long signedNum = static_cast<long long>(num ^ mask) - static_cast<long long>(mask);
                        return std::to_string(signedNum);
                    } else {
                        unsigned long long num = std::stoull(val, nullptr, 2);
                        return std::to_string(num);
                    }
                } catch (...) {
                    return val;
                }
            }
            return val;
        case FORMAT_REAL:
            // 简单实现，仅支持二进制转实数
            if (val.find_first_not_of("01") == std::string::npos) {
                try {
                    unsigned long long num = std::stoull(val, nullptr, 2);
                    return std::to_string(static_cast<double>(num));
                } catch (...) {
                    return val;
                }
            }
            return val;
        default:
            return val;
        }
    }

    void BuildDrawCacheAsync()
    {
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        if (m_allSignals.empty()) { m_cachedSegments.clear(); return; }
        
        // 强制获取最新、有效的面板尺寸
        wxSize size = GetClientSize();
        int viewW = size.GetWidth() - LEFT_MARGIN - WAVE_PADDING;
        // 确保viewW至少为100，避免缓存为空
        viewW = std::max(viewW, 100);
        double scale = (double)viewW / m_displayTimeRange;
        int offset = m_timeOffset, range = m_displayTimeRange;
        auto signals = m_displayedSignals2;
        
        // 复制数据格式映射
        std::map<signal_t*, DataFormat> signalDataFormats = m_signalDataFormats;

        std::thread([=, this]() {
            std::vector<std::vector<DrawSegment>> newCache;
            newCache.reserve(signals.size());

            int visibleStart = offset;
            int visibleEnd = offset + range;

            for (size_t i = 0; i < signals.size(); i++)
            {
                signal_t* sig = signals[i];
                std::vector<DrawSegment> segments;
                if (!sig || sig->changes_count == 0 || !sig->value_changes)
                {
                    newCache.emplace_back(); continue;
                }

                int yBase = 25 + (int)i * SIGNAL_ROW_HEIGHT + 30;
                int yH = yBase - 15, yL = yBase + 15;
                bool isBus = (sig->size > 1);
                auto* vec = sig->value_changes;
                size_t count = sig->changes_count;

                char lastVal = '0';
                std::string lastText;
                int lastDrawX = LEFT_MARGIN;

                // 获取信号的数据格式
                DataFormat format = FORMAT_BINARY;
                auto formatIt = signalDataFormats.find(sig);
                if (formatIt != signalDataFormats.end()) {
                    format = formatIt->second;
                }

                size_t c;
                for (c = 0; c < count; c++) {
                    if (vec[c].timestamp >= visibleStart)
                        break;
                }

                if (c > 0 && c - 1 < count) {
                    lastVal = ParseVcdValue(vec[c - 1].value);
                    lastText = FormatValueByDataFormat(vec[c - 1].value, format);
                }
                else if (count > 0) {
                    lastVal = ParseVcdValue(vec[0].value);
                    lastText = FormatValueByDataFormat(vec[0].value, format);
                }

                lastDrawX = LEFT_MARGIN;

                int lastPixelX = -1;

                const int MAX_SEGMENTS = 5000;
                size_t step = std::max((size_t)1, count / MAX_SEGMENTS);

                for (; c < count; c+=step)
                {
                    int ts = vec[c].timestamp;
                    if (ts > visibleEnd) break;

                    int currX = LEFT_MARGIN + (int)((ts - offset) * scale);

                    // ✅ 像素级合并（关键）
                    if (currX == lastPixelX) continue;
                    lastPixelX = currX;

                    if (currX <= lastDrawX) continue;

                    DrawSegment seg;
                    seg.x1 = lastDrawX;
                    seg.x2 = currX;
                    seg.value = isBus ? 'b' : lastVal;
                    seg.y = isBus ? yBase : (lastVal == '0' ? yL : yH);
                    seg.text = lastText;
                    segments.push_back(seg);

                    lastDrawX = currX;
                    lastVal = ParseVcdValue(vec[c].value);
                    lastText = FormatValueByDataFormat(vec[c].value, format);
                }

                int endX = LEFT_MARGIN + (int)((visibleEnd - offset) * scale);
                if (endX > lastDrawX) {
                    DrawSegment seg;
                    seg.x1 = lastDrawX;
                    seg.x2 = endX;
                    seg.value = isBus ? 'b' : lastVal;
                    seg.y = isBus ? yBase : (lastVal == '0' ? yL : yH);
                    seg.text = lastText;
                    segments.push_back(seg);
                }

                newCache.push_back(std::move(segments));
            }

            std::lock_guard<std::mutex> lock2(m_cacheMutex);
            m_cachedSegments = std::move(newCache);
            CallAfter([this]() { Refresh(); });
            }).detach();
    }

    void OnResize(wxSizeEvent&)
    {
        if (m_vcdData && !m_allSignals.empty()) BuildDrawCacheAsync();
    }

    void OpenVCDFile(wxString path)
    {
        if (path.IsEmpty()) return;
        ClearWavePanel();
        std::string stdPath = path.ToStdString();
        vcd_t* data = vcd_read_from_path(const_cast<char*>(stdPath.c_str()));
        if (data) SetVcdData(data);
    }

    void ClearWavePanel()
    {
        m_vcdData = nullptr;
        m_allSignals.clear();
        m_signalColors.clear();
        m_currentTimestamp = 0;
        m_displayTimeRange = 1000;
        m_maxTimestamp = 1000;
        { std::lock_guard<std::mutex> lock(m_cacheMutex); m_cachedSegments.clear(); }
        Refresh();
    }

    void SetVcdData(vcd_t* vcdData)
    {
        m_vcdData = vcdData;
        if (!m_vcdData) return;
        m_maxTimestamp = vcd_get_max_timestamp(m_vcdData);
        if (m_maxTimestamp <= 0) m_maxTimestamp = 1000;
        m_displayTimeRange = m_maxTimestamp;
        m_allSignals.clear();
        signal_node_t* node = m_vcdData->signals_head;
        while (node) { m_allSignals.push_back(&node->signal); node = node->next; }
        AssignSignalColors();
        BuildDrawCacheAsync();
        Refresh();
    }

    double NiceStep(double rawStep) const
    {
        double expv = floor(log10(rawStep));
        double base = rawStep / pow(10.0, expv);
        double niceBase = 1;
        if (base < 1.5) niceBase = 1;
        else if (base < 3) niceBase = 2;
        else if (base < 7) niceBase = 5;
        else niceBase = 10;
        return niceBase * pow(10.0, expv);
    }

    void FilterSignalsByModulePath(const std::string& module_path)
    {
        if (!m_vcdData) return;
        m_allSignals.clear();
        signal_node_t* node = m_vcdData->signals_head;
        while (node)
        {
            signal_t* sig = &node->signal;
            std::string sigPath = sig->module_path;
            if (module_path.empty() || sigPath.find(module_path) == 0)
                m_allSignals.push_back(sig);
            node = node->next;
        }
        AssignSignalColors();
        BuildDrawCacheAsync();
        Refresh();
    }

    void SetCurrentTimestamp(int ts)
    {
        m_currentTimestamp = ts;
        Refresh();
    }

    void ZoomIn() { m_displayTimeRange = std::max(10, (int)(m_displayTimeRange * 0.7)); BuildDrawCacheAsync(); Refresh(); }
    void ZoomOut() { m_displayTimeRange = std::min(m_maxTimestamp * 2, (int)(m_displayTimeRange * 1.5)); BuildDrawCacheAsync(); Refresh(); }
    void ZoomReset() { m_displayTimeRange = m_maxTimestamp; BuildDrawCacheAsync(); Refresh(); }

private:
    wxString FormatTimeLabel(double t) const
    {
        if (t < 1e3) return wxString::Format("%.0f ns", t);
        else if (t < 1e6) return wxString::Format("%.2f us", t / 1e3);
        else if (t < 1e9) return wxString::Format("%.2f ms", t / 1e6);
        else return wxString::Format("%.2f s", t / 1e9);
    }

    void DrawMiniMap(wxAutoBufferedPaintDC& dc, wxSize size)
    {
        int w = 200, h = 80;
        int x = size.x - w - 10, y = 10;
        m_minimapRect = wxRect(x, y, w, h);
        dc.SetBrush(wxColour(30, 30, 30));
        dc.SetPen(wxPen(wxColour(100, 100, 100)));
        dc.DrawRectangle(m_minimapRect);
        if (m_allSignals.empty()) return;

        double scale = (double)w / m_maxTimestamp;
        int maxSignals = std::min(20, (int)m_displayedSignals2.size());
        for (int i = 0; i < maxSignals; i++)
        {
            signal_t* sig = m_displayedSignals2[i];
            if (!sig || sig->changes_count == 0) continue;
            int yBase = y + 5 + i * (h / maxSignals);
            for (size_t c = 1; c < sig->changes_count; c++)
            {
                int t1 = sig->value_changes[c - 1].timestamp;
                int t2 = sig->value_changes[c].timestamp;
                int x1 = x + (int)(t1 * scale);
                int x2 = x + (int)(t2 * scale);
                dc.SetPen(wxPen(wxColour(100, 200, 255), 1));
                dc.DrawLine(x1, yBase, x2, yBase);
            }
        }

        int viewX1 = x + (int)((double)m_timeOffset / m_maxTimestamp * w);
        int viewX2 = x + (int)((double)(m_timeOffset + m_displayTimeRange) / m_maxTimestamp * w);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(wxPen(wxColour(255, 200, 0), 2));
        dc.DrawRectangle(viewX1, y, viewX2 - viewX1, h);
    }

    void OnPaint(wxPaintEvent&)
    {
        wxAutoBufferedPaintDC dc(this);
        dc.Clear();
        wxSize size = GetSize();
        if (m_allSignals.empty())
        {
            dc.SetTextForeground(wxColour(80, 80, 80));
            dc.DrawText("Please Start Simulation first", size.x / 2 - 80, size.y / 2);
            return;
        }

        int viewW = size.x - LEFT_MARGIN - WAVE_PADDING;
        if (viewW < 100) viewW = 100;
        double scale = (double)viewW / m_displayTimeRange;
        int timeAxisY = 25;
        int cursorX = TimeToX(m_currentTimestamp, scale);
        double targetPixel = 80.0;
        double rawStep = targetPixel / scale;
        double step = NiceStep(rawStep);
        double start = floor(m_timeOffset / step) * step;

        dc.SetPen(wxPen(wxColour(220, 220, 220), 1, wxPENSTYLE_DOT));
        int lastTextX = -1000;
        for (double ts = start; ts <= m_timeOffset + m_displayTimeRange; ts += step)
        {
            int x = TimeToX((int)ts, scale);
            if (abs(x - lastTextX) > 60)
            {
                dc.DrawText(FormatTimeLabel(ts), x - 10, 5);
                lastTextX = x;
            }
        }

        dc.SetPen(wxPen(wxColour(180, 180, 180), 1));
        for (double ts = start; ts <= m_timeOffset + m_displayTimeRange; ts += step)
        {
            int x = TimeToX((int)ts, scale);
            dc.DrawLine(x, timeAxisY, x, size.y);
        }

        wxRect clip;
        dc.GetClippingBox(&clip.x, &clip.y, &clip.width, &clip.height);
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        int safeCount = std::min((int)m_displayedSignals2.size(), (int)m_cachedSegments.size());
        int yOffset = 30;

        for (int i = 0; i < safeCount; i++)
        {
            signal_t* sig = m_displayedSignals2[i];
            if (!sig) continue;
            
            int yBase = timeAxisY + i * SIGNAL_ROW_HEIGHT + yOffset;
            // 根据模拟信号高度扩展值调整信号高度
            int height = 15 + (m_analogHeightExtension * 15 / 100);
            int yH = yBase - height, yL = yBase + height;

            // 绘制选中状态
            if (i == m_selectedSignalIndex)
            {
                dc.SetBrush(wxBrush(wxColour(220, 240, 255)));
                dc.SetPen(wxPen(wxColour(0, 120, 255), 2));
                dc.DrawRectangle(0, yBase - 20, LEFT_MARGIN, SIGNAL_ROW_HEIGHT);
            }

            // 显示信号别名或名称，以及组合信息
            wxString displayName;
            auto aliasIt = m_signalAliases.find(sig);
            if (aliasIt != m_signalAliases.end() && !aliasIt->second.IsEmpty()) {
                displayName = aliasIt->second + wxString::Format(" (%s)", sig->full_name);
            } else {
                displayName = sig->full_name;
            }
            
            // 检查是否是组合信号
            for (auto& group : m_signalGroups) {
                if (group.signals.size() > 0 && group.signals[0] == sig && !group.isExpanded) {
                    displayName = wxString::Format("[Group] %s (%d signals)", displayName, (int)group.signals.size());
                    break;
                }
            }

            bool isMatch = !m_searchKeyword.empty() && m_searchMatchedSignals.count(sig->signal_id);
            dc.SetTextForeground(isMatch ? wxColour(255, 0, 0) : wxColour(80, 80, 80));
            dc.DrawText(displayName, 10, yBase - 8);

            auto& segments = m_cachedSegments[i];
            bool isBus = (sig->size > 1);
            for (size_t k = 0; k < segments.size(); k++)
            {
                auto& seg = segments[k];
                int drawX1 = std::max(seg.x1, clip.x);
                int drawX2 = std::min(seg.x2, clip.x + clip.width);
                if (drawX2 <= drawX1) continue;

                bool isHover = (i == m_hoverSignal && k == m_hoverSegment);
                if (isBus)
                {
                    wxRect rect(drawX1, seg.y - 9, drawX2 - drawX1, 18);
                    dc.SetBrush(isHover ? wxColour(255, 220, 150) : wxColour(200, 230, 255));
                    dc.SetPen(isHover ? wxPen(wxColour(255, 140, 0), 2) : wxPen(wxColour(0, 100, 200), 1));
                    dc.DrawRectangle(rect);
                    if (!seg.text.empty() && rect.width > 25)
                    {
                        wxSize sz = dc.GetTextExtent(seg.text);
                        dc.SetTextForeground(wxColour(0, 80, 0));
                        dc.DrawText(seg.text, rect.x + (rect.width - sz.x) / 2, rect.y + (rect.height - sz.y) / 2);
                    }
                    continue;
                }

                int yCurr = (seg.value == '0') ? yL : yH;
                wxColour color;
                if (!m_searchKeyword.empty())
                    color = isMatch ? wxColour(255, 60, 60) : wxColour(180, 180, 180);
                else
                {
                    auto it = m_signalColors.find(sig->signal_id);
                    color = (it != m_signalColors.end()) ? it->second : wxColour(0, 0, 0);
                }

                dc.SetPen(isHover ? wxPen(wxColour(255, 140, 0), 2) : wxPen(color, 2));
                dc.DrawLine(drawX1, yCurr, drawX2, yCurr);
                if (k > 0)
                {
                    auto& prev = segments[k - 1];
                    int yPrev = (prev.value == '0') ? yL : yH;
                    if (prev.value != seg.value) dc.DrawLine(drawX1, yPrev, drawX1, yCurr);
                }
                if (isHover) dc.DrawText(wxString::Format("Value: %c", seg.value), seg.x1, yCurr - 20);
            }

            if (m_showCursorValue)
            {
                std::string val = GetValueAt(sig, m_currentTimestamp);
                if (!val.empty())
                {
                    wxString text = wxString::Format("%s = %s", sig->full_name, val);
                    int tx = cursorX + 5;
                    int ty = yBase - 8;
                    if (tx > size.x - 150) tx = cursorX - 140;
                    wxSize sz = dc.GetTextExtent(text);
                    dc.SetBrush(wxColour(255, 255, 220));
                    dc.SetPen(wxPen(wxColour(200, 200, 100)));
                    dc.DrawRectangle(tx - 2, ty - 1, sz.x + 4, sz.y + 2);
                    dc.SetTextForeground(*wxBLACK);
                    dc.DrawText(text, tx, ty);
                }
            }

            if (m_measureMode != MEASURE_NONE)
            {
                double val = 0;
                wxString text;
                if (m_measureMode == MEASURE_FREQ) { val = ComputeFrequency(sig); text = wxString::Format("F=%.3f", val); }
                else if (m_measureMode == MEASURE_DUTY) { val = ComputeDuty(sig); text = wxString::Format("D=%.2f%%", val * 100); }
                dc.SetTextForeground(wxColour(120, 0, 120));
                dc.DrawText(text, size.x - 120, yBase - 10);
            }
        }

        if (m_hoverSignal >= 0 && m_hoverSegment >= 0 && m_hoverSignal < (int)m_cachedSegments.size())
        {
            auto& seg = m_cachedSegments[m_hoverSignal][m_hoverSegment];
            if (!seg.text.empty())
            {
                wxString info = "Value: " + seg.text;
                int mx, my;
                wxGetMousePosition(&mx, &my);
                ScreenToClient(&mx, &my);
                wxSize sz = dc.GetTextExtent(info);
                dc.SetBrush(wxColour(255, 255, 220));
                dc.SetPen(wxPen(wxColour(200, 200, 100)));
                dc.DrawRectangle(mx + 10, my + 10, sz.x + 10, sz.y + 6);
                dc.SetTextForeground(*wxBLACK);
                dc.DrawText(info, mx + 15, my + 13);
            }
        }

        for (size_t i = 0; i < m_markers.size(); i++)
        {
            auto& mk = m_markers[i];
            int x = TimeToX(mk.timestamp, scale);
            bool isHover = ((int)i == m_hoverMarkerIndex);
            bool isDrag = ((int)i == m_draggingMarkerIndex);

            dc.SetPen(isDrag ? wxPen(wxColour(255, 0, 0), 2) :
                isHover ? wxPen(wxColour(255, 140, 0), 2) :
                wxPen(wxColour(0, 180, 0), 2));
            dc.DrawLine(x, 0, x, size.y);

            wxSize sz = dc.GetTextExtent(mk.label);
            wxColour bg = isDrag ? wxColour(255, 220, 220) :
                isHover ? wxColour(255, 240, 200) :
                wxColour(200, 255, 200);
            wxColour bd = isDrag ? wxColour(200, 0, 0) :
                isHover ? wxColour(255, 140, 0) :
                wxColour(0, 120, 0);
            dc.SetBrush(bg);
            dc.SetPen(bd);
            dc.DrawRectangle(x + 3, 5, sz.x + 6, sz.y + 4);
            dc.DrawText(mk.label, x + 6, 7);
            if (isDrag) dc.DrawText(wxString::Format("T=%d", mk.timestamp), x + 5, sz.y + 15);
        }

        if (m_hasMarkerA && m_markerA >= 0 && m_markerB >= 0)
        {
            int xA = TimeToX(m_markerA, scale);
            int xB = TimeToX(m_markerB, scale);
            dc.SetPen(wxPen(wxColour(255, 0, 0), 2));
            dc.DrawLine(xA, 0, xA, size.y);
            dc.DrawLine(xB, 0, xB, size.y);
            wxString text = wxString::Format("ΔT = %d", m_markerB - m_markerA);
            dc.SetTextForeground(wxColour(200, 0, 0));
            dc.DrawText(text, (xA + xB) / 2, 10);
        }

        if (m_isSelecting)
        {
            int x1 = m_selectStartX, x2 = m_selectEndX;
            if (x1 > x2) std::swap(x1, x2);
            dc.SetBrush(wxColour(100, 150, 255, 60));
            dc.SetPen(wxPen(wxColour(50, 100, 200), 1));
            dc.DrawRectangle(x1, 0, x2 - x1, size.y);
        }

        DrawMarkerMeasurementBar(dc, size, scale);
        if (m_isEditingMarker && m_editingMarkerIndex >= 0)
        {
            wxTextEntryDialog dlg(this, "Edit label:", "Marker Name", m_editingMarkerText);
            if (dlg.ShowModal() == wxID_OK) m_markers[m_editingMarkerIndex].label = dlg.GetValue();
            m_isEditingMarker = false;
            m_editingMarkerIndex = -1;
        }

        dc.SetPen(wxPen(wxColour(150, 150, 150), 1, wxPENSTYLE_DOT));
        dc.DrawLine(cursorX, 0, cursorX, size.y);
        DrawMiniMap(dc, size);
    }

public:
    void InitSignalTree() {
        if (m_signalTreeRoot) { FreeSignalTree(m_signalTreeRoot); m_signalTreeRoot = nullptr; }
    }

    vcd_t* m_vcdData;
    int m_currentTimestamp;
    SignalGroup* m_signalTreeRoot = nullptr;
    int m_displayTimeRange;
    int m_maxTimestamp;
    std::vector<signal_t*> m_allSignals;
    std::map<std::string, wxColour> m_signalColors;
    std::mt19937 m_rng;
};

class MyFrame : public wxFrame
{
public:
    WaveformPanel* m_wavePanel;
    wxSlider* m_slider;
    wxButton* m_playBtn;
    wxTimer* m_timer;
    wxTreeListCtrl* m_signalTree;
    wxPanel* m_treePanel;
    wxSplitterWindow* m_splitter;
    wxTextCtrl* m_searchBox;
    wxTextCtrl* m_fromText;
    wxTextCtrl* m_toText;
    wxComboBox* m_measureCombo;
    wxListCtrl* m_signalList;
    AIAnalysisPanel* m_aiPanel;
    wxSplitterWindow* m_mainSplitter;

    // 新增：满足你需求的核心变量
    std::vector<signal_t*> m_displayedSignals;
    std::map<std::string, std::vector<signal_t*>> m_moduleToSignals;
    wxMenu* m_moduleMenu;
    wxTreeListItem m_rightClickModuleItem;

    MyFrame() : wxFrame(nullptr, wxID_ANY, "VCD Waveform Viewer", wxDefaultPosition, wxSize(1400, 800))
    {
        auto menu = new wxMenu;
        menu->Append(1001, "Open New Window\tCtrl+N");
        menu->Append(1002, "Open New Tab\tCtrl+T");
        menu->Append(1003, "Open New Lab\tCtrl+L");
        menu->Append(1004, "Reload Waveform\tShift+Ctrl+R");
        
        // Export 子菜单
        auto exportSubMenu = new wxMenu;
        exportSubMenu->Append(1101, "ASCII Text");
        exportSubMenu->Append(1102, "VCD");
        exportSubMenu->Append(1103, "CSV");
        exportSubMenu->Append(1104, "PostScript");
        exportSubMenu->Append(1105, "PNG");
        exportSubMenu->Append(1106, "SVG");
        menu->AppendSubMenu(exportSubMenu, "Export");
        
        menu->Append(1004, "Close\tCtrl+W");
        menu->AppendSeparator();
        
        menu->Append(1005, "Print To File\tCtrl+P");
        menu->Append(1006, "Grab To File");
        menu->AppendSeparator();
        
        menu->Append(1007, "Read Save File\tCtrl+O");
        menu->Append(1008, "Write Save File\tCtrl+S");
        menu->Append(1009, "Write Save File As\tShift+Ctrl+S");
        menu->AppendSeparator();
        
        menu->Append(1010, "Read Sim Logfile\tL");
        menu->Append(1011, "Read Verilog Stemsfile");
        menu->Append(1012, "Read Tcl Script File");
        menu->AppendSeparator();
        
        menu->Append(1013, "Quit\tCtrl+Q");
        auto measureMenu = new wxMenu;
        measureMenu->Append(2001, "Show Measurement");
        
        // ===== Time 菜单 =====
        auto timeMenu = new wxMenu;
        timeMenu->Append(7001, "Move To Time\tF1");
        timeMenu->AppendSeparator();
        
        // Zoom 子菜单
        auto zoomSubMenu = new wxMenu;
        zoomSubMenu->Append(7101, "Zoom In");
        zoomSubMenu->Append(7102, "Zoom Out");
        zoomSubMenu->Append(7103, "Zoom Full");
        zoomSubMenu->Append(7104, "Zoom Last");
        timeMenu->AppendSubMenu(zoomSubMenu, "Zoom");
        
        // Fetch 子菜单
        auto fetchSubMenu = new wxMenu;
        fetchSubMenu->Append(7201, "Fetch More");
        fetchSubMenu->Append(7202, "Fetch All");
        timeMenu->AppendSubMenu(fetchSubMenu, "Fetch");
        
        // Discard 子菜单
        auto discardSubMenu = new wxMenu;
        discardSubMenu->Append(7301, "Discard To Start");
        discardSubMenu->Append(7302, "Discard To End");
        timeMenu->AppendSubMenu(discardSubMenu, "Discard");
        
        // Shift 子菜单
        auto shiftSubMenu = new wxMenu;
        shiftSubMenu->Append(7401, "Shift Left");
        shiftSubMenu->Append(7402, "Shift Right");
        timeMenu->AppendSubMenu(shiftSubMenu, "Shift");
        
        // Page 子菜单
        auto pageSubMenu = new wxMenu;
        pageSubMenu->Append(7501, "Page Left");
        pageSubMenu->Append(7502, "Page Right");
        timeMenu->AppendSubMenu(pageSubMenu, "Page");
        
        // Markers 菜单
        auto markersMenu = new wxMenu;
        markersMenu->Append(8001, "Show-Change Marker Data	Alt+M");
        markersMenu->Append(8002, "Drop Named Marker	Alt+H");
        markersMenu->Append(8003, "Collect Named Marker	Shift+Alt+H");
        markersMenu->Append(8004, "Collect All Named Markers	Shift+Ctrl+Alt+H");
        markersMenu->Append(8005, "Copy Primary->B Marker	B");
        markersMenu->Append(8006, "Delete Primary Marker	Shift+Alt+M");
        markersMenu->AppendSeparator();
        markersMenu->Append(8007, "Find Previous Edge");
        markersMenu->Append(8008, "Find Next Edge");
        markersMenu->AppendSeparator();
        markersMenu->AppendCheckItem(8009, "Alternate Wheel Mode");
        markersMenu->AppendCheckItem(8010, "Wave Scrolling	F9");
        markersMenu->AppendCheckItem(8011, "Locking");
        
        // Edit 菜单
        auto editMenu = new wxMenu;
        editMenu->Append(8001, "Set Trace Max Hier");
        editMenu->Append(8002, "Toggle Trace Hier");
        editMenu->AppendSeparator();
        editMenu->Append(8003, "Insert Blank	Ctrl+B");
        editMenu->Append(8004, "Insert Comment");
        editMenu->Append(8005, "Insert Analog Height Extension");
        editMenu->AppendSeparator();
        editMenu->Append(8006, "Cut	Ctrl+X");
        editMenu->Append(8007, "Copy	Ctrl+C");
        editMenu->Append(8008, "Paste	Ctrl+V");
        editMenu->Append(8009, "Delete	Ctrl+Delete");
        editMenu->AppendSeparator();
        editMenu->Append(8010, "Alias Highlighted Trace	Alt+A");
        editMenu->Append(8011, "Remove Highlighted Aliases	Shift+Alt+A");
        editMenu->AppendSeparator();
        editMenu->Append(8012, "Expand");
        editMenu->Append(8013, "Combine Down	F3");
        editMenu->Append(8014, "Combine Up	F5");
        editMenu->AppendSeparator();
        
        // Data Format 子菜单
        auto dataFormatSubMenu = new wxMenu;
        dataFormatSubMenu->Append(8101, "Binary");
        dataFormatSubMenu->Append(8102, "Octal");
        dataFormatSubMenu->Append(8103, "Decimal");
        dataFormatSubMenu->Append(8104, "Hexadecimal");
        dataFormatSubMenu->Append(8105, "ASCII");
        dataFormatSubMenu->Append(8106, "Signed Decimal");
        dataFormatSubMenu->Append(8107, "Real");
        editMenu->AppendSubMenu(dataFormatSubMenu, "Data Format");
        
        // Color Format 子菜单
        auto colorFormatSubMenu = new wxMenu;
        colorFormatSubMenu->Append(8201, "Default");
        colorFormatSubMenu->Append(8202, "Signal Name");
        colorFormatSubMenu->Append(8203, "Value");
        colorFormatSubMenu->Append(8204, "Module");
        editMenu->AppendSubMenu(colorFormatSubMenu, "Color Format");
        
        editMenu->Append(8015, "Show-Change All Highlighted	Ctrl+F");
        editMenu->Append(8016, "Show-Change All");
        editMenu->AppendSeparator();
        
        // Time Warp 子菜单
        auto timeWarpSubMenu = new wxMenu;
        timeWarpSubMenu->Append(8301, "Enable");
        timeWarpSubMenu->Append(8302, "Disable");
        timeWarpSubMenu->Append(8303, "Set");
        editMenu->AppendSubMenu(timeWarpSubMenu, "Time Warp");
        
        editMenu->Append(8017, "Exclude	Shift+Alt+E");
        editMenu->Append(8018, "Show	Shift+Alt+S");
        editMenu->AppendSeparator();
        editMenu->Append(8019, "Toggle Group Open/Close	T");
        editMenu->Append(8020, "Create Group	Alt+R");
        editMenu->AppendSeparator();
        editMenu->Append(8021, "Highlight Regexp");
        editMenu->Append(8022, "Highlight All	Ctrl+A");
        editMenu->Append(8023, "UnHighlight All	Shift+Ctrl+A");
        editMenu->AppendSeparator();
        
        // Sort 子菜单
        auto sortSubMenu = new wxMenu;
        sortSubMenu->Append(8401, "By Name");
        sortSubMenu->Append(8402, "By Group");
        sortSubMenu->Append(8403, "By Value");
        sortSubMenu->Append(8404, "By Module");
        editMenu->AppendSubMenu(sortSubMenu, "Sort");
        
        // 添加AI菜单
        auto aiMenu = new wxMenu;
        aiMenu->Append(9001, "Toggle AI Panel");
        aiMenu->Append(9002, "Set API Key");
        
        auto mb = new wxMenuBar;
        mb->Append(menu, "File");
        mb->Append(editMenu, "Edit");
        mb->Append(timeMenu, "Time");
        mb->Append(markersMenu, "Markers");
        mb->Append(measureMenu, "Measure");
        mb->Append(aiMenu, "AI");
        SetMenuBar(mb);

        wxToolBar* tb = CreateToolBar();
        tb->SetToolBitmapSize(wxSize(16, 16));

        tb->AddTool(3001, "Open", wxArtProvider::GetBitmap(wxART_FILE_OPEN, wxART_TOOLBAR));
        tb->AddTool(3002, "Zoom In", wxArtProvider::GetBitmap(wxART_PLUS, wxART_TOOLBAR));
        tb->AddTool(3003, "Zoom Out", wxArtProvider::GetBitmap(wxART_MINUS, wxART_TOOLBAR));
        tb->AddSeparator();
        tb->AddTool(3005, "Filter", wxArtProvider::GetBitmap(wxART_FIND, wxART_TOOLBAR));
        tb->AddTool(3006, "Cursor", wxArtProvider::GetBitmap(wxART_CROSS_MARK, wxART_TOOLBAR), wxNullBitmap, wxITEM_CHECK);
        tb->AddSeparator();

        tb->AddTool(4001, "Start", wxArtProvider::GetBitmap(wxART_GO_HOME, wxART_TOOLBAR));
        tb->AddTool(4002, "Back", wxArtProvider::GetBitmap(wxART_GO_BACK, wxART_TOOLBAR));
        tb->AddTool(4003, "Forward", wxArtProvider::GetBitmap(wxART_GO_FORWARD, wxART_TOOLBAR));
        tb->AddTool(4004, "End", wxArtProvider::GetBitmap(wxART_GOTO_LAST, wxART_TOOLBAR));
        tb->AddSeparator();

        tb->AddTool(4101, "Prev Edge", wxArtProvider::GetBitmap(wxART_GO_BACK, wxART_TOOLBAR));
        tb->AddTool(4102, "Next Edge", wxArtProvider::GetBitmap(wxART_GO_FORWARD, wxART_TOOLBAR));
        tb->AddSeparator();

        m_fromText = new wxTextCtrl(tb, 5001, "0", wxDefaultPosition, wxSize(80, -1), wxTE_PROCESS_ENTER);
        m_toText = new wxTextCtrl(tb, 5002, "0", wxDefaultPosition, wxSize(80, -1), wxTE_PROCESS_ENTER);

        tb->AddControl(new wxStaticText(tb, wxID_ANY, "From:"));
        tb->AddControl(m_fromText);
        tb->AddControl(new wxStaticText(tb, wxID_ANY, "To:"));
        tb->AddControl(m_toText);
        tb->Realize();

        // 创建状态栏
        CreateStatusBar();
        SetStatusText("Ready");

        // 创建主分割窗口
        m_mainSplitter = new wxSplitterWindow(this, wxID_ANY);
        
        // 创建左侧分割窗口
        m_splitter = new wxSplitterWindow(m_mainSplitter, wxID_ANY);
        m_treePanel = new wxPanel(m_splitter);
        m_wavePanel = new WaveformPanel(m_splitter);

        // 创建AI分析面板
        m_aiPanel = new AIAnalysisPanel(m_mainSplitter, m_wavePanel);

        wxSplitterWindow* leftSplitter = new wxSplitterWindow(m_treePanel);
        wxPanel* topPanel = new wxPanel(leftSplitter);
        wxPanel* bottomPanel = new wxPanel(leftSplitter);
        leftSplitter->SplitHorizontally(topPanel, bottomPanel, 300);
        m_splitter->SplitVertically(m_treePanel, m_wavePanel, 280);
        m_mainSplitter->SplitVertically(m_splitter, m_aiPanel, 1000);

        auto ctrl = new wxBoxSizer(wxHORIZONTAL);
        m_playBtn = new wxButton(this, wxID_ANY, "Play");
        m_slider = new wxSlider(this, wxID_ANY, 0, 0, 1000);
        auto zoomIn = new wxButton(this, wxID_ANY, "Zoom+");
        auto zoomOut = new wxButton(this, wxID_ANY, "Zoom-");
        auto reset = new wxButton(this, wxID_ANY, "Reset");
        auto filter = new wxButton(this, wxID_ANY, "Filter");
        auto toggle = new wxButton(this, wxID_ANY, "Cursor Value");

        ctrl->Add(m_playBtn, 0, wxALL, 5);
        ctrl->Add(m_slider, 1, wxEXPAND | wxALL, 5);
        ctrl->Add(zoomIn, 0, wxALL, 5);
        ctrl->Add(zoomOut, 0, wxALL, 5);
        ctrl->Add(reset, 0, wxALL, 5);
        ctrl->Add(filter, 0, wxALL, 5);
        ctrl->Add(toggle, 0, wxALL, 5);

        zoomIn->Hide(); zoomOut->Hide(); filter->Hide(); toggle->Hide();

        wxBoxSizer* leftSizer = new wxBoxSizer(wxVERTICAL);
        leftSizer->Add(leftSplitter, 1, wxEXPAND);
        m_treePanel->SetSizer(leftSizer);
        m_measureCombo = new wxComboBox(this, wxID_ANY);
        m_measureCombo->Append("None");
        m_measureCombo->Append("Frequency");
        m_measureCombo->Append("Duty");
        ctrl->Add(m_measureCombo, 0, wxALL, 5);
        m_measureCombo->Hide();

        m_signalTree = new wxTreeListCtrl(topPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxTL_DEFAULT_STYLE);
        m_signalTree->AppendColumn("Modules");

        wxBoxSizer* topSizer = new wxBoxSizer(wxVERTICAL);
        topSizer->Add(m_signalTree, 1, wxEXPAND);
        topPanel->SetSizer(topSizer);

        m_signalList = new wxListCtrl(bottomPanel, wxID_ANY, wxDefaultPosition, wxDefaultSize, wxLC_REPORT | wxLC_SINGLE_SEL);
        m_signalList->AppendColumn("Type", wxLIST_FORMAT_LEFT, 60);
        m_signalList->AppendColumn("Signals", wxLIST_FORMAT_LEFT, 120);

        wxBoxSizer* bottomSizer = new wxBoxSizer(wxVERTICAL);
        bottomSizer->Add(m_signalList, 1, wxEXPAND);
        bottomPanel->SetSizer(bottomSizer);

        // 绑定信号点击事件
        m_signalList->Bind(wxEVT_LIST_ITEM_ACTIVATED, &MyFrame::OnSignalClick, this);

        // ===== 右键菜单绑定（正确版本）=====
        m_moduleMenu = new wxMenu();
        m_moduleMenu->Append(6001, "Add all signals (include submodules)");

        // 关键：这里必须用 wxEVT_RIGHT_DOWN，参数是 wxMouseEvent
        m_signalTree->Bind(wxEVT_CONTEXT_MENU,
            &MyFrame::OnModuleRightClick, this);

        // 菜单命令
        Bind(wxEVT_MENU, &MyFrame::OnAddAllModuleSignals, this, 6001);

        Bind(wxEVT_TOOL, &MyFrame::OnToolbarClick, this);

        m_searchBox = new wxTextCtrl(this, wxID_ANY);
        ctrl->Add(m_searchBox, 0, wxALL, 5);
        m_searchBox->Bind(wxEVT_TEXT, [this](wxCommandEvent& e) {
            m_wavePanel->SearchSignals(m_searchBox->GetValue().ToStdString());
            });

        m_playBtn->Bind(wxEVT_BUTTON, &MyFrame::OnPlay, this);
        m_slider->Bind(wxEVT_SLIDER, &MyFrame::OnSlide, this);
        zoomIn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { m_wavePanel->ZoomIn(); SyncTimeRangeUI(); });
        zoomOut->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { m_wavePanel->ZoomOut(); SyncTimeRangeUI(); });
        reset->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { m_wavePanel->ZoomReset(); SyncTimeRangeUI(); });
        filter->Bind(wxEVT_BUTTON, &MyFrame::OnFilter, this);
        toggle->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { m_wavePanel->ToggleCursorValueDisplay(); });
        m_signalTree->Bind(wxEVT_TREELIST_SELECTION_CHANGED, &MyFrame::OnTreeSelect, this);
        m_fromText->Bind(wxEVT_TEXT_ENTER, &MyFrame::OnTimeRangeChanged, this);
        m_toText->Bind(wxEVT_TEXT_ENTER, &MyFrame::OnTimeRangeChanged, this);

        m_measureCombo->Bind(wxEVT_COMBOBOX, [this](wxCommandEvent& e) {
            int sel = e.GetSelection();
            if (sel == 1) m_wavePanel->m_measureMode = WaveformPanel::MEASURE_FREQ;
            else if (sel == 2) m_wavePanel->m_measureMode = WaveformPanel::MEASURE_DUTY;
            else m_wavePanel->m_measureMode = WaveformPanel::MEASURE_NONE;
            m_wavePanel->Refresh();
            });

        m_timer = new wxTimer(this);
        Bind(wxEVT_TIMER, &MyFrame::OnTimer, this);

        wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
        mainSizer->Add(m_mainSplitter, 1, wxEXPAND);
        mainSizer->Add(ctrl, 0, wxEXPAND);
        SetSizer(mainSizer);

        // File 菜单事件绑定
        Bind(wxEVT_MENU, &MyFrame::OnOpenNewWindow, this, 1001);
        Bind(wxEVT_MENU, &MyFrame::OnOpenNewTab, this, 1002);
        Bind(wxEVT_MENU, &MyFrame::OnOpenNewLab, this, 1003);
        
        // Markers 菜单事件绑定
        Bind(wxEVT_MENU, &MyFrame::OnShowChangeMarkerData, this, 8001);
        Bind(wxEVT_MENU, &MyFrame::OnDropNamedMarker, this, 8002);
        Bind(wxEVT_MENU, &MyFrame::OnCollectNamedMarker, this, 8003);
        Bind(wxEVT_MENU, &MyFrame::OnCollectAllNamedMarkers, this, 8004);
        Bind(wxEVT_MENU, &MyFrame::OnCopyPrimaryToBMarker, this, 8005);
        Bind(wxEVT_MENU, &MyFrame::OnDeletePrimaryMarker, this, 8006);
        Bind(wxEVT_MENU, &MyFrame::OnFindPreviousEdge, this, 8007);
        Bind(wxEVT_MENU, &MyFrame::OnFindNextEdge, this, 8008);
        Bind(wxEVT_MENU, &MyFrame::OnAlternateWheelMode, this, 8009);
        Bind(wxEVT_MENU, &MyFrame::OnWaveScrolling, this, 8010);
        Bind(wxEVT_MENU, &MyFrame::OnLocking, this, 8011);
        Bind(wxEVT_MENU, &MyFrame::OnReloadWaveform, this, 1004);
        Bind(wxEVT_MENU, &MyFrame::OnClose, this, 1005);
        Bind(wxEVT_MENU, &MyFrame::OnPrintToFile, this, 1005);
        Bind(wxEVT_MENU, &MyFrame::OnGrabToFile, this, 1006);
        Bind(wxEVT_MENU, &MyFrame::OnReadSaveFile, this, 1007);
        Bind(wxEVT_MENU, &MyFrame::OnWriteSaveFile, this, 1008);
        Bind(wxEVT_MENU, &MyFrame::OnWriteSaveFileAs, this, 1009);
        Bind(wxEVT_MENU, &MyFrame::OnReadSimLogfile, this, 1010);
        Bind(wxEVT_MENU, &MyFrame::OnReadVerilogStemsfile, this, 1011);
        Bind(wxEVT_MENU, &MyFrame::OnReadTclScriptFile, this, 1012);
        Bind(wxEVT_MENU, &MyFrame::OnQuit, this, 1013);
        
        // Export 子菜单事件绑定
        Bind(wxEVT_MENU, &MyFrame::OnExportAsciiText, this, 1101);
        Bind(wxEVT_MENU, &MyFrame::OnExportVCD, this, 1102);
        Bind(wxEVT_MENU, &MyFrame::OnExportCSV, this, 1103);
        Bind(wxEVT_MENU, &MyFrame::OnExportPostScript, this, 1104);
        Bind(wxEVT_MENU, &MyFrame::OnExportPNG, this, 1105);
        Bind(wxEVT_MENU, &MyFrame::OnExportSVG, this, 1106);
        Bind(wxEVT_MENU, &MyFrame::OnShowMeasure, this, 2001);
        
        // ===== Time 菜单事件绑定 =====
        Bind(wxEVT_MENU, &MyFrame::OnMoveToTime, this, 7001);
        Bind(wxEVT_MENU, &MyFrame::OnZoomIn, this, 7101);
        Bind(wxEVT_MENU, &MyFrame::OnZoomOut, this, 7102);
        Bind(wxEVT_MENU, &MyFrame::OnZoomFull, this, 7103);
        Bind(wxEVT_MENU, &MyFrame::OnZoomLast, this, 7104);
        Bind(wxEVT_MENU, &MyFrame::OnFetchMore, this, 7201);
        Bind(wxEVT_MENU, &MyFrame::OnFetchAll, this, 7202);
        Bind(wxEVT_MENU, &MyFrame::OnDiscardToStart, this, 7301);
        Bind(wxEVT_MENU, &MyFrame::OnDiscardToEnd, this, 7302);
        Bind(wxEVT_MENU, &MyFrame::OnShiftLeft, this, 7401);
        Bind(wxEVT_MENU, &MyFrame::OnShiftRight, this, 7402);
        Bind(wxEVT_MENU, &MyFrame::OnPageLeft, this, 7501);
        Bind(wxEVT_MENU, &MyFrame::OnPageRight, this, 7502);
        
        // Edit 菜单事件绑定
        Bind(wxEVT_MENU, &MyFrame::OnSetTraceMaxHier, this, 8001);
        Bind(wxEVT_MENU, &MyFrame::OnToggleTraceHier, this, 8002);
        
        // AI 菜单事件绑定
        Bind(wxEVT_MENU, &MyFrame::OnToggleAIPanel, this, 9001);
        Bind(wxEVT_MENU, &MyFrame::OnSetAPIKey, this, 9002);
        Bind(wxEVT_MENU, &MyFrame::OnInsertBlank, this, 8003);
        Bind(wxEVT_MENU, &MyFrame::OnInsertComment, this, 8004);
        Bind(wxEVT_MENU, &MyFrame::OnInsertAnalogHeightExtension, this, 8005);
        Bind(wxEVT_MENU, &MyFrame::OnCut, this, 8006);
        Bind(wxEVT_MENU, &MyFrame::OnCopy, this, 8007);
        Bind(wxEVT_MENU, &MyFrame::OnPaste, this, 8008);
        Bind(wxEVT_MENU, &MyFrame::OnDelete, this, 8009);
        Bind(wxEVT_MENU, &MyFrame::OnAliasHighlightedTrace, this, 8010);
        Bind(wxEVT_MENU, &MyFrame::OnRemoveHighlightedAliases, this, 8011);
        Bind(wxEVT_MENU, &MyFrame::OnExpand, this, 8012);
        Bind(wxEVT_MENU, &MyFrame::OnCombineDown, this, 8013);
        Bind(wxEVT_MENU, &MyFrame::OnCombineUp, this, 8014);
        Bind(wxEVT_MENU, &MyFrame::OnShowChangeAllHighlighted, this, 8015);
        Bind(wxEVT_MENU, &MyFrame::OnShowChangeAll, this, 8016);
        Bind(wxEVT_MENU, &MyFrame::OnExclude, this, 8017);
        Bind(wxEVT_MENU, &MyFrame::OnShow, this, 8018);
        Bind(wxEVT_MENU, &MyFrame::OnToggleGroupOpenClose, this, 8019);
        Bind(wxEVT_MENU, &MyFrame::OnCreateGroup, this, 8020);
        Bind(wxEVT_MENU, &MyFrame::OnHighlightRegexp, this, 8021);
        Bind(wxEVT_MENU, &MyFrame::OnHighlightAll, this, 8022);
        Bind(wxEVT_MENU, &MyFrame::OnUnHighlightAll, this, 8023);
        
        // Data Format 子菜单事件绑定
        Bind(wxEVT_MENU, &MyFrame::OnDataFormatBinary, this, 8101);
        Bind(wxEVT_MENU, &MyFrame::OnDataFormatOctal, this, 8102);
        Bind(wxEVT_MENU, &MyFrame::OnDataFormatDecimal, this, 8103);
        Bind(wxEVT_MENU, &MyFrame::OnDataFormatHexadecimal, this, 8104);
        Bind(wxEVT_MENU, &MyFrame::OnDataFormatASCII, this, 8105);
        Bind(wxEVT_MENU, &MyFrame::OnDataFormatSignedDecimal, this, 8106);
        Bind(wxEVT_MENU, &MyFrame::OnDataFormatReal, this, 8107);
        
        // Color Format 子菜单事件绑定
        Bind(wxEVT_MENU, &MyFrame::OnColorFormatDefault, this, 8201);
        Bind(wxEVT_MENU, &MyFrame::OnColorFormatSignalName, this, 8202);
        Bind(wxEVT_MENU, &MyFrame::OnColorFormatValue, this, 8203);
        Bind(wxEVT_MENU, &MyFrame::OnColorFormatModule, this, 8204);
        
        // Time Warp 子菜单事件绑定
        Bind(wxEVT_MENU, &MyFrame::OnTimeWarpEnable, this, 8301);
        Bind(wxEVT_MENU, &MyFrame::OnTimeWarpDisable, this, 8302);
        Bind(wxEVT_MENU, &MyFrame::OnTimeWarpSet, this, 8303);
        
        // Sort 子菜单事件绑定
        Bind(wxEVT_MENU, &MyFrame::OnSortByName, this, 8401);
        Bind(wxEVT_MENU, &MyFrame::OnSortByGroup, this, 8402);
        Bind(wxEVT_MENU, &MyFrame::OnSortByValue, this, 8403);
        Bind(wxEVT_MENU, &MyFrame::OnSortByModule, this, 8404);
    }

    // 点击信号 → 添加到波形区（可重复）
    void OnSignalClick(wxListEvent& event)
    {
        signal_t* sig = (signal_t*)m_signalList->GetItemData(event.GetIndex());
        if (!sig) {
            wxLogDebug("OnSignalClick: sig is NULL");
            return;
        }

        // 关键：调用WaveformPanel的AddVisibleSignal，而不是直接赋值
        m_wavePanel->AddDisplaySignal(sig);

        
    }

    void OnModuleRightClick(wxContextMenuEvent& event)
    {
        wxPoint pt = event.GetPosition();

        if (pt == wxDefaultPosition)
            pt = wxGetMousePosition();

        pt = m_signalTree->ScreenToClient(pt);

        // ❗关键：你这里写残了
        // item = m_signalTree->GetItem  ← 这行是废的！

        wxTreeListItem item = m_signalTree->GetSelection(); // ✔ 稳定解

        if (!item.IsOk())
            return;

        m_signalTree->Select(item);
        m_rightClickModuleItem = item;

        m_signalTree->PopupMenu(m_moduleMenu, pt);
    }

    // 递归获取模块+子模块所有信号
    void GetAllSignalsInModuleTree(const std::string& rootPath, std::vector<signal_t*>& outSignals)
    {
        for (auto& pair : m_moduleToSignals)
        {
            const std::string& path = pair.first;
            if (path.find(rootPath) == 0)
            {
                outSignals.insert(outSignals.end(), pair.second.begin(), pair.second.end());
            }
        }
    }

    // 右键添加全部信号
    void OnAddAllModuleSignals(wxCommandEvent&)
    {
        if (!m_rightClickModuleItem.IsOk()) return;
        std::string modulePath = GetFullPath(m_signalTree, m_rightClickModuleItem);
        std::vector<signal_t*> allSigs;
        GetAllSignalsInModuleTree(modulePath, allSigs);

        m_displayedSignals.insert(m_displayedSignals.end(), allSigs.begin(), allSigs.end());
        m_wavePanel->ClearDisplaySignals();
        for (auto sig : allSigs) {
            m_wavePanel->AddDisplaySignal(sig);
        }
        m_wavePanel->AssignSignalColors();
        m_wavePanel->BuildDrawCacheAsync();
        m_wavePanel->Refresh();
    }

    void OnTimeRangeChanged(wxCommandEvent&)
    {
        long from, to;
        if (m_fromText->GetValue().ToLong(&from) && m_toText->GetValue().ToLong(&to) && to > from)
        {
            m_wavePanel->m_limitStart = from;
            m_wavePanel->m_limitEnd = to;
            m_wavePanel->m_hasLimit = true;

            m_wavePanel->m_timeOffset = from;
            m_wavePanel->m_displayTimeRange = to - from;

            m_wavePanel->BuildDrawCacheAsync();
            m_wavePanel->Refresh();
            SyncTimeRangeUI();
        }
    }

    void SyncTimeRangeUI()
    {
        m_fromText->SetValue(wxString::Format("%d", m_wavePanel->m_timeOffset));
        m_toText->SetValue(wxString::Format("%d", m_wavePanel->m_timeOffset + m_wavePanel->m_displayTimeRange));
    }

    void OnToolbarClick(wxCommandEvent& e)
    {
        switch (e.GetId())
        {
        case 3001: OnOpenNewTab(e); break;
        case 3002: m_wavePanel->ZoomIn(); break;
        case 3003: m_wavePanel->ZoomOut(); break;
        case 3005: OnFilter(e); break;
        case 3006: m_wavePanel->ToggleCursorValueDisplay(); break;
        case 4001: m_wavePanel->m_timeOffset = 0; break;
        case 4002: m_wavePanel->m_timeOffset -= m_wavePanel->m_displayTimeRange; if (m_wavePanel->m_timeOffset < 0) m_wavePanel->m_timeOffset = 0; break;
        case 4003: m_wavePanel->m_timeOffset += m_wavePanel->m_displayTimeRange; break;
        case 4004: m_wavePanel->m_timeOffset = m_wavePanel->m_maxTimestamp - m_wavePanel->m_displayTimeRange; if (m_wavePanel->m_timeOffset < 0) m_wavePanel->m_timeOffset = 0; break;
        case 4101: m_wavePanel->SetCurrentTimestamp(m_wavePanel->FindPrevEdge(m_wavePanel->m_currentTimestamp)); break;
        case 4102: m_wavePanel->SetCurrentTimestamp(m_wavePanel->FindNextEdge(m_wavePanel->m_currentTimestamp)); break;
        }

        if (m_wavePanel->m_timeOffset < 0) m_wavePanel->m_timeOffset = 0;
        if (m_wavePanel->m_timeOffset + m_wavePanel->m_displayTimeRange > m_wavePanel->m_maxTimestamp)
            m_wavePanel->m_timeOffset = m_wavePanel->m_maxTimestamp - m_wavePanel->m_displayTimeRange;
        if (m_wavePanel->m_timeOffset < 0) m_wavePanel->m_timeOffset = 0;

        m_wavePanel->BuildDrawCacheAsync();
        m_wavePanel->Refresh();
        SyncTimeRangeUI();
        m_wavePanel->ClampViewToLimit();
    }

    void OnShowMeasure(wxCommandEvent&)
    {
        m_measureCombo->IsShown() ? m_measureCombo->Hide() : m_measureCombo->Show();
        Layout();
    }
    
    // ===== File 菜单功能实现 =====
    
    void OnOpenNewWindow(wxCommandEvent&)
    {
        (new MyFrame())->Show();
    }
    
    void OnOpenNewTab(wxCommandEvent&)
    {
        wxFileDialog dlg(this, "Open VCD", "", "", "VCD (*.vcd)|*.vcd");

        if (dlg.ShowModal() == wxID_OK)
        {
            m_displayedSignals.clear();
            m_wavePanel->ClearDisplaySignals();
            m_wavePanel->OpenVCDFile(dlg.GetPath());
            m_slider->SetRange(0, m_wavePanel->m_maxTimestamp);
            m_slider->SetValue(0);
            BuildSignalTreeView(m_signalTree, m_wavePanel->m_vcdData);
            m_signalList->DeleteAllItems();
            
            // 将所有信号传递给AI分析面板
            if (m_aiPanel && !m_wavePanel->m_allSignals.empty())
            {
                m_aiPanel->SetSignalInfo(m_wavePanel->m_allSignals);
            }
        }
    }
    
    void OnReloadWaveform(wxCommandEvent&)
    {
        // 重新加载波形
        m_wavePanel->BuildDrawCacheAsync();
        m_wavePanel->Refresh();
    }
    
    void OnClose(wxCommandEvent&)
    {
        Close();
    }
    
    void OnPrintToFile(wxCommandEvent&)
    {
        wxFileDialog saveDialog(this, "Print To File", "", "waveform.ps", "PostScript files (*.ps)|*.ps|All files (*.*)|*.*", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (saveDialog.ShowModal() == wxID_OK)
        {
            wxMessageBox("Printing to: " + saveDialog.GetPath());
        }
    }
    
    void OnGrabToFile(wxCommandEvent&)
    {
        wxFileDialog saveDialog(this, "Grab To File", "", "waveform.png", "PNG files (*.png)|*.png|All files (*.*)|*.*", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (saveDialog.ShowModal() == wxID_OK)
        {
            wxMessageBox("Grabbing waveform to: " + saveDialog.GetPath());
        }
    }
    
    void OnReadSaveFile(wxCommandEvent&)
    {
        wxFileDialog openDialog(this, "Read Save File", "", "", "Save files (*.save)|*.save|All files (*.*)|*.*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (openDialog.ShowModal() == wxID_OK)
        {
            wxMessageBox("Reading save file: " + openDialog.GetPath());
        }
    }
    
    void OnWriteSaveFile(wxCommandEvent&)
    {
        wxFileDialog saveDialog(this, "Write Save File", "", "waveform.save", "Save files (*.save)|*.save|All files (*.*)|*.*", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (saveDialog.ShowModal() == wxID_OK)
        {
            wxMessageBox("Writing save file: " + saveDialog.GetPath());
        }
    }
    
    void OnWriteSaveFileAs(wxCommandEvent&)
    {
        wxFileDialog saveDialog(this, "Write Save File As", "", "waveform.save", "Save files (*.save)|*.save|All files (*.*)|*.*", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (saveDialog.ShowModal() == wxID_OK)
        {
            wxMessageBox("Writing save file as: " + saveDialog.GetPath());
        }
    }
    
    void OnReadSimLogfile(wxCommandEvent&)
    {
        wxFileDialog openDialog(this, "Read Sim Logfile", "", "", "Log files (*.log)|*.log|All files (*.*)|*.*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (openDialog.ShowModal() == wxID_OK)
        {
            wxMessageBox("Reading sim logfile: " + openDialog.GetPath());
        }
    }
    
    void OnReadVerilogStemsfile(wxCommandEvent&)
    {
        wxFileDialog openDialog(this, "Read Verilog Stemsfile", "", "", "Stems files (*.stems)|*.stems|All files (*.*)|*.*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (openDialog.ShowModal() == wxID_OK)
        {
            wxMessageBox("Reading Verilog stemsfile: " + openDialog.GetPath());
        }
    }
    
    void OnReadTclScriptFile(wxCommandEvent&)
    {
        wxFileDialog openDialog(this, "Read Tcl Script File", "", "", "Tcl files (*.tcl)|*.tcl|All files (*.*)|*.*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (openDialog.ShowModal() == wxID_OK)
        {
            wxMessageBox("Reading Tcl script file: " + openDialog.GetPath());
        }
    }
    
    void OnQuit(wxCommandEvent&)
    {
        Close();
    }
    
    // Export 子菜单功能实现
    void OnExportAsciiText(wxCommandEvent&)
    {
        wxFileDialog saveDialog(this, "Export ASCII Text", "", "waveform.txt", "Text files (*.txt)|*.txt|All files (*.*)|*.*", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (saveDialog.ShowModal() == wxID_OK)
        {
            wxString filePath = saveDialog.GetPath();
            wxFile file;
            if (file.Open(filePath, wxFile::write))
            {
                // 写入文件头
                file.Write("VCD Waveform Export\n");
                file.Write("====================\n");
                file.Write(wxString::Format("Export time: %s\n", wxDateTime::Now().Format().c_str()));
                file.Write(wxString::Format("Time range: %d - %d\n", m_wavePanel->m_timeOffset, m_wavePanel->m_timeOffset + m_wavePanel->m_displayTimeRange));
                file.Write("\n");
                
                // 写入信号数据
                for (size_t i = 0; i < m_wavePanel->m_displayedSignals2.size(); i++)
                {
                    signal_t* sig = m_wavePanel->m_displayedSignals2[i];
                    if (sig)
                    {
                        file.Write(wxString::Format("Signal: %s\n", sig->name));
                        file.Write("-" + wxString('=', 50) + "\n");
                        
                        // 写入信号值变化
                        int currentTime = m_wavePanel->m_timeOffset;
                        int endTime = m_wavePanel->m_timeOffset + m_wavePanel->m_displayTimeRange;
                        wxString lastValue = "";
                        
                        for (int t = currentTime; t <= endTime; t += std::max(1, m_wavePanel->m_displayTimeRange / 100))
                        {
                            const char* value = m_wavePanel->GetSignalValueAt(sig, t);
                        wxString valueStr = value;
                        if (valueStr != lastValue || t == currentTime)
                        {
                            file.Write(wxString::Format("%8d: %s\n", t, value));
                            lastValue = valueStr;
                        }
                        }
                        file.Write("\n");
                    }
                }
                
                file.Close();
                wxMessageBox("Successfully exported ASCII Text to: " + filePath);
            }
            else
            {
                wxMessageBox("Failed to open file for writing: " + filePath);
            }
        }
    }
    
    void OnExportVCD(wxCommandEvent&)
    {
        wxFileDialog saveDialog(this, "Export VCD", "", "waveform.vcd", "VCD files (*.vcd)|*.vcd|All files (*.*)|*.*", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (saveDialog.ShowModal() == wxID_OK)
        {
            wxMessageBox("Exporting VCD to: " + saveDialog.GetPath());
        }
    }
    
    void OnExportCSV(wxCommandEvent&)
    {
        wxFileDialog saveDialog(this, "Export CSV", "", "waveform.csv", "CSV files (*.csv)|*.csv|All files (*.*)|*.*", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (saveDialog.ShowModal() == wxID_OK)
        {
            wxString filePath = saveDialog.GetPath();
            wxFile file;
            if (file.Open(filePath, wxFile::write))
            {
                // 检查是否有信号可导出
                if (m_wavePanel->m_displayedSignals2.empty())
                {
                    file.Close();
                    wxMessageBox("No signals to export. Please add signals to the waveform first.");
                    return;
                }
                
                // 写入CSV表头
                file.Write("Time");
                for (size_t i = 0; i < m_wavePanel->m_displayedSignals2.size(); i++)
                {
                    signal_t* sig = m_wavePanel->m_displayedSignals2[i];
                    if (sig)
                    {
                        file.Write("," + wxString(sig->name));
                    }
                }
                file.Write("\n");
                
                // 写入数据
                int currentTime = m_wavePanel->m_timeOffset;
                int endTime = m_wavePanel->m_timeOffset + m_wavePanel->m_displayTimeRange;
                int step = std::max(1, m_wavePanel->m_displayTimeRange / 100);
                
                // 确保至少有一个时间点
                if (currentTime > endTime)
                {
                    currentTime = 0;
                    endTime = m_wavePanel->m_maxTimestamp;
                    step = std::max(1, endTime / 100);
                }
                
                for (int t = currentTime; t <= endTime; t += step)
                {
                    file.Write(wxString::Format("%d", t));
                    for (size_t i = 0; i < m_wavePanel->m_displayedSignals2.size(); i++)
                    {
                        signal_t* sig = m_wavePanel->m_displayedSignals2[i];
                        if (sig)
                        {
                            // 直接使用信号的value_changes来获取值，确保CSV信号也能正确处理
                            const char* value = "";
                            if (sig->changes_count > 0)
                            {
                                // 找到最后一个不大于当前时间的value change
                                size_t c;
                                for (c = 0; c < sig->changes_count; c++) {
                                    if (sig->value_changes[c].timestamp > t)
                                        break;
                                }
                                if (c > 0) {
                                    value = sig->value_changes[c - 1].value;
                                } else if (sig->changes_count > 0) {
                                    value = sig->value_changes[0].value;
                                }
                            }
                            file.Write("," + wxString(value));
                        }
                    }
                    file.Write("\n");
                }
                
                file.Close();
                wxMessageBox("Successfully exported CSV to: " + filePath);
            }
            else
            {
                wxMessageBox("Failed to open file for writing: " + filePath);
            }
        }
    }
    
    void OnExportPostScript(wxCommandEvent&)
    {
        wxFileDialog saveDialog(this, "Export PostScript", "", "waveform.ps", "PostScript files (*.ps)|*.ps|All files (*.*)|*.*", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (saveDialog.ShowModal() == wxID_OK)
        {
            wxMessageBox("Exporting PostScript to: " + saveDialog.GetPath());
        }
    }
    
    void OnExportPNG(wxCommandEvent&)
    {
        wxFileDialog saveDialog(this, "Export PNG", "", "waveform.png", "PNG files (*.png)|*.png|All files (*.*)|*.*", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (saveDialog.ShowModal() == wxID_OK)
        {
            wxMessageBox("Exporting PNG to: " + saveDialog.GetPath());
        }
    }
    
    void ProcessCSVData(CSVParser& parser)
    {
        if (!parser.HasHeaders() || parser.GetRowCount() == 0) {
            return;
        }
        
        std::vector<std::string> headers = parser.GetHeaders();
        
        // 假设第一列是时间
        std::string timeHeader = headers[0];
        
        // 为每个数据列创建信号
        for (size_t col = 1; col < headers.size(); col++) {
            const std::string& signalName = headers[col];
            
            // 创建信号
            signal_t* sig = new signal_t();
            strncpy(sig->name, signalName.c_str(), VCD_NAME_SIZE - 1);
            sig->name[VCD_NAME_SIZE - 1] = '\0';
            strncpy(sig->module_path, "CSV", VCD_SIGNAL_SIZE - 1);
            sig->module_path[VCD_SIGNAL_SIZE - 1] = '\0';
            snprintf(sig->full_name, VCD_SIGNAL_SIZE, "CSV.%s", signalName.c_str());
            snprintf(sig->signal_id, VCD_NAME_SIZE, "%s", signalName.c_str());
            sig->size = 1;
            sig->changes_count = 0;
            
            // 填充信号值变化
            for (size_t row = 0; row < parser.GetRowCount(); row++) {
                std::string timeStr = parser.GetValue(row, timeHeader);
                std::string valueStr = parser.GetValue(row, signalName);
                
                if (!timeStr.empty() && !valueStr.empty()) {
                    try {
                        int timestamp = std::stoi(timeStr);
                        
                        if (sig->changes_count < VCD_VALUE_CHANGE_COUNT) {
                            value_change_t& change = sig->value_changes[sig->changes_count];
                            change.timestamp = timestamp;
                            strncpy(change.value, valueStr.c_str(), VCD_SIGNAL_SIZE - 1);
                            change.value[VCD_SIGNAL_SIZE - 1] = '\0';
                            sig->changes_count++;
                        }
                    } catch (...) {
                        // 忽略无效的时间值
                    }
                }
            }
            
            if (sig->changes_count > 0) {
                // 只添加到模块映射，不自动添加到显示列表
                // 这样用户需要通过点击模块和信号列表来添加信号
                m_moduleToSignals["CSV"].push_back(sig);
                // 同时添加到m_displayedSignals，这样m_allSignals才能正确更新
                m_displayedSignals.push_back(sig);
            } else {
                delete sig;
            }
        }
        
        // 更新波形面板的信号列表
        m_wavePanel->m_allSignals = m_displayedSignals;
        m_wavePanel->m_maxTimestamp = 1000; // 默认时间范围
        m_wavePanel->AssignSignalColors();
        m_wavePanel->BuildDrawCacheAsync();
    }
    
    void OnOpenNewLab(wxCommandEvent&)
    {
        wxFileDialog dlg(this, "Open File", "", "", "VCD (*.vcd)|*.vcd|CSV (*.csv)|*.csv|All files (*.*)|*.*", wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dlg.ShowModal() == wxID_OK)
        {
            wxString filePath = dlg.GetPath();
            wxFileName fileName(filePath);
            wxString extension = fileName.GetExt().Lower();
            
            // 清空之前的信号
            m_displayedSignals.clear();
            m_wavePanel->ClearDisplaySignals();
            m_moduleToSignals.clear();
            
            if (extension == "vcd")
            {
                // 加载VCD文件
                m_wavePanel->OpenVCDFile(filePath);
                m_slider->SetRange(0, m_wavePanel->m_maxTimestamp);
                m_slider->SetValue(0);
                BuildSignalTreeView(m_signalTree, m_wavePanel->m_vcdData);
                m_signalList->DeleteAllItems();
                
                // 将所有信号传递给AI分析面板
                if (m_aiPanel && !m_wavePanel->m_allSignals.empty())
                {
                    m_aiPanel->SetSignalInfo(m_wavePanel->m_allSignals);
                }
                
                wxMessageBox("Successfully loaded VCD file: " + filePath);
            }
            else if (extension == "csv")
            {
                // 加载CSV文件
                CSVParser parser;
                if (parser.LoadFromFile(filePath.ToStdString()))
                {
                    // 处理CSV数据
                    ProcessCSVData(parser);
                    
                    // 更新信号树
                    BuildSignalTreeView(m_signalTree, m_wavePanel->m_vcdData);
                    m_signalList->DeleteAllItems();
                    
                    // 重置滑块范围
                    m_slider->SetRange(0, 1000);
                    m_slider->SetValue(0);
                    
                    // 将所有信号传递给AI分析面板
                    if (m_aiPanel && !m_displayedSignals.empty())
                    {
                        m_aiPanel->SetSignalInfo(m_displayedSignals);
                    }
                    
                    wxMessageBox("Successfully loaded CSV file: " + filePath);
                }
                else
                {
                    wxMessageBox("Failed to load CSV file: " + filePath);
                }
            }
            else
            {
                wxMessageBox("Unsupported file format. Please select a VCD or CSV file.");
            }
        }
    }
    
    void OnExportSVG(wxCommandEvent&)
    {
        wxFileDialog saveDialog(this, "Export SVG", "", "waveform.svg", "SVG files (*.svg)|*.svg|All files (*.*)|*.*", wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (saveDialog.ShowModal() == wxID_OK)
        {
            wxMessageBox("Exporting SVG to: " + saveDialog.GetPath());
        }
    }
    
    // ===== Time 菜单功能实现 =====
    
    // 保存上一次的时间范围，用于Zoom Last
    int m_lastTimeOffset = 0;
    int m_lastDisplayTimeRange = 1000;
    
    void SaveCurrentView()
    {
        m_lastTimeOffset = m_wavePanel->m_timeOffset;
        m_lastDisplayTimeRange = m_wavePanel->m_displayTimeRange;
    }
    
    void OnMoveToTime(wxCommandEvent&)
    {
        wxString val = wxGetTextFromUser("Enter time:", "Move To Time", 
            wxString::Format("%d", m_wavePanel->m_timeOffset));
        long t;
        if (val.ToLong(&t))
        {
            SaveCurrentView();
            m_wavePanel->m_timeOffset = t;
            if (m_wavePanel->m_timeOffset < 0) m_wavePanel->m_timeOffset = 0;
            if (m_wavePanel->m_timeOffset > m_wavePanel->m_maxTimestamp - m_wavePanel->m_displayTimeRange)
                m_wavePanel->m_timeOffset = m_wavePanel->m_maxTimestamp - m_wavePanel->m_displayTimeRange;
            m_wavePanel->BuildDrawCacheAsync();
            m_wavePanel->Refresh();
            SyncTimeRangeUI();
        }
    }
    
    void OnZoomIn(wxCommandEvent&)
    {
        SaveCurrentView();
        m_wavePanel->ZoomIn();
        SyncTimeRangeUI();
    }
    
    void OnZoomOut(wxCommandEvent&)
    {
        SaveCurrentView();
        m_wavePanel->ZoomOut();
        SyncTimeRangeUI();
    }
    
    void OnZoomFull(wxCommandEvent&)
    {
        SaveCurrentView();
        m_wavePanel->m_timeOffset = 0;
        m_wavePanel->m_displayTimeRange = m_wavePanel->m_maxTimestamp;
        m_wavePanel->BuildDrawCacheAsync();
        m_wavePanel->Refresh();
        SyncTimeRangeUI();
    }
    
    void OnZoomLast(wxCommandEvent&)
    {
        int tempOffset = m_wavePanel->m_timeOffset;
        int tempRange = m_wavePanel->m_displayTimeRange;
        
        m_wavePanel->m_timeOffset = m_lastTimeOffset;
        m_wavePanel->m_displayTimeRange = m_lastDisplayTimeRange;
        
        m_lastTimeOffset = tempOffset;
        m_lastDisplayTimeRange = tempRange;
        
        m_wavePanel->BuildDrawCacheAsync();
        m_wavePanel->Refresh();
        SyncTimeRangeUI();
    }
    
    void OnFetchMore(wxCommandEvent&)
    {
        SaveCurrentView();
        int center = m_wavePanel->m_timeOffset + m_wavePanel->m_displayTimeRange / 2;
        m_wavePanel->m_displayTimeRange *= 2;
        if (m_wavePanel->m_displayTimeRange > m_wavePanel->m_maxTimestamp)
            m_wavePanel->m_displayTimeRange = m_wavePanel->m_maxTimestamp;
        m_wavePanel->m_timeOffset = center - m_wavePanel->m_displayTimeRange / 2;
        if (m_wavePanel->m_timeOffset < 0) m_wavePanel->m_timeOffset = 0;
        m_wavePanel->BuildDrawCacheAsync();
        m_wavePanel->Refresh();
        SyncTimeRangeUI();
    }
    
    void OnFetchAll(wxCommandEvent&)
    {
        SaveCurrentView();
        m_wavePanel->m_timeOffset = 0;
        m_wavePanel->m_displayTimeRange = m_wavePanel->m_maxTimestamp;
        m_wavePanel->BuildDrawCacheAsync();
        m_wavePanel->Refresh();
        SyncTimeRangeUI();
    }
    
    void OnDiscardToStart(wxCommandEvent&)
    {
        SaveCurrentView();
        int newStart = m_wavePanel->m_timeOffset + m_wavePanel->m_displayTimeRange / 4;
        if (newStart < m_wavePanel->m_maxTimestamp)
        {
            m_wavePanel->m_timeOffset = newStart;
            m_wavePanel->m_displayTimeRange = m_wavePanel->m_maxTimestamp - newStart;
            m_wavePanel->BuildDrawCacheAsync();
            m_wavePanel->Refresh();
            SyncTimeRangeUI();
        }
    }
    
    void OnDiscardToEnd(wxCommandEvent&)
    {
        SaveCurrentView();
        int newEnd = m_wavePanel->m_timeOffset + m_wavePanel->m_displayTimeRange * 3 / 4;
        if (newEnd > 0)
        {
            m_wavePanel->m_displayTimeRange = newEnd;
            m_wavePanel->BuildDrawCacheAsync();
            m_wavePanel->Refresh();
            SyncTimeRangeUI();
        }
    }
    
    void OnShiftLeft(wxCommandEvent&)
    {
        SaveCurrentView();
        int shift = m_wavePanel->m_displayTimeRange / 4;
        m_wavePanel->m_timeOffset -= shift;
        if (m_wavePanel->m_timeOffset < 0) m_wavePanel->m_timeOffset = 0;
        m_wavePanel->BuildDrawCacheAsync();
        m_wavePanel->Refresh();
        SyncTimeRangeUI();
    }
    
    void OnShiftRight(wxCommandEvent&)
    {
        SaveCurrentView();
        int shift = m_wavePanel->m_displayTimeRange / 4;
        m_wavePanel->m_timeOffset += shift;
        if (m_wavePanel->m_timeOffset + m_wavePanel->m_displayTimeRange > m_wavePanel->m_maxTimestamp)
            m_wavePanel->m_timeOffset = m_wavePanel->m_maxTimestamp - m_wavePanel->m_displayTimeRange;
        m_wavePanel->BuildDrawCacheAsync();
        m_wavePanel->Refresh();
        SyncTimeRangeUI();
    }
    
    void OnPageLeft(wxCommandEvent&)
    {
        SaveCurrentView();
        m_wavePanel->m_timeOffset -= m_wavePanel->m_displayTimeRange;
        if (m_wavePanel->m_timeOffset < 0) m_wavePanel->m_timeOffset = 0;
        m_wavePanel->BuildDrawCacheAsync();
        m_wavePanel->Refresh();
        SyncTimeRangeUI();
    }
    
    void OnPageRight(wxCommandEvent&)
    {
        SaveCurrentView();
        m_wavePanel->m_timeOffset += m_wavePanel->m_displayTimeRange;
        if (m_wavePanel->m_timeOffset + m_wavePanel->m_displayTimeRange > m_wavePanel->m_maxTimestamp)
            m_wavePanel->m_timeOffset = m_wavePanel->m_maxTimestamp - m_wavePanel->m_displayTimeRange;
        m_wavePanel->BuildDrawCacheAsync();
        m_wavePanel->Refresh();
        SyncTimeRangeUI();
    }

    // Edit 菜单事件处理函数
    void OnSetTraceMaxHier(wxCommandEvent&)
    {
        wxString currentValue = wxString::Format("%d", m_wavePanel->m_traceMaxHier);
        wxString newMaxHier = wxGetTextFromUser("Enter maximum trace hierarchy level:", "Set Trace Max Hier", currentValue, this);
        if (!newMaxHier.IsEmpty()) {
            long value;
            if (newMaxHier.ToLong(&value) && value > 0) {
                m_wavePanel->m_traceMaxHier = value;
                m_wavePanel->Refresh();
            }
        }
    }
    
    void OnToggleTraceHier(wxCommandEvent&)
    {
        m_wavePanel->m_showTraceHier = !m_wavePanel->m_showTraceHier;
        m_wavePanel->Refresh();
    }
    
    void OnInsertBlank(wxCommandEvent&)
    {
        // 插入一个空信号到选中位置
        m_wavePanel->m_displayedSignals2.insert(m_wavePanel->m_displayedSignals2.begin() + (m_wavePanel->m_selectedSignalIndex >= 0 ? m_wavePanel->m_selectedSignalIndex : m_wavePanel->m_displayedSignals2.size()), nullptr);
        m_wavePanel->BuildDrawCacheAsync();
        m_wavePanel->Refresh();
    }
    
    void OnInsertComment(wxCommandEvent&)
    {
        wxString commentText = wxGetTextFromUser("Enter comment text:", "Insert Comment", "", this);
        if (!commentText.IsEmpty()) {
            WaveformPanel::Comment comment;
            comment.position = m_wavePanel->m_selectedSignalIndex >= 0 ? m_wavePanel->m_selectedSignalIndex : m_wavePanel->m_displayedSignals2.size();
            comment.text = commentText;
            m_wavePanel->m_comments.push_back(comment);
            m_wavePanel->Refresh();
        }
    }
    
    void OnInsertAnalogHeightExtension(wxCommandEvent&)
    {
        // 模拟信号高度扩展功能
        wxString heightStr = wxGetTextFromUser("Enter analog height extension (0-100):", "Analog Height Extension", wxString::Format("%d", m_wavePanel->m_analogHeightExtension), this);
        if (!heightStr.IsEmpty()) {
            long height;
            if (heightStr.ToLong(&height) && height >= 0 && height <= 100) {
                m_wavePanel->m_analogHeightExtension = height;
                m_wavePanel->Refresh();
                wxMessageBox(wxString::Format("Analog height extension set to %d%%.", height));
            } else {
                wxMessageBox("Invalid height value. Please enter a value between 0 and 100.");
            }
        }
    }
    
    void OnCut(wxCommandEvent&)
    {
        if (m_wavePanel->m_selectedSignalIndex >= 0 && m_wavePanel->m_selectedSignalIndex < (int)m_wavePanel->m_displayedSignals2.size()) {
            signal_t* sig = m_wavePanel->m_displayedSignals2[m_wavePanel->m_selectedSignalIndex];
            if (sig) {
                // 复制到剪贴板
                m_wavePanel->m_clipboard.clear();
                m_wavePanel->m_clipboard.push_back(sig);
                // 从显示列表中删除
                m_wavePanel->m_displayedSignals2.erase(m_wavePanel->m_displayedSignals2.begin() + m_wavePanel->m_selectedSignalIndex);
                m_wavePanel->m_selectedSignalIndex = -1;
                m_wavePanel->BuildDrawCacheAsync();
                m_wavePanel->Refresh();
            }
        }
    }
    
    void OnCopy(wxCommandEvent&)
    {
        if (m_wavePanel->m_selectedSignalIndex >= 0 && m_wavePanel->m_selectedSignalIndex < (int)m_wavePanel->m_displayedSignals2.size()) {
            signal_t* sig = m_wavePanel->m_displayedSignals2[m_wavePanel->m_selectedSignalIndex];
            if (sig) {
                // 复制到剪贴板
                m_wavePanel->m_clipboard.clear();
                m_wavePanel->m_clipboard.push_back(sig);
            }
        }
    }
    
    void OnPaste(wxCommandEvent&)
    {
        if (!m_wavePanel->m_clipboard.empty()) {
            // 粘贴到选中位置
            int insertPos = m_wavePanel->m_selectedSignalIndex >= 0 ? m_wavePanel->m_selectedSignalIndex : m_wavePanel->m_displayedSignals2.size();
            for (auto sig : m_wavePanel->m_clipboard) {
                if (sig) {
                    m_wavePanel->m_displayedSignals2.insert(m_wavePanel->m_displayedSignals2.begin() + insertPos, sig);
                    insertPos++;
                }
            }
            m_wavePanel->BuildDrawCacheAsync();
            m_wavePanel->Refresh();
        }
    }
    
    void OnDelete(wxCommandEvent&)
    {
        if (m_wavePanel->m_selectedSignalIndex >= 0 && m_wavePanel->m_selectedSignalIndex < (int)m_wavePanel->m_displayedSignals2.size()) {
            // 从显示列表中删除
            m_wavePanel->m_displayedSignals2.erase(m_wavePanel->m_displayedSignals2.begin() + m_wavePanel->m_selectedSignalIndex);
            m_wavePanel->m_selectedSignalIndex = -1;
            m_wavePanel->BuildDrawCacheAsync();
            m_wavePanel->Refresh();
        }
    }
    
    void OnAliasHighlightedTrace(wxCommandEvent&)
    {
        if (m_wavePanel->m_selectedSignalIndex >= 0 && m_wavePanel->m_selectedSignalIndex < (int)m_wavePanel->m_displayedSignals2.size()) {
            signal_t* sig = m_wavePanel->m_displayedSignals2[m_wavePanel->m_selectedSignalIndex];
            if (sig) {
                wxString currentAlias;
                auto aliasIt = m_wavePanel->m_signalAliases.find(sig);
                if (aliasIt != m_wavePanel->m_signalAliases.end()) {
                    currentAlias = aliasIt->second;
                }
                wxString newAlias = wxGetTextFromUser("Enter alias for signal:", "Alias Highlighted Trace", currentAlias, this);
                if (!newAlias.IsEmpty()) {
                    m_wavePanel->m_signalAliases[sig] = newAlias;
                    m_wavePanel->Refresh();
                }
            }
        }
    }
    
    void OnRemoveHighlightedAliases(wxCommandEvent&)
    {
        if (m_wavePanel->m_selectedSignalIndex >= 0 && m_wavePanel->m_selectedSignalIndex < (int)m_wavePanel->m_displayedSignals2.size()) {
            signal_t* sig = m_wavePanel->m_displayedSignals2[m_wavePanel->m_selectedSignalIndex];
            if (sig) {
                m_wavePanel->m_signalAliases.erase(sig);
                m_wavePanel->Refresh();
            }
        }
    }
    
    void OnExpand(wxCommandEvent&)
    {
        // 展开信号功能
        if (m_wavePanel->m_selectedSignalIndex >= 0) {
            // 检查是否是组合信号
            bool isInGroup = false;
            int groupIndex = -1;
            
            for (size_t i = 0; i < m_wavePanel->m_signalGroups.size(); i++) {
                auto& group = m_wavePanel->m_signalGroups[i];
                if (!group.isExpanded) {
                    // 检查选中的信号是否是组合信号的第一个信号
                    if (m_wavePanel->m_displayedSignals2[m_wavePanel->m_selectedSignalIndex] == group.signals[0]) {
                        isInGroup = true;
                        groupIndex = i;
                        break;
                    }
                }
            }
            
            if (isInGroup && groupIndex >= 0) {
                // 展开组合信号
                auto& group = m_wavePanel->m_signalGroups[groupIndex];
                group.isExpanded = true;
                
                // 在显示列表中插入组合的信号
                int insertPos = m_wavePanel->m_selectedSignalIndex + 1;
                for (size_t i = 1; i < group.signals.size(); i++) {
                    m_wavePanel->m_displayedSignals2.insert(m_wavePanel->m_displayedSignals2.begin() + insertPos, group.signals[i]);
                    insertPos++;
                }
                
                m_wavePanel->BuildDrawCacheAsync();
                m_wavePanel->Refresh();
            } else {
                wxMessageBox("No group to expand at selected position.");
            }
        } else {
            wxMessageBox("Please select a signal to expand.");
        }
    }
    
    void OnCombineDown(wxCommandEvent&)
    {
        // 向下组合信号
        if (m_wavePanel->m_selectedSignalIndex >= 0 && m_wavePanel->m_selectedSignalIndex < (int)m_wavePanel->m_displayedSignals2.size() - 1) {
            // 创建新的组合
            WaveformPanel::WaveformSignalGroup group;
            group.signals.push_back(m_wavePanel->m_displayedSignals2[m_wavePanel->m_selectedSignalIndex]);
            group.signals.push_back(m_wavePanel->m_displayedSignals2[m_wavePanel->m_selectedSignalIndex + 1]);
            group.name = wxString::Format("Group_%d", (int)m_wavePanel->m_signalGroups.size() + 1);
            group.isExpanded = false;
            
            m_wavePanel->m_signalGroups.push_back(group);
            
            // 从显示列表中移除第二个信号
            m_wavePanel->m_displayedSignals2.erase(m_wavePanel->m_displayedSignals2.begin() + m_wavePanel->m_selectedSignalIndex + 1);
            
            m_wavePanel->BuildDrawCacheAsync();
            m_wavePanel->Refresh();
        } else {
            wxMessageBox("Please select a signal with another signal below it to combine.");
        }
    }
    
    void OnCombineUp(wxCommandEvent&)
    {
        // 向上组合信号
        if (m_wavePanel->m_selectedSignalIndex > 0 && m_wavePanel->m_selectedSignalIndex < (int)m_wavePanel->m_displayedSignals2.size()) {
            // 创建新的组合
            WaveformPanel::WaveformSignalGroup group;
            group.signals.push_back(m_wavePanel->m_displayedSignals2[m_wavePanel->m_selectedSignalIndex - 1]);
            group.signals.push_back(m_wavePanel->m_displayedSignals2[m_wavePanel->m_selectedSignalIndex]);
            group.name = wxString::Format("Group_%d", (int)m_wavePanel->m_signalGroups.size() + 1);
            group.isExpanded = false;
            
            m_wavePanel->m_signalGroups.push_back(group);
            
            // 从显示列表中移除当前信号
            m_wavePanel->m_displayedSignals2.erase(m_wavePanel->m_displayedSignals2.begin() + m_wavePanel->m_selectedSignalIndex);
            m_wavePanel->m_selectedSignalIndex--;
            
            m_wavePanel->BuildDrawCacheAsync();
            m_wavePanel->Refresh();
        } else {
            wxMessageBox("Please select a signal with another signal above it to combine.");
        }
    }
    
    void OnShowChangeAllHighlighted(wxCommandEvent&)
    {
        // 显示/更改所有高亮信号
        if (m_wavePanel->m_highlightedSignals.empty()) {
            wxMessageBox("No signals are highlighted.");
        } else {
            wxMessageBox(wxString::Format("There are %d highlighted signals.", (int)m_wavePanel->m_highlightedSignals.size()));
            // 这里可以添加更多功能，比如显示一个对话框来更改所有高亮信号的属性
        }
    }
    
    void OnShowChangeAll(wxCommandEvent&)
    {
        // 显示/更改所有信号
        wxMessageBox(wxString::Format("There are %d signals in total.", (int)m_wavePanel->m_allSignals.size()));
        // 这里可以添加更多功能，比如显示一个对话框来更改所有信号的属性
    }
    
    void OnExclude(wxCommandEvent&)
    {
        // 排除信号
        if (m_wavePanel->m_selectedSignalIndex >= 0 && m_wavePanel->m_selectedSignalIndex < (int)m_wavePanel->m_displayedSignals2.size()) {
            signal_t* sig = m_wavePanel->m_displayedSignals2[m_wavePanel->m_selectedSignalIndex];
            if (sig) {
                m_wavePanel->m_excludedSignals.insert(sig);
                m_wavePanel->m_displayedSignals2.erase(m_wavePanel->m_displayedSignals2.begin() + m_wavePanel->m_selectedSignalIndex);
                m_wavePanel->m_selectedSignalIndex = -1;
                m_wavePanel->BuildDrawCacheAsync();
                m_wavePanel->Refresh();
                wxMessageBox("Signal excluded.");
            }
        } else {
            wxMessageBox("Please select a signal to exclude.");
        }
    }
    
    void OnShow(wxCommandEvent&)
    {
        // 显示信号
        if (!m_wavePanel->m_excludedSignals.empty()) {
            wxArrayString excludedSignalNames;
            std::vector<signal_t*> excludedSignalsList;
            
            for (auto sig : m_wavePanel->m_excludedSignals) {
                excludedSignalNames.Add(sig->full_name);
                excludedSignalsList.push_back(sig);
            }
            
            int selection = wxGetSingleChoiceIndex("Select a signal to show:", "Show Signal", excludedSignalNames);
            if (selection >= 0) {
                signal_t* sig = excludedSignalsList[selection];
                m_wavePanel->m_excludedSignals.erase(sig);
                m_wavePanel->m_displayedSignals2.push_back(sig);
                m_wavePanel->BuildDrawCacheAsync();
                m_wavePanel->Refresh();
                wxMessageBox("Signal shown.");
            }
        } else {
            wxMessageBox("No signals are excluded.");
        }
    }
    
    void OnToggleGroupOpenClose(wxCommandEvent&)
    {
        // 切换组的打开/关闭状态
        if (m_wavePanel->m_selectedSignalIndex >= 0) {
            // 检查是否是组合信号
            bool isInGroup = false;
            int groupIndex = -1;
            
            for (size_t i = 0; i < m_wavePanel->m_signalGroups.size(); i++) {
                auto& group = m_wavePanel->m_signalGroups[i];
                // 检查选中的信号是否是组合信号的第一个信号
                if (group.signals.size() > 0 && group.signals[0] == m_wavePanel->m_displayedSignals2[m_wavePanel->m_selectedSignalIndex]) {
                    isInGroup = true;
                    groupIndex = i;
                    break;
                }
            }
            
            if (isInGroup && groupIndex >= 0) {
                auto& group = m_wavePanel->m_signalGroups[groupIndex];
                if (group.isExpanded) {
                    // 关闭组
                    group.isExpanded = false;
                    // 从显示列表中移除组合的信号
                    int removeCount = 0;
                    for (size_t i = 1; i < group.signals.size(); i++) {
                        auto it = std::find(m_wavePanel->m_displayedSignals2.begin() + m_wavePanel->m_selectedSignalIndex + 1, 
                                           m_wavePanel->m_displayedSignals2.end(), 
                                           group.signals[i]);
                        if (it != m_wavePanel->m_displayedSignals2.end()) {
                            m_wavePanel->m_displayedSignals2.erase(it);
                            removeCount++;
                        }
                    }
                } else {
                    // 打开组
                    group.isExpanded = true;
                    // 在显示列表中插入组合的信号
                    int insertPos = m_wavePanel->m_selectedSignalIndex + 1;
                    for (size_t i = 1; i < group.signals.size(); i++) {
                        m_wavePanel->m_displayedSignals2.insert(m_wavePanel->m_displayedSignals2.begin() + insertPos, group.signals[i]);
                        insertPos++;
                    }
                }
                
                m_wavePanel->BuildDrawCacheAsync();
                m_wavePanel->Refresh();
            } else {
                wxMessageBox("No group found at selected position.");
            }
        } else {
            wxMessageBox("Please select a signal to toggle group.");
        }
    }
    
    void OnCreateGroup(wxCommandEvent&)
    {
        // 创建信号组
        if (!m_wavePanel->m_displayedSignals2.empty()) {
            wxString groupName = wxGetTextFromUser("Enter group name:", "Create Group", wxString::Format("Group_%d", (int)m_wavePanel->m_signalGroups.size() + 1), this);
            if (!groupName.IsEmpty()) {
                // 创建新的组合
                WaveformPanel::WaveformSignalGroup group;
                group.signals = m_wavePanel->m_displayedSignals2;
                group.name = groupName;
                group.isExpanded = true;
                
                m_wavePanel->m_signalGroups.push_back(group);
                
                wxMessageBox(wxString::Format("Group '%s' created with %d signals.", groupName, (int)group.signals.size()));
            }
        } else {
            wxMessageBox("No signals to create a group with.");
        }
    }
    
    void OnHighlightRegexp(wxCommandEvent&)
    {
        // 通过正则表达式高亮信号
        wxString regexp = wxGetTextFromUser("Enter regular expression:", "Highlight Regexp", "", this);
        if (!regexp.IsEmpty()) {
            m_wavePanel->m_highlightedSignals.clear();
            for (auto sig : m_wavePanel->m_allSignals) {
                if (wxRegEx(regexp).Matches(sig->full_name)) {
                    m_wavePanel->m_highlightedSignals.insert(sig);
                }
            }
            wxMessageBox(wxString::Format("Highlighted %d signals matching regexp.", (int)m_wavePanel->m_highlightedSignals.size()));
        }
    }
    
    void OnHighlightAll(wxCommandEvent&)
    {
        // 高亮所有信号
        m_wavePanel->m_highlightedSignals.clear();
        for (auto sig : m_wavePanel->m_allSignals) {
            m_wavePanel->m_highlightedSignals.insert(sig);
        }
        wxMessageBox(wxString::Format("Highlighted all %d signals.", (int)m_wavePanel->m_highlightedSignals.size()));
    }
    
    void OnUnHighlightAll(wxCommandEvent&)
    {
        // 取消所有高亮
        m_wavePanel->m_highlightedSignals.clear();
        wxMessageBox("All signals unhighlighted.");
    }
    
    // Data Format 子菜单事件处理函数
    void OnDataFormatBinary(wxCommandEvent&)
    {
        if (m_wavePanel->m_selectedSignalIndex >= 0 && m_wavePanel->m_selectedSignalIndex < (int)m_wavePanel->m_displayedSignals2.size()) {
            signal_t* sig = m_wavePanel->m_displayedSignals2[m_wavePanel->m_selectedSignalIndex];
            if (sig) {
                m_wavePanel->m_signalDataFormats[sig] = WaveformPanel::FORMAT_BINARY;
                m_wavePanel->BuildDrawCacheAsync();
                m_wavePanel->Refresh();
            }
        }
    }
    
    void OnDataFormatOctal(wxCommandEvent&)
    {
        if (m_wavePanel->m_selectedSignalIndex >= 0 && m_wavePanel->m_selectedSignalIndex < (int)m_wavePanel->m_displayedSignals2.size()) {
            signal_t* sig = m_wavePanel->m_displayedSignals2[m_wavePanel->m_selectedSignalIndex];
            if (sig) {
                m_wavePanel->m_signalDataFormats[sig] = WaveformPanel::FORMAT_OCTAL;
                m_wavePanel->BuildDrawCacheAsync();
                m_wavePanel->Refresh();
            }
        }
    }
    
    void OnDataFormatDecimal(wxCommandEvent&)
    {
        if (m_wavePanel->m_selectedSignalIndex >= 0 && m_wavePanel->m_selectedSignalIndex < (int)m_wavePanel->m_displayedSignals2.size()) {
            signal_t* sig = m_wavePanel->m_displayedSignals2[m_wavePanel->m_selectedSignalIndex];
            if (sig) {
                m_wavePanel->m_signalDataFormats[sig] = WaveformPanel::FORMAT_DECIMAL;
                m_wavePanel->BuildDrawCacheAsync();
                m_wavePanel->Refresh();
            }
        }
    }
    
    void OnDataFormatHexadecimal(wxCommandEvent&)
    {
        if (m_wavePanel->m_selectedSignalIndex >= 0 && m_wavePanel->m_selectedSignalIndex < (int)m_wavePanel->m_displayedSignals2.size()) {
            signal_t* sig = m_wavePanel->m_displayedSignals2[m_wavePanel->m_selectedSignalIndex];
            if (sig) {
                m_wavePanel->m_signalDataFormats[sig] = WaveformPanel::FORMAT_HEXADECIMAL;
                m_wavePanel->BuildDrawCacheAsync();
                m_wavePanel->Refresh();
            }
        }
    }
    
    void OnDataFormatASCII(wxCommandEvent&)
    {
        if (m_wavePanel->m_selectedSignalIndex >= 0 && m_wavePanel->m_selectedSignalIndex < (int)m_wavePanel->m_displayedSignals2.size()) {
            signal_t* sig = m_wavePanel->m_displayedSignals2[m_wavePanel->m_selectedSignalIndex];
            if (sig) {
                m_wavePanel->m_signalDataFormats[sig] = WaveformPanel::FORMAT_ASCII;
                m_wavePanel->BuildDrawCacheAsync();
                m_wavePanel->Refresh();
            }
        }
    }
    
    void OnDataFormatSignedDecimal(wxCommandEvent&)
    {
        if (m_wavePanel->m_selectedSignalIndex >= 0 && m_wavePanel->m_selectedSignalIndex < (int)m_wavePanel->m_displayedSignals2.size()) {
            signal_t* sig = m_wavePanel->m_displayedSignals2[m_wavePanel->m_selectedSignalIndex];
            if (sig) {
                m_wavePanel->m_signalDataFormats[sig] = WaveformPanel::FORMAT_SIGNED_DECIMAL;
                m_wavePanel->BuildDrawCacheAsync();
                m_wavePanel->Refresh();
            }
        }
    }
    
    void OnDataFormatReal(wxCommandEvent&)
    {
        if (m_wavePanel->m_selectedSignalIndex >= 0 && m_wavePanel->m_selectedSignalIndex < (int)m_wavePanel->m_displayedSignals2.size()) {
            signal_t* sig = m_wavePanel->m_displayedSignals2[m_wavePanel->m_selectedSignalIndex];
            if (sig) {
                m_wavePanel->m_signalDataFormats[sig] = WaveformPanel::FORMAT_REAL;
                m_wavePanel->BuildDrawCacheAsync();
                m_wavePanel->Refresh();
            }
        }
    }
    
    // Color Format 子菜单事件处理函数
    void OnColorFormatDefault(wxCommandEvent&)
    {
        m_wavePanel->m_globalColorFormat = WaveformPanel::COLOR_DEFAULT;
        m_wavePanel->AssignSignalColors();
        m_wavePanel->BuildDrawCacheAsync();
        m_wavePanel->Refresh();
    }
    
    void OnColorFormatSignalName(wxCommandEvent&)
    {
        m_wavePanel->m_globalColorFormat = WaveformPanel::COLOR_SIGNAL_NAME;
        m_wavePanel->AssignSignalColors();
        m_wavePanel->BuildDrawCacheAsync();
        m_wavePanel->Refresh();
    }
    
    void OnColorFormatValue(wxCommandEvent&)
    {
        m_wavePanel->m_globalColorFormat = WaveformPanel::COLOR_VALUE;
        m_wavePanel->AssignSignalColors();
        m_wavePanel->BuildDrawCacheAsync();
        m_wavePanel->Refresh();
    }
    
    void OnColorFormatModule(wxCommandEvent&)
    {
        m_wavePanel->m_globalColorFormat = WaveformPanel::COLOR_MODULE;
        m_wavePanel->AssignSignalColors();
        m_wavePanel->BuildDrawCacheAsync();
        m_wavePanel->Refresh();
    }
    
    // Time Warp 子菜单事件处理函数
    void OnTimeWarpEnable(wxCommandEvent&)
    {
        m_wavePanel->m_timeWarpEnabled = true;
        wxMessageBox("Time Warp enabled.");
    }
    
    void OnTimeWarpDisable(wxCommandEvent&)
    {
        m_wavePanel->m_timeWarpEnabled = false;
        wxMessageBox("Time Warp disabled.");
    }
    
    void OnTimeWarpSet(wxCommandEvent&)
    {
        wxString factorStr = wxGetTextFromUser("Enter time warp factor:", "Time Warp: Set", wxString::Format("%.2f", m_wavePanel->m_timeWarpFactor), this);
        double factor;
        if (factorStr.ToDouble(&factor) && factor > 0) {
            m_wavePanel->m_timeWarpFactor = factor;
            wxMessageBox(wxString::Format("Time Warp factor set to %.2f.", factor));
        } else {
            wxMessageBox("Invalid time warp factor.");
        }
    }
    
    // Sort 子菜单事件处理函数
    void OnSortByName(wxCommandEvent&)
    {
        // 按名称排序
        std::sort(m_wavePanel->m_displayedSignals2.begin(), m_wavePanel->m_displayedSignals2.end(), [](signal_t* a, signal_t* b) {
            return strcmp(a->full_name, b->full_name) < 0;
        });
        m_wavePanel->m_selectedSignalIndex = -1;
        m_wavePanel->BuildDrawCacheAsync();
        m_wavePanel->Refresh();
        wxMessageBox("Signals sorted by name.");
    }
    
    void OnSortByGroup(wxCommandEvent&)
    {
        // 按组排序
        if (m_wavePanel->m_signalGroups.empty()) {
            wxMessageBox("No signal groups to sort by.");
            return;
        }
        
        // 复制当前显示的信号
        std::vector<signal_t*> currentSignals = m_wavePanel->m_displayedSignals2;
        
        // 清空显示列表
        m_wavePanel->m_displayedSignals2.clear();
        
        // 按组名排序信号组
        std::vector<WaveformPanel::WaveformSignalGroup> sortedGroups = m_wavePanel->m_signalGroups;
        std::sort(sortedGroups.begin(), sortedGroups.end(), [](const WaveformPanel::WaveformSignalGroup& a, const WaveformPanel::WaveformSignalGroup& b) {
            return a.name < b.name;
        });
        
        // 将每个组内的信号添加到显示列表
        for (auto& group : sortedGroups) {
            for (signal_t* sig : group.signals) {
                m_wavePanel->m_displayedSignals2.push_back(sig);
            }
        }
        
        // 添加不属于任何组的信号
        for (signal_t* sig : currentSignals) {
            if (!sig) continue; // 跳过空信号
            bool inGroup = false;
            for (auto& group : m_wavePanel->m_signalGroups) {
                if (std::find(group.signals.begin(), group.signals.end(), sig) != group.signals.end()) {
                    inGroup = true;
                    break;
                }
            }
            if (!inGroup) {
                m_wavePanel->m_displayedSignals2.push_back(sig);
            }
        }
        
        // 重新构建绘制缓存
        m_wavePanel->BuildDrawCacheAsync();
        m_wavePanel->Refresh();
        
        wxMessageBox("Signals sorted by group.");
    }
    
    void OnSortByValue(wxCommandEvent&)
    {
        // 按值排序
        if (m_wavePanel->m_displayedSignals2.empty()) {
            wxMessageBox("No signals to sort.");
            return;
        }
        
        int currentTime = m_wavePanel->GetCurrentTime();
        
        // 按信号当前值排序
        std::sort(m_wavePanel->m_displayedSignals2.begin(), m_wavePanel->m_displayedSignals2.end(), [&](signal_t* a, signal_t* b) {
            std::string valA = m_wavePanel->GetValueAt(a, currentTime);
            std::string valB = m_wavePanel->GetValueAt(b, currentTime);
            return valA < valB;
        });
        
        // 重新构建绘制缓存
        m_wavePanel->BuildDrawCacheAsync();
        m_wavePanel->Refresh();
        
        wxMessageBox("Signals sorted by value.");
    }
    
    void OnSortByModule(wxCommandEvent&)
    {
        // 按模块排序
        if (m_wavePanel->m_displayedSignals2.empty()) {
            wxMessageBox("No signals to sort.");
            return;
        }
        std::sort(m_wavePanel->m_displayedSignals2.begin(), m_wavePanel->m_displayedSignals2.end(), [](signal_t* a, signal_t* b) {
            return strcmp(a->module_path, b->module_path) < 0;
        });
        m_wavePanel->m_selectedSignalIndex = -1;
        m_wavePanel->BuildDrawCacheAsync();
        m_wavePanel->Refresh();
        wxMessageBox("Signals sorted by module.");
    }
    
    // Markers 菜单事件处理函数
    void OnShowChangeMarkerData(wxCommandEvent&)
    {
        // 显示/更改标记数据
        if (m_wavePanel->m_markers.empty()) {
            wxMessageBox("No markers to show.");
            return;
        }
        
        // 创建一个对话框来显示和编辑标记数据
        wxDialog* dialog = new wxDialog(this, wxID_ANY, "Marker Data", wxDefaultPosition, wxSize(400, 300));
        wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
        
        // 创建一个列表框来显示标记
        wxListBox* markerList = new wxListBox(dialog, wxID_ANY, wxDefaultPosition, wxSize(-1, 150));
        for (size_t i = 0; i < m_wavePanel->m_markers.size(); i++) {
            auto& mk = m_wavePanel->m_markers[i];
            markerList->Append(wxString::Format("%s: %d", mk.label, mk.timestamp));
        }
        mainSizer->Add(markerList, 1, wxEXPAND | wxALL, 10);
        
        // 添加编辑控件
        wxBoxSizer* editSizer = new wxBoxSizer(wxHORIZONTAL);
        wxStaticText* nameLabel = new wxStaticText(dialog, wxID_ANY, "Name:");
        wxTextCtrl* nameCtrl = new wxTextCtrl(dialog, wxID_ANY);
        wxStaticText* timeLabel = new wxStaticText(dialog, wxID_ANY, "Time:");
        wxTextCtrl* timeCtrl = new wxTextCtrl(dialog, wxID_ANY);
        editSizer->Add(nameLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
        editSizer->Add(nameCtrl, 1, wxRIGHT, 10);
        editSizer->Add(timeLabel, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 5);
        editSizer->Add(timeCtrl, 1);
        mainSizer->Add(editSizer, 0, wxEXPAND | wxALL, 10);
        
        // 添加按钮
        wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
        wxButton* okBtn = new wxButton(dialog, wxID_OK, "OK");
        wxButton* cancelBtn = new wxButton(dialog, wxID_CANCEL, "Cancel");
        buttonSizer->Add(okBtn, 0, wxRIGHT, 10);
        buttonSizer->Add(cancelBtn);
        mainSizer->Add(buttonSizer, 0, wxALIGN_CENTER | wxALL, 10);
        
        dialog->SetSizer(mainSizer);
        
        // 当选择列表项时，更新编辑控件
        markerList->Bind(wxEVT_LISTBOX, [=](wxCommandEvent& event) {
            int selected = event.GetSelection();
            if (selected >= 0 && selected < (int)m_wavePanel->m_markers.size()) {
                auto& mk = m_wavePanel->m_markers[selected];
                nameCtrl->SetValue(mk.label);
                timeCtrl->SetValue(wxString::Format("%d", mk.timestamp));
            }
        });
        
        // 显示对话框
        if (dialog->ShowModal() == wxID_OK) {
            int selected = markerList->GetSelection();
            if (selected >= 0 && selected < (int)m_wavePanel->m_markers.size()) {
                // 更新选中的标记
                m_wavePanel->m_markers[selected].label = nameCtrl->GetValue();
                long timestamp;
                if (timeCtrl->GetValue().ToLong(&timestamp)) {
                    m_wavePanel->m_markers[selected].timestamp = timestamp;
                }
                m_wavePanel->Refresh();
            }
        }
        
        dialog->Destroy();
    }
    
    void OnDropNamedMarker(wxCommandEvent&)
    {
        // 放置命名标记
        wxString markerName = wxGetTextFromUser("Enter marker name:", "Drop Named Marker", wxString::Format("M%d", (int)m_wavePanel->m_markers.size() + 1), this);
        if (!markerName.IsEmpty()) {
            int currentTime = m_wavePanel->GetCurrentTime();
            m_wavePanel->AddMarker(currentTime, markerName);
            // 显示成功消息
            wxMessageBox(wxString::Format("Marker '%s' added at time %d.", markerName, currentTime));
        }
    }
    
    void OnCollectNamedMarker(wxCommandEvent&)
    {
        // 收集命名标记
        if (m_wavePanel->m_markers.empty()) {
            wxMessageBox("No markers to collect.");
            return;
        }
        
        // 创建一个对话框来选择要收集的标记
        wxDialog* dialog = new wxDialog(this, wxID_ANY, "Collect Named Marker", wxDefaultPosition, wxSize(400, 300));
        wxBoxSizer* mainSizer = new wxBoxSizer(wxVERTICAL);
        
        // 创建一个列表框来显示标记
        wxListBox* markerList = new wxListBox(dialog, wxID_ANY, wxDefaultPosition, wxSize(-1, 150));
        for (size_t i = 0; i < m_wavePanel->m_markers.size(); i++) {
            auto& mk = m_wavePanel->m_markers[i];
            markerList->Append(mk.label);
        }
        mainSizer->Add(markerList, 1, wxEXPAND | wxALL, 10);
        
        // 添加按钮
        wxBoxSizer* buttonSizer = new wxBoxSizer(wxHORIZONTAL);
        wxButton* okBtn = new wxButton(dialog, wxID_OK, "Collect");
        wxButton* cancelBtn = new wxButton(dialog, wxID_CANCEL, "Cancel");
        buttonSizer->Add(okBtn, 0, wxRIGHT, 10);
        buttonSizer->Add(cancelBtn);
        mainSizer->Add(buttonSizer, 0, wxALIGN_CENTER | wxALL, 10);
        
        dialog->SetSizer(mainSizer);
        
        // 显示对话框
        if (dialog->ShowModal() == wxID_OK) {
            int selected = markerList->GetSelection();
            if (selected >= 0 && selected < (int)m_wavePanel->m_markers.size()) {
                auto& mk = m_wavePanel->m_markers[selected];
                // 这里可以实现收集标记的逻辑，例如将标记添加到收藏列表中
                wxMessageBox(wxString::Format("Marker '%s' collected at time %d.", mk.label, mk.timestamp));
            }
        }
        
        dialog->Destroy();
    }
    
    void OnCollectAllNamedMarkers(wxCommandEvent&)
    {
        // 收集所有命名标记
        if (m_wavePanel->m_markers.empty()) {
            wxMessageBox("No markers to collect.");
            return;
        }
        
        // 收集所有标记
        wxString collectedMarkers;
        for (size_t i = 0; i < m_wavePanel->m_markers.size(); i++) {
            auto& mk = m_wavePanel->m_markers[i];
            collectedMarkers += wxString::Format("%s: %d\n", mk.label, mk.timestamp);
        }
        
        // 显示收集的标记
        wxMessageBox(collectedMarkers, "Collected Markers");
        
        // 显示成功消息
        wxMessageBox(wxString::Format("Collected %d markers.", (int)m_wavePanel->m_markers.size()));
    }
    
    void OnCopyPrimaryToBMarker(wxCommandEvent&)
    {
        // 复制主标记到 B 标记
        if (m_wavePanel->m_hasMarkerA) {
            m_wavePanel->m_markerB = m_wavePanel->m_markerA;
            m_wavePanel->Refresh();
            // 显示成功消息
            wxMessageBox(wxString::Format("Primary marker copied to B marker at time %d.", m_wavePanel->m_markerB));
        } else {
            // 显示错误消息
            wxMessageBox("No primary marker to copy.");
        }
    }
    
    void OnDeletePrimaryMarker(wxCommandEvent&)
    {
        // 删除主标记
        if (m_wavePanel->m_hasMarkerA) {
            m_wavePanel->m_hasMarkerA = false;
            m_wavePanel->m_markerA = -1;
            m_wavePanel->m_markerB = -1;
            m_wavePanel->Refresh();
            // 显示成功消息
            wxMessageBox("Primary marker deleted.");
        } else {
            // 显示错误消息
            wxMessageBox("No primary marker to delete.");
        }
    }
    
    void OnFindPreviousEdge(wxCommandEvent&)
    {
        // 查找上一个边沿
        m_wavePanel->FindPreviousEdge();
    }
    
    void OnFindNextEdge(wxCommandEvent&)
    {
        // 查找下一个边沿
        m_wavePanel->FindNextEdge();
    }
    
    void OnAlternateWheelMode(wxCommandEvent& event)
    {
        // 切换滚轮模式
        m_wavePanel->m_alternateWheelMode = !m_wavePanel->m_alternateWheelMode;
        
        // 显示当前模式
        if (m_wavePanel->m_alternateWheelMode) {
            wxMessageBox("Alternate Wheel Mode enabled.\nWheel now controls time instead of zoom.");
        } else {
            wxMessageBox("Alternate Wheel Mode disabled.\nWheel now controls zoom instead of time.");
        }
    }
    
    void OnWaveScrolling(wxCommandEvent& event)
    {
        // 波形滚动
        m_wavePanel->m_waveScrollingEnabled = !m_wavePanel->m_waveScrollingEnabled;
        
        // 显示当前状态
        if (m_wavePanel->m_waveScrollingEnabled) {
            wxMessageBox("Wave Scrolling enabled.\nUse arrow keys to scroll through signals.");
        } else {
            wxMessageBox("Wave Scrolling disabled.");
        }
    }
    
    void OnLocking(wxCommandEvent&)
    {
        // 锁定
        m_wavePanel->m_markersLocked = !m_wavePanel->m_markersLocked;
        
        if (m_wavePanel->m_markersLocked) {
            wxMessageBox("Markers locked. They cannot be moved or modified.");
        } else {
            wxMessageBox("Markers unlocked. They can be moved and modified.");
        }
    }

    // 点击模块 → 显示直属信号（不含子模块）
    void OnTreeSelect(wxTreeListEvent& event)
    {
        wxTreeListItem item = event.GetItem();
        if (!item.IsOk()) return;

        std::string modulePath = GetFullPath(m_signalTree, item);
        m_signalList->DeleteAllItems();

        auto it = m_moduleToSignals.find(modulePath);
        if (it != m_moduleToSignals.end())
        {
            for (signal_t* sig : it->second)
            {
                long idx = m_signalList->InsertItem(m_signalList->GetItemCount(), "wire");
                m_signalList->SetItem(idx, 1, sig->name);
                m_signalList->SetItemPtrData(idx, (wxUIntPtr)sig);
            }
        }
    }

    std::string GetFullPath(wxTreeListCtrl* tree, wxTreeListItem item)
    {
        wxString path;
        while (item.IsOk())
        {
            wxString name = tree->GetItemText(item, 0);
            if (name == "VCD Modules") break;
            path = path.IsEmpty() ? name : name + "." + path;
            item = tree->GetItemParent(item);
        }
        return path.ToStdString();
    }

    // 重构：模块树只显示模块，不显示信号
    void BuildSignalTreeView(wxTreeListCtrl* tree, vcd_t* vcd)
    {
        if (!tree) return;
        tree->DeleteAllItems();
        wxTreeListItem rootId = tree->AppendItem(tree->GetRootItem(), "VCD Modules");
        std::unordered_map<std::string, wxTreeListItem> pathMap;
        pathMap[""] = rootId;

        if (vcd)
        {
            // 处理VCD文件
            m_moduleToSignals.clear();
            signal_node_t* node = vcd->signals_head;

            while (node)
            {
                signal_t* sig = &node->signal;
                std::string modulePath = sig->module_path;
                m_moduleToSignals[modulePath].push_back(sig);

                wxArrayString parts = wxSplit(sig->module_path, '.');
                std::string fullPath;
                wxTreeListItem parent = rootId;

                for (size_t i = 0; i < parts.size(); i++)
                {
                    wxString part = parts[i];
                    if (part.IsEmpty()) continue;
                    if (!fullPath.empty()) fullPath += ".";
                    fullPath += part.ToStdString();

                    if (pathMap.count(fullPath))
                    {
                        parent = pathMap[fullPath];
                        continue;
                    }
                    wxTreeListItem item = tree->AppendItem(parent, part);
                    pathMap[fullPath] = item;
                    parent = item;
                }
                node = node->next;
            }
        }
        else
        {
            // 处理CSV文件
            for (const auto& pair : m_moduleToSignals)
            {
                const std::string& modulePath = pair.first;
                if (modulePath.empty()) continue;

                wxArrayString parts = wxSplit(modulePath, '.');
                std::string fullPath;
                wxTreeListItem parent = rootId;

                for (size_t i = 0; i < parts.size(); i++)
                {
                    wxString part = parts[i];
                    if (part.IsEmpty()) continue;
                    if (!fullPath.empty()) fullPath += ".";
                    fullPath += part.ToStdString();

                    if (pathMap.count(fullPath))
                    {
                        parent = pathMap[fullPath];
                        continue;
                    }
                    wxTreeListItem item = tree->AppendItem(parent, part);
                    pathMap[fullPath] = item;
                    parent = item;
                }
            }
        }
        tree->Expand(rootId);
    }



    void OnPlay(wxCommandEvent&)
    {
        if (m_timer->IsRunning()) { m_timer->Stop(); m_playBtn->SetLabel("Play"); }
        else { m_timer->Start(40); m_playBtn->SetLabel("Pause"); }
    }

    void OnTimer(wxTimerEvent&)
    {
        int v = m_slider->GetValue();
        if (v < m_slider->GetMax())
        {
            v += std::max(1, m_wavePanel->m_displayTimeRange / 100);
            m_slider->SetValue(v);
            m_wavePanel->SetCurrentTimestamp(v);
        }
        else { m_timer->Stop(); m_playBtn->SetLabel("Play"); }
        SyncTimeRangeUI();
    }

    void OnSlide(wxCommandEvent&)
    {
        m_wavePanel->SetCurrentTimestamp(m_slider->GetValue());
        SyncTimeRangeUI();
    }

    void OnFilter(wxCommandEvent&)
    {
        wxTextEntryDialog dlg(this, "Alias (comma split)", "Filter");
        if (dlg.ShowModal() != wxID_OK) return;
    }
    
    // 切换AI面板显示
    void OnToggleAIPanel(wxCommandEvent& event)
    {
        if (m_aiPanel->IsShown())
            m_aiPanel->Hide();
        else
            m_aiPanel->Show();
        
        m_mainSplitter->Layout();
    }
    
    // 设置API Key
    void OnSetAPIKey(wxCommandEvent& event)
    {
        wxString currentKey = m_aiPanel->GetApiKey();
        wxString defaultValue = (currentKey == wxString("your-deepseek-api-key-here")) ? (wxString)wxEmptyString : currentKey;
        wxString newKey = wxGetTextFromUser(
            "Enter your DeepSeek API Key:",
            "Set API Key",
            defaultValue,
            this
        );
        
        if (!newKey.IsEmpty())
        {
            m_aiPanel->SetApiKey(newKey);
            wxMessageBox("API Key has been set successfully!");
        }
    }
    void SetProjectDir(const wxString& filePath)
    {
        if (filePath.IsEmpty())
        {
            return; // 如果路径为空，直接返回
        }

        wxFileName fileName(filePath);
        wxString extension = fileName.GetExt().Lower();

        // 检查m_wavePanel是否为nullptr
        if (!m_wavePanel)
        {
            // 使用日志代替消息框，避免wxMessageBox的问题
            wxLogError("Waveform panel not initialized!");
            return;
        }

        // 清空之前的信号
        m_displayedSignals.clear();
        m_wavePanel->ClearDisplaySignals();
        m_moduleToSignals.clear();

        if (extension == "vcd")
        {
            // 加载VCD文件
            m_wavePanel->OpenVCDFile(filePath);
            m_slider->SetRange(0, m_wavePanel->m_maxTimestamp);
            m_slider->SetValue(0);
            BuildSignalTreeView(m_signalTree, m_wavePanel->m_vcdData);
            m_signalList->DeleteAllItems();

            // 将所有信号传递给AI分析面板
            if (m_aiPanel && !m_wavePanel->m_allSignals.empty())
            {
                m_aiPanel->SetSignalInfo(m_wavePanel->m_allSignals);
            }

            // 使用状态栏或日志代替消息框，避免wxMessageBox的问题
            SetStatusText("Successfully loaded VCD file: " + filePath);
        }
        else if (extension == "csv")
        {
            // 加载CSV文件
            CSVParser parser;
            if (parser.LoadFromFile(filePath.ToStdString()))
            {
                // 处理CSV数据
                ProcessCSVData(parser);

                // 更新信号树
                BuildSignalTreeView(m_signalTree, m_wavePanel->m_vcdData);
                m_signalList->DeleteAllItems();

                // 重置滑块范围
                m_slider->SetRange(0, 1000);
                m_slider->SetValue(0);

                // 将所有信号传递给AI分析面板
                if (m_aiPanel && !m_displayedSignals.empty())
                {
                    m_aiPanel->SetSignalInfo(m_displayedSignals);
                }

                // 使用状态栏或日志代替消息框，避免wxMessageBox的问题
                SetStatusText("Successfully loaded CSV file: " + filePath);
            }
            else
            {
                // 使用日志代替消息框，避免wxMessageBox的问题
                wxLogError("Failed to load CSV file: " + filePath);
            }
        }
        else if (extension == "json")
        {
            // 处理JSON文件
            SetStatusText("JSON file loaded: " + filePath);
            // 这里可以添加JSON文件的处理逻辑
        }
        else
        {
            // 处理其他文件类型
            SetStatusText("Unsupported file type: " + extension);
        }
    }
};

class MyApp : public wxApp
{
public:
    bool OnInit() override
    {
        wxString projectDir;
        int ret = wxID_CANCEL;
        
        {
            ProjectStartWindow startWindow;
            ret = startWindow.ShowModal();
            
            if (ret == wxID_OK)
            {
                projectDir = startWindow.GetProjectDir();
            }
        }

        if (ret == wxID_CANCEL)
        {
            return false;
        }

        MyFrame* frame = new MyFrame();
        frame->Centre(wxBOTH);
        frame->Show(true);

        if (!projectDir.IsEmpty())
        {
            frame->SetProjectDir(projectDir);
        }
        else
        {
            
        }

        SetTopWindow(frame);

        return true;
    }
};

wxIMPLEMENT_APP(MyApp);