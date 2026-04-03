#pragma once
#ifndef MAIN_H
#define MAIN_H

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

// 宏定义
#define SIGNAL_ROW_HEIGHT 60
#define LEFT_MARGIN 180
#define WAVE_PADDING 40

// 前向声明
class WaveformPanel;

// 函数前向声明
void split_module_path(const char* full_path, std::vector<std::string>& out_parts);
void FreeSignalTree(SignalGroup* root);

// AI分析面板类声明
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

// 波形面板类声明
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

    struct Marker
    {
        int timestamp;
        wxString label;
    };

    struct Comment {
        int position; // 在信号列表中的位置
        wxString text; // 注释内容
    };

    enum DataFormat {
        FORMAT_BINARY,
        FORMAT_OCTAL,
        FORMAT_DECIMAL,
        FORMAT_HEXADECIMAL,
        FORMAT_ASCII,
        FORMAT_SIGNED_DECIMAL,
        FORMAT_REAL
    };

    enum ColorFormat {
        COLOR_DEFAULT,
        COLOR_SIGNAL_NAME,
        COLOR_VALUE,
        COLOR_MODULE
    };

    struct WaveformSignalGroup {
        std::vector<signal_t*> signals;
        wxString name;
        bool isExpanded;
    };

    // 成员变量
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
    std::unordered_set<std::string> m_searchMatchedSignals;
    std::string m_searchKeyword;
    wxString m_editingMarkerText;
    std::unordered_set<std::string> m_visibleSignals;
    std::mutex m_cacheMutex;
    std::vector<std::vector<DrawSegment>> m_cachedSegments;
    std::vector<Marker> m_markers;
    std::vector<signal_t*> m_displayedSignals2;
    std::vector<signal_t*> m_clipboard;
    int m_traceMaxHier = 10;
    bool m_showTraceHier = true;
    std::vector<Comment> m_comments;
    int m_selectedSignalIndex = -1;
    std::map<signal_t*, wxString> m_signalAliases;
    std::map<signal_t*, DataFormat> m_signalDataFormats;
    ColorFormat m_globalColorFormat = COLOR_DEFAULT;
    std::vector<WaveformSignalGroup> m_signalGroups;
    std::unordered_set<signal_t*> m_highlightedSignals;
    std::unordered_set<signal_t*> m_excludedSignals;
    bool m_timeWarpEnabled = false;
    double m_timeWarpFactor = 1.0;
    bool m_alternateWheelMode = false;
    bool m_waveScrollingEnabled = false;
    int m_analogHeightExtension = 0;
    bool m_markersLocked = false;

    // 未在原代码中显式声明但使用的成员变量（补充）
    vcd_t* m_vcdData = nullptr;
    int m_currentTimestamp = 0;
    int m_displayTimeRange = 1000;
    int m_maxTimestamp = 1000;
    std::vector<signal_t*> m_allSignals;
    std::map<std::string, wxColour> m_signalColors;
    std::mt19937 m_rng;
    SignalGroup* m_signalTreeRoot = nullptr;

    // 成员函数声明
    std::string GetValueAt(signal_t* sig, int ts) const;
    void AddDisplaySignal(signal_t* sig);
    void ClearDisplaySignals();
    void AssignSignalColors();
    void ClampViewToLimit();
    const char* GetSignalValueAt(signal_t* sig, int timestamp);
    int FindNextEdge(int t);
    int FindPrevEdge(int t);
    void SearchSignals(const std::string& keyword);
    double ComputeFrequency(signal_t* sig);
    double ComputeDuty(signal_t* sig);
    int FindNearestEdge(int targetTime);
    void StartEditMarker(int index);
    void DeleteMarker(int index);
    void DrawMarkerMeasurementBar(wxAutoBufferedPaintDC& dc, wxSize& size, double scale);
    int TimeToX(int t, double scale) const;
    void LoadVcdSignals(vcd_t* vcd);
    void ToggleCursorValueDisplay();
    char ParseVcdValue(const char* v) const;
    std::string ParseBusValue(const char* v) const;
    void SetVisibleSignals(const std::unordered_set<std::string>& visible);

    // 构造/析构函数
    WaveformPanel(wxWindow* parent);
    ~WaveformPanel();

    // 事件处理函数声明
    void OnPaint(wxPaintEvent& event);
    void OnResize(wxSizeEvent& event);
    void OnMouseMove(wxMouseEvent& event);
    void OnMouseWheel(wxMouseEvent& event);
    void OnMouseDown(wxMouseEvent& event);
    void OnMouseUp(wxMouseEvent& event);
    void OnRightDown(wxMouseEvent& event);
    void OnRightUp(wxMouseEvent& event);

    // 未实现的函数声明（原代码中引用但未给出实现）
    void BuildDrawCacheAsync();
    void InitSignalTree();
};

#endif // MAIN_H