#pragma once

#if defined(_WIN32) && !defined(NOMINMAX)
#define NOMINMAX
#endif

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
#include <wx/strconv.h>
#include <wx/file.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/checkbox.h>
#include <wx/log.h>
#include <wx/generic/logg.h>
#include <wx/slider.h>
#include <wx/combobox.h>

#include <windows.h>
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif
#ifdef SetCurrentTime
#undef SetCurrentTime
#endif
#include <wininet.h>
#include <algorithm>
#include <cstdio>
#include <cstring>
#include <cmath>
#include <ctime>
#include <fstream>
#include <functional>
#include <map>
#include <sstream>
#include <string>
#include <vector>
#include <climits>
#include <cstdint>

#include "waveform_constants.h"
#include "csv.h"
#include "vcd.h"
#include "fst_loader.h"
#include "ProjectStartWindow.h"
#include "AIAnalysisPanel.h"
#include "panels/WaveformPanel.hpp"
#include "core/WaveformCommandHandlers.h"
#include "script/TclScriptEngine.h"
#include "ui/WaveformCompareHub.h"
#include "core/WaveformSession.h"

inline void FstLoaderLineToWxLog(const char* line, void*)
{
    if (!line || !line[0])
        return;
    wxString s = wxString::FromUTF8(line);
    if (wxTheApp)
        wxTheApp->CallAfter([s]() { wxLogMessage(wxString::FromUTF8("[FST] ") + s); });
}
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
    wxSplitterWindow* m_leftSplitter = nullptr;
    wxLogWindow* m_debugLogWindow = nullptr;

    // 新增：满足你需求的核心变量
    std::vector<signal_t*> m_displayedSignals;
    std::map<std::string, std::vector<signal_t*>> m_moduleToSignals;
    wxMenu* m_moduleMenu;
    wxTreeListItem m_rightClickModuleItem;

    /** CSV-loaded signals are heap-allocated; tracked here for correct delete vs VCD nodes. */
    std::vector<signal_t*> m_csvHeapSignals;
    bool m_suppressSliderPlayheadSync = false;
    /** 避免联动广播时递归同步其它窗口。 */
    bool m_compareEcho = false;
    wxString m_tracePathLabel;
    wxString m_sessionFilePath;

    void SyncAiPanelFromWavePanel()
    {
        if (!m_aiPanel || !m_wavePanel)
            return;
        m_aiPanel->SetDisplayedSignals(m_wavePanel->m_displayedSignals2, true);
    }

    void ReleaseCsvHeapSignals()
    {
        for (signal_t* s : m_csvHeapSignals) {
            if (!s) continue;
            signal_free_value_changes(s);
            delete s;
        }
        m_csvHeapSignals.clear();
    }

    signal_t* FindSignalForScript(const std::string& name) const
    {
        if (!m_wavePanel || !m_wavePanel->m_vcdData || name.empty())
            return nullptr;
        if (signal_t* exact = vcd_get_signal_by_name(m_wavePanel->m_vcdData, name.c_str()))
            return exact;
        for (signal_t* sig : m_wavePanel->m_allSignals) {
            if (!sig) continue;
            if (name == sig->full_name || name == sig->name)
                return sig;
            const std::string fn = sig->full_name;
            if (fn.size() > name.size() && fn.compare(fn.size() - name.size(), name.size(), name) == 0
                && fn[fn.size() - name.size() - 1] == '.')
                return sig;
        }
        return nullptr;
    }

    WaveformCommandHandlers BuildScriptHandlers()
    {
        WaveformCommandHandlers h;
        MyFrame* self = this;

        h.loadTrace = [self](const std::string& path, std::string& err) -> bool {
            const wxString wxPath = wxString::FromUTF8(path.c_str());
            if (!wxFileName::FileExists(wxPath)) {
                err = "file not found: " + path;
                return false;
            }
            self->SetProjectDir(wxPath);
            if (!self->m_wavePanel || !self->m_wavePanel->m_vcdData) {
                err = "failed to load trace (unsupported format or parse error)";
                return false;
            }
            return true;
        };

        h.addSignals = [self](const std::vector<std::string>& names, std::string& err) -> bool {
            if (!self->m_wavePanel || !self->m_wavePanel->m_vcdData) {
                err = "no trace loaded";
                return false;
            }
            int added = 0;
            std::string missing;
            for (const std::string& n : names) {
                signal_t* sig = self->FindSignalForScript(n);
                if (sig) {
                    self->m_wavePanel->AddDisplaySignal(sig);
                    ++added;
                } else {
                    if (!missing.empty()) missing += ", ";
                    missing += n;
                }
            }
            if (added == 0) {
                err = missing.empty() ? "no signal names given" : ("unknown signals: " + missing);
                return false;
            }
            if (!missing.empty())
                self->SetStatusText(wxString::Format("Script: added %d signal(s); skipped: %s", added, missing));
            else
                self->SetStatusText(wxString::Format("Script: added %d signal(s)", added));
            return true;
        };

        h.zoomFull = [self](std::string& err) -> bool {
            if (!self->m_wavePanel) { err = "no waveform panel"; return false; }
            self->SaveCurrentView();
            self->m_wavePanel->ZoomReset();
            self->SyncTimeRangeUI();
            return true;
        };
        h.zoomIn = [self](std::string& err) -> bool {
            if (!self->m_wavePanel) { err = "no waveform panel"; return false; }
            self->SaveCurrentView();
            self->m_wavePanel->ZoomIn();
            self->SyncTimeRangeUI();
            return true;
        };
        h.zoomOut = [self](std::string& err) -> bool {
            if (!self->m_wavePanel) { err = "no waveform panel"; return false; }
            self->SaveCurrentView();
            self->m_wavePanel->ZoomOut();
            self->SyncTimeRangeUI();
            return true;
        };
        h.pageLeft = [self](std::string& err) -> bool {
            if (!self->m_wavePanel) { err = "no waveform panel"; return false; }
            self->SaveCurrentView();
            self->m_wavePanel->PageLeft();
            self->SyncTimeRangeUI();
            return true;
        };
        h.pageRight = [self](std::string& err) -> bool {
            if (!self->m_wavePanel) { err = "no waveform panel"; return false; }
            self->SaveCurrentView();
            self->m_wavePanel->PageRight();
            self->SyncTimeRangeUI();
            return true;
        };
        h.setPlayhead = [self](long long t, std::string& err) -> bool {
            if (!self->m_wavePanel) { err = "no waveform panel"; return false; }
            self->m_wavePanel->SetCurrentTimestamp(t, true);
            self->SyncTimeRangeUI();
            return true;
        };
        h.setTimeRange = [self](long long from, long long to, std::string& err) -> bool {
            if (!self->m_wavePanel) { err = "no waveform panel"; return false; }
            self->SaveCurrentView();
            self->m_wavePanel->ApplyVisibleTimeRange(from, to);
            self->SyncTimeRangeUI();
            return true;
        };
        h.logMessage = [self](const std::string& msg) {
            self->SetStatusText(wxString::FromUTF8(msg.c_str()));
            wxLogMessage(wxString::FromUTF8("[Script] ") + wxString::FromUTF8(msg.c_str()));
        };
        return h;
    }

    void SyncSliderToWavePlayhead(long long t)
    {
        if (!m_slider || !m_wavePanel) return;
        static long long s_dbgLastSyncT = LLONG_MIN;
        if (t != s_dbgLastSyncT) {
            s_dbgLastSyncT = t;
            const int sv = TraceTimeToSliderValue(t);
            wxLogDebug(
                "[Bear2Wave][Frame] SyncSliderToWavePlayhead: t=%lld -> slider=%d/%d maxTs=%lld",
                t,
                sv,
                m_slider->GetMax(),
                m_wavePanel->m_maxTimestamp);
        }
        m_suppressSliderPlayheadSync = true;
        m_slider->SetValue(TraceTimeToSliderValue(t));
        m_suppressSliderPlayheadSync = false;
        SyncTimeRangeUI();
        if (!m_compareEcho)
            WaveformCompare::BroadcastPlayhead(this, t);
    }

    void PublishCompareTimeView()
    {
        if (m_compareEcho || !m_wavePanel)
            return;
        WaveformCompare::BroadcastTimeView(
            this, m_wavePanel->m_timeOffset, m_wavePanel->m_displayTimeRange);
    }

    void ApplyLinkedPlayhead(long long t)
    {
        if (!m_wavePanel)
            return;
        m_compareEcho = true;
        m_wavePanel->SetCurrentTimestamp(t, true);
        m_compareEcho = false;
    }

    void ApplyLinkedTimeView(long long offset, long long range)
    {
        if (!m_wavePanel || range < 1)
            return;
        m_compareEcho = true;
        SaveCurrentView();
        m_wavePanel->ApplyVisibleTimeRange(offset, offset + range);
        m_wavePanel->ClampTimeView();
        SyncTimeRangeUI();
        m_compareEcho = false;
    }

    void CopyDisplayedSignalsFrom(const MyFrame* source)
    {
        if (!source || !source->m_wavePanel || !m_wavePanel)
            return;
        m_wavePanel->ClearDisplaySignals();
        for (signal_t* srcSig : source->m_wavePanel->m_displayedSignals2) {
            if (!srcSig)
                continue;
            signal_t* found = nullptr;
            if (srcSig->full_name[0])
                found = vcd_get_signal_by_name(m_wavePanel->m_vcdData, srcSig->full_name);
            if (!found && srcSig->name[0])
                found = vcd_get_signal_by_name(m_wavePanel->m_vcdData, srcSig->name);
            if (found)
                m_wavePanel->AddDisplaySignal(found);
        }
    }

    void UpdateWindowTitle()
    {
        if (m_tracePathLabel.empty()) {
            SetTitle("VCD Waveform Viewer");
            return;
        }
        wxFileName fn(m_tracePathLabel);
        SetTitle("Compare: " + fn.GetFullName());
    }

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
        
        menu->Append(1007, "Read Session (.gtkw/.b2w)");
        menu->Append(1008, "Write Session\tCtrl+S");
        menu->Append(1009, "Write Session As\tShift+Ctrl+S");
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
        pageSubMenu->Append(7501, "Page Left\tPgUp");
        pageSubMenu->Append(7502, "Page Right\tPgDn");
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
        dataFormatSubMenu->AppendSeparator();
        dataFormatSubMenu->Append(8108, "Apply to All Displayed Traces");
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

        auto viewMenu = new wxMenu;
        // ASCII labels: wxString::FromUTF8("中文") is empty if the .cpp is not UTF-8,
        // which triggers wxMenuItemBase::SetItemLabel assert (non-stock item, empty label).
        viewMenu->AppendCheckItem(8601, "Debug log window\tCtrl+Shift+L");
        viewMenu->AppendCheckItem(8602, "Verbose FST load to log window");
        viewMenu->AppendSeparator();
        viewMenu->Append(8603, "Dump waveform / trace summary");

        auto compareMenu = new wxMenu;
        compareMenu->Append(9501, "Open Second Trace for Compare...\tCtrl+Shift+O");
        compareMenu->AppendSeparator();
        compareMenu->AppendCheckItem(9502, "Link Playheads Across Windows");
        compareMenu->AppendCheckItem(9503, "Link Time View Across Windows");
        compareMenu->AppendSeparator();
        compareMenu->Append(9504, "Tile Windows Horizontally");
        
        auto mb = new wxMenuBar;
        mb->Append(menu, "File");
        mb->Append(editMenu, "Edit");
        mb->Append(timeMenu, "Time");
        mb->Append(markersMenu, "Markers");
        mb->Append(measureMenu, "Measure");
        mb->Append(compareMenu, "Compare");
        mb->Append(viewMenu, "View");
        mb->Append(aiMenu, "AI");
        SetMenuBar(mb);

        fst_loader_set_line_logger(FstLoaderLineToWxLog, nullptr);
        fst_loader_set_line_logger_enabled(0);

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

        m_leftSplitter = new wxSplitterWindow(m_treePanel);
        wxPanel* topPanel = new wxPanel(m_leftSplitter);
        wxPanel* bottomPanel = new wxPanel(m_leftSplitter);
        m_leftSplitter->SplitHorizontally(topPanel, bottomPanel, 300);
        m_splitter->SplitVertically(m_treePanel, m_wavePanel, 280);
        m_mainSplitter->SplitVertically(m_splitter, m_aiPanel, 1000);

        m_wavePanel->SetOpenScopeCallback([this](signal_t* sig) { OpenScopeForSignal(sig); });

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
        leftSizer->Add(m_leftSplitter, 1, wxEXPAND);
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
        m_wavePanel->SetPlayheadChangedCallback([this](long long t) { SyncSliderToWavePlayhead(t); });
        m_wavePanel->SetTimeViewChangedCallback([this](long long, long long) { PublishCompareTimeView(); });
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
        Bind(wxEVT_MENU, &MyFrame::OnDataFormatApplyAll, this, 8108);
        
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

        Bind(wxEVT_MENU, &MyFrame::OnViewDebugLog, this, 8601);
        Bind(wxEVT_MENU, &MyFrame::OnViewFstVerbose, this, 8602);
        Bind(wxEVT_MENU, &MyFrame::OnViewWaveformSummary, this, 8603);

        Bind(wxEVT_MENU, &MyFrame::OnCompareOpenSecond, this, 9501);
        Bind(wxEVT_MENU, &MyFrame::OnCompareLinkPlayheads, this, 9502);
        Bind(wxEVT_MENU, &MyFrame::OnCompareLinkTimeView, this, 9503);
        Bind(wxEVT_MENU, &MyFrame::OnCompareTileWindows, this, 9504);

        WaveformCompare::RegisterFrame(this);
    }

    ~MyFrame() override
    {
        WaveformCompare::UnregisterFrame(this);
        if (m_wavePanel)
            m_wavePanel->ClearWavePanel();
        ReleaseCsvHeapSignals();
        delete m_debugLogWindow;
        m_debugLogWindow = nullptr;
    }

    void OnViewDebugLog(wxCommandEvent& ev)
    {
        if (ev.IsChecked()) {
            if (!m_debugLogWindow)
                m_debugLogWindow = new wxLogWindow(this, "Debug log", true, true);
            m_debugLogWindow->Show(true);
            wxLogMessage(
                "Debug log is open. Enable \"Verbose FST load to log window\" under View, then reload the FST file to see parse details.");
        } else if (m_debugLogWindow) {
            m_debugLogWindow->Show(false);
        }
    }

    void OnViewFstVerbose(wxCommandEvent& ev)
    {
        fst_loader_set_line_logger_enabled(ev.IsChecked() ? 1 : 0);
    }

    void OnViewWaveformSummary(wxCommandEvent&)
    {
        if (!m_wavePanel) {
            wxLogMessage("m_wavePanel is null");
            return;
        }
        vcd_t* vcd = m_wavePanel->m_vcdData;
        const size_t nAll = m_wavePanel->m_allSignals.size();
        const size_t nDisp = m_wavePanel->m_displayedSignals2.size();
        wxLogMessage(wxString::Format(
            "vcd=%p  allSignals=%llu  displayed=%llu  maxTs=%lld  timeOffset=%lld  displayRange=%lld  hasLimit=%d",
            vcd, (unsigned long long)nAll, (unsigned long long)nDisp,
            m_wavePanel->m_maxTimestamp,
            m_wavePanel->m_timeOffset,
            m_wavePanel->m_displayTimeRange,
            m_wavePanel->m_hasLimit ? 1 : 0));
        if (vcd && vcd->signals_head) {
            int count = 0;
            for (signal_node_t* p = vcd->signals_head; p; p = p->next)
                ++count;
            wxLogMessage(wxString::Format("vcd_t::signals_head chain length: %d", count));
        } else {
            wxLogMessage("vcd or signals_head is null");
        }
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
        SyncAiPanelFromWavePanel();
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
        m_wavePanel->RequestDrawCacheRebuild();
        m_wavePanel->Refresh();
        SyncAiPanelFromWavePanel();
    }

    void OnTimeRangeChanged(wxCommandEvent&)
    {
        long long from = 0, to = 0;
        if (m_fromText->GetValue().ToLongLong(&from) && m_toText->GetValue().ToLongLong(&to) && to > from)
        {
            m_wavePanel->m_limitStart = from;
            m_wavePanel->m_limitEnd = to;
            m_wavePanel->m_hasLimit = true;

            m_wavePanel->m_timeOffset = from;
            m_wavePanel->m_displayTimeRange = to - from;

            m_wavePanel->RequestDrawCacheRebuild();
            m_wavePanel->Refresh();
            SyncTimeRangeUI();
            PublishCompareTimeView();
        }
    }

    void SyncTimeRangeUI()
    {
        m_fromText->SetValue(wxString::Format("%lld", m_wavePanel->m_timeOffset));
        m_toText->SetValue(wxString::Format("%lld", m_wavePanel->m_timeOffset + m_wavePanel->m_displayTimeRange));
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
        case 4002:
            SaveCurrentView();
            m_wavePanel->PageLeft();
            SyncTimeRangeUI();
            return;
        case 4003:
            SaveCurrentView();
            m_wavePanel->PageRight();
            SyncTimeRangeUI();
            return;
        case 4004: m_wavePanel->m_timeOffset = m_wavePanel->m_maxTimestamp - m_wavePanel->m_displayTimeRange; if (m_wavePanel->m_timeOffset < 0) m_wavePanel->m_timeOffset = 0; break;
        case 4101: m_wavePanel->SetCurrentTimestamp(m_wavePanel->FindPrevEdge(m_wavePanel->m_currentTimestamp), true); break;
        case 4102: m_wavePanel->SetCurrentTimestamp(m_wavePanel->FindNextEdge(m_wavePanel->m_currentTimestamp), true); break;
        }

        if (m_wavePanel->m_timeOffset < 0) m_wavePanel->m_timeOffset = 0;
        if (m_wavePanel->m_timeOffset + m_wavePanel->m_displayTimeRange > m_wavePanel->m_maxTimestamp)
            m_wavePanel->m_timeOffset = m_wavePanel->m_maxTimestamp - m_wavePanel->m_displayTimeRange;
        if (m_wavePanel->m_timeOffset < 0) m_wavePanel->m_timeOffset = 0;

        m_wavePanel->RequestDrawCacheRebuild();
        m_wavePanel->Refresh();
        SyncTimeRangeUI();
        m_wavePanel->ClampViewToLimit();
        PublishCompareTimeView();
    }

    void OnShowMeasure(wxCommandEvent&)
    {
        m_measureCombo->IsShown() ? m_measureCombo->Hide() : m_measureCombo->Show();
        Layout();
    }
    
    // ===== File 菜单功能实现 =====
    
    void OnOpenNewWindow(wxCommandEvent&)
    {
        MyFrame* peer = new MyFrame();
        peer->Show();
        if (WaveformCompare::LinkPlayheads() || WaveformCompare::LinkTimeView())
            peer->ApplyLinkedPlayhead(m_wavePanel ? m_wavePanel->m_currentTimestamp : 0);
        if (WaveformCompare::LinkTimeView() && m_wavePanel)
            peer->ApplyLinkedTimeView(m_wavePanel->m_timeOffset, m_wavePanel->m_displayTimeRange);
    }

    void OnCompareOpenSecond(wxCommandEvent&)
    {
        wxString defaultDir;
        if (!m_tracePathLabel.empty())
            defaultDir = wxFileName(m_tracePathLabel).GetPath();

        wxFileDialog dlg(
            this,
            "Open second trace to compare",
            defaultDir,
            "",
            "Waveform files (*.vcd;*.fst;*.vzt;*.lxt;*.lxt2;*.ghw)|*.vcd;*.fst;*.vzt;*.lxt;*.lxt2;*.ghw|"
            "VCD (*.vcd)|*.vcd|FST (*.fst)|*.fst|VZT (*.vzt)|*.vzt|LXT (*.lxt;*.lxt2)|*.lxt;*.lxt2|GHW (*.ghw)|*.ghw|All files (*.*)|*.*");

        if (dlg.ShowModal() != wxID_OK)
            return;

        MyFrame* peer = new MyFrame();
        peer->Show();
        peer->SetProjectDir(dlg.GetPath());
        peer->CopyDisplayedSignalsFrom(this);

        WaveformCompare::SetLinkPlayheads(true);
        WaveformCompare::SetLinkTimeView(true);

        if (wxMenuBar* bar = GetMenuBar()) {
            if (wxMenuItem* it = bar->FindItem(9502))
                it->Check(true);
            if (wxMenuItem* it = bar->FindItem(9503))
                it->Check(true);
        }
        if (wxMenuBar* bar = peer->GetMenuBar()) {
            if (wxMenuItem* it = bar->FindItem(9502))
                it->Check(true);
            if (wxMenuItem* it = bar->FindItem(9503))
                it->Check(true);
        }

        if (m_wavePanel) {
            peer->ApplyLinkedPlayhead(m_wavePanel->m_currentTimestamp);
            peer->ApplyLinkedTimeView(m_wavePanel->m_timeOffset, m_wavePanel->m_displayTimeRange);
        }

        WaveformCompare::TileFramesHorizontally();
        SetStatusText("Compare: loaded " + dlg.GetPath());
    }

    void OnCompareLinkPlayheads(wxCommandEvent& e)
    {
        WaveformCompare::SetLinkPlayheads(e.IsChecked());
        if (e.IsChecked() && m_wavePanel)
            WaveformCompare::BroadcastPlayhead(this, m_wavePanel->m_currentTimestamp);
    }

    void OnCompareLinkTimeView(wxCommandEvent& e)
    {
        WaveformCompare::SetLinkTimeView(e.IsChecked());
        if (e.IsChecked() && m_wavePanel)
            WaveformCompare::BroadcastTimeView(
                this, m_wavePanel->m_timeOffset, m_wavePanel->m_displayTimeRange);
    }

    void OnCompareTileWindows(wxCommandEvent&)
    {
        WaveformCompare::TileFramesHorizontally();
    }
    
    void OnOpenNewTab(wxCommandEvent&)
    {
        wxFileDialog dlg(this, "Open waveform", "", "",
            "All traces (*.vcd;*.fst;*.vzt;*.lxt;*.lxt2;*.ghw)|*.vcd;*.fst;*.vzt;*.lxt;*.lxt2;*.ghw|"
            "VCD (*.vcd)|*.vcd|FST (*.fst)|*.fst|VZT (*.vzt)|*.vzt|LXT (*.lxt;*.lxt2)|*.lxt;*.lxt2|GHW (*.ghw)|*.ghw");

        if (dlg.ShowModal() == wxID_OK)
        {
            m_wavePanel->ClearWavePanel();
            ReleaseCsvHeapSignals();
            m_displayedSignals.clear();
            m_moduleToSignals.clear();
            wxFileName fn(dlg.GetPath());
            wxString ext = fn.GetExt().Lower();
            if (ext == "fst")
                m_wavePanel->OpenFSTFile(dlg.GetPath());
            else
                m_wavePanel->OpenVCDFile(dlg.GetPath());
            UpdateTraceSliderForWavePanel();
            m_slider->SetValue(0);
            BuildSignalTreeView(m_signalTree, m_wavePanel->m_vcdData);
            
            // 将所有信号传递给AI分析面板
            SyncAiPanelFromWavePanel();
            SyncTimeRangeUI();
        }
    }
    
    void OnReloadWaveform(wxCommandEvent&)
    {
        // 重新加载波形
        m_wavePanel->RequestDrawCacheRebuild();
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
    
    static const char* SessionFileWildcard()
    {
        return "Session (*.gtkw;*.b2w)|*.gtkw;*.b2w|GTKWave (*.gtkw)|*.gtkw|Bear2Wave (*.b2w)|*.b2w|All (*.*)|*.*";
    }

    WaveformSessionData CollectSessionState() const
    {
        WaveformSessionData d;
        if (!m_tracePathLabel.empty())
            d.tracePath = m_tracePathLabel.ToUTF8().data();
        if (m_wavePanel) {
            d.timeOffset = m_wavePanel->m_timeOffset;
            d.displayRange = m_wavePanel->m_displayTimeRange;
            d.playhead = m_wavePanel->m_currentTimestamp;
            d.showCursorValue = m_wavePanel->m_showCursorValue;
            for (signal_t* sig : m_wavePanel->m_displayedSignals2) {
                if (!sig)
                    continue;
                SessionTrace tr;
                tr.name = sig->full_name[0] ? sig->full_name : sig->name;
                const int code = m_wavePanel->GtkwaveRadixCodeForSignal(sig);
                tr.gtkwaveRadixCode = (code == 0) ? -1 : code;
                d.traces.push_back(std::move(tr));
            }
        }

        const wxSize sz = GetSize();
        const wxPoint pos = GetPosition();
        d.windowW = sz.GetWidth();
        d.windowH = sz.GetHeight();
        d.windowX = pos.x;
        d.windowY = pos.y;
        if (m_splitter && m_splitter->IsSplit())
            d.splitterTreeWave = m_splitter->GetSashPosition();
        if (m_mainSplitter && m_mainSplitter->IsSplit())
            d.splitterMainAi = m_mainSplitter->GetSashPosition();
        if (m_leftSplitter && m_leftSplitter->IsSplit())
            d.splitterTreeList = m_leftSplitter->GetSashPosition();
        CollectExpandedTreePaths(d.treeOpenPaths);
        return d;
    }

    void ApplySessionLayout(const WaveformSessionData& session)
    {
        if (session.windowW > 0 && session.windowH > 0)
            SetSize(session.windowW, session.windowH);
        if (session.windowX >= 0 && session.windowY >= 0)
            SetPosition(wxPoint(session.windowX, session.windowY));

        if (m_splitter && m_splitter->IsSplit() && session.splitterTreeWave > 0)
            m_splitter->SetSashPosition(session.splitterTreeWave);
        if (m_mainSplitter && m_mainSplitter->IsSplit() && session.splitterMainAi > 0)
            m_mainSplitter->SetSashPosition(session.splitterMainAi);
        if (m_leftSplitter && m_leftSplitter->IsSplit() && session.splitterTreeList > 0)
            m_leftSplitter->SetSashPosition(session.splitterTreeList);
    }

    void ApplyTreeOpenPaths(const std::vector<std::string>& paths)
    {
        if (!m_signalTree || paths.empty())
            return;

        wxTreeListItem lastOk;
        for (const std::string& raw : paths) {
            wxTreeListItem item = FindTreeItemByModulePath(raw);
            if (!item.IsOk())
                continue;
            ExpandTreeToItem(item);
            lastOk = item;
        }
        if (lastOk.IsOk()) {
            m_signalTree->Select(lastOk);
            m_signalTree->EnsureVisible(lastOk);
            FillSignalListForModulePath(GetFullPath(m_signalTree, lastOk));
        }
    }

    static std::string ModulePathFromSignal(const signal_t* sig)
    {
        if (!sig)
            return "$root";
        std::string mp = NormalizeModulePathKey(sig->module_path);
        if (mp != "$root" && !mp.empty())
            return mp;
        if (!sig->full_name[0])
            return "$root";
        std::string fn = sig->full_name;
        const size_t dot = fn.rfind('.');
        if (dot == std::string::npos)
            return "$root";
        return fn.substr(0, dot);
    }

    wxTreeListItem FindVcdModulesRoot() const
    {
        if (!m_signalTree)
            return wxTreeListItem();
        const wxTreeListItem root = m_signalTree->GetRootItem();
        wxTreeListItem child = m_signalTree->GetFirstChild(root);
        while (child.IsOk()) {
            if (m_signalTree->GetItemText(child, 0) == "VCD Modules")
                return child;
            child = m_signalTree->GetNextSibling(child);
        }
        return wxTreeListItem();
    }

    wxTreeListItem FindTreeItemByModulePath(const std::string& modulePathIn) const
    {
        const wxTreeListItem modules = FindVcdModulesRoot();
        if (!modules.IsOk())
            return wxTreeListItem();

        std::string path = TrimModulePathForTree(modulePathIn);
        if (path.empty() || path == "$root")
            return modules;

        wxTreeListItem cur = modules;
        size_t start = 0;
        while (start < path.size()) {
            size_t dot = path.find('.', start);
            const std::string part = (dot == std::string::npos) ? path.substr(start) : path.substr(start, dot - start);
            start = (dot == std::string::npos) ? path.size() : dot + 1;
            if (part.empty())
                continue;

            wxTreeListItem found;
            wxTreeListItem child = m_signalTree->GetFirstChild(cur);
            while (child.IsOk()) {
                if (m_signalTree->GetItemText(child, 0) == wxString::FromUTF8(part)) {
                    found = child;
                    break;
                }
                child = m_signalTree->GetNextSibling(child);
            }
            if (!found.IsOk())
                return wxTreeListItem();
            cur = found;
        }
        return cur;
    }

    static std::string TrimModulePathForTree(std::string p)
    {
        while (!p.empty() && (p.back() == '.' || p.back() == '/'))
            p.pop_back();
        return p;
    }

    void ExpandTreeToItem(wxTreeListItem item) const
    {
        if (!m_signalTree || !item.IsOk())
            return;
        for (wxTreeListItem p = item; p.IsOk(); p = m_signalTree->GetItemParent(p))
            m_signalTree->Expand(p);
    }

    void CollectExpandedTreePaths(std::vector<std::string>& out) const
    {
        if (!m_signalTree)
            return;
        const wxTreeListItem modules = FindVcdModulesRoot();
        if (!modules.IsOk())
            return;

        std::function<void(wxTreeListItem)> walk;
        walk = [&](wxTreeListItem it) {
            if (!it.IsOk())
                return;
            if (it != modules && m_signalTree->IsExpanded(it)) {
                const std::string p = GetFullPath(m_signalTree, it);
                if (!p.empty()) {
                    bool dup = false;
                    for (const std::string& e : out) {
                        if (e == p) {
                            dup = true;
                            break;
                        }
                    }
                    if (!dup)
                        out.push_back(p);
                }
            }
            wxTreeListItem ch = m_signalTree->GetFirstChild(it);
            while (ch.IsOk()) {
                walk(ch);
                ch = m_signalTree->GetNextSibling(ch);
            }
        };
        walk(modules);
    }

    void FinishTraceLoadUI(const wxString& filePath, const wxString& ext)
    {
        UpdateTraceSliderForWavePanel();
        m_slider->SetValue(0);
        BuildSignalTreeView(m_signalTree, m_wavePanel->m_vcdData);
        SyncAiPanelFromWavePanel();
        SetStatusText("Loaded " + ext.Upper() + " trace: " + filePath);
        SyncTimeRangeUI();
    }

    void OpenScopeForSignal(signal_t* sig)
    {
        if (!sig || !m_signalTree) {
            SetStatusText("Open Scope: no signal or module tree.");
            return;
        }

        const std::string mp = ModulePathFromSignal(sig);
        wxTreeListItem item = FindTreeItemByModulePath(mp);
        if (!item.IsOk()) {
            SetStatusText("Open Scope: module not found in tree: " + wxString::FromUTF8(mp.c_str()));
            return;
        }

        ExpandTreeToItem(item);
        m_signalTree->Select(item);
        m_signalTree->EnsureVisible(item);
        FillSignalListForModulePath(GetFullPath(m_signalTree, item));

        const wxString sigLabel = sig->full_name[0] ? wxString::FromUTF8(sig->full_name)
                                                    : wxString::FromUTF8(sig->name);
        SetStatusText("Open Scope: " + wxString::FromUTF8(mp.c_str()) + " (" + sigLabel + ")");
    }

    bool ApplySessionState(const WaveformSessionData& session, wxString& err)
    {
        if (!m_wavePanel) {
            err = "waveform panel not ready";
            return false;
        }

        if (!session.tracePath.empty()) {
            const wxString tracePath = wxString::FromUTF8(session.tracePath.c_str());
            if (!wxFileName::FileExists(tracePath)) {
                err = "trace file not found: " + tracePath;
                return false;
            }
            SetProjectDir(tracePath);
        }

        m_wavePanel->ClearDisplaySignals();
        int added = 0;
        for (const SessionTrace& tr : session.traces) {
            signal_t* sig = FindSignalForScript(tr.name);
            if (!sig)
                continue;
            m_wavePanel->AddDisplaySignal(sig);
            if (tr.gtkwaveRadixCode >= 0) {
                const WaveformRadix::Radix r = WaveformRadix::FromGtkwaveCode(tr.gtkwaveRadixCode);
                m_wavePanel->m_signalDataFormats[sig] = WaveformPanel::FromWaveformRadix(r);
            }
            ++added;
        }

        if (session.displayRange > 0)
            m_wavePanel->ApplyVisibleTimeRange(session.timeOffset, session.timeOffset + session.displayRange);
        else if (session.timeOffset > 0)
            m_wavePanel->m_timeOffset = session.timeOffset;

        const long long ph = session.playhead > 0 ? session.playhead : session.timeOffset;
        m_wavePanel->SetCurrentTimestamp(ph, true);
        m_wavePanel->m_showCursorValue = session.showCursorValue;
        m_wavePanel->RequestDrawCacheRebuild();
        m_wavePanel->Refresh();
        SyncTimeRangeUI();

        ApplySessionLayout(session);
        if (!session.treeOpenPaths.empty())
            ApplyTreeOpenPaths(session.treeOpenPaths);

        if (added == 0 && !session.traces.empty())
            err = "session loaded but no trace names matched this dump";
        return true;
    }

    bool SaveSessionToPath(const wxString& path, wxString& err)
    {
        WaveformSessionData data = CollectSessionState();
        if (data.tracePath.empty() && !m_tracePathLabel.empty())
            data.tracePath = m_tracePathLabel.ToUTF8().data();
        if (!WaveformSession::Save(path, data, err))
            return false;
        m_sessionFilePath = path;
        return true;
    }

    void ApplyDataFormatToTargets(WaveformPanel::DataFormat fmt, bool forceAllDisplayed = false)
    {
        if (!m_wavePanel)
            return;

        std::vector<signal_t*> targets;
        if (!forceAllDisplayed
            && m_wavePanel->m_selectedSignalIndex >= 0
            && m_wavePanel->m_selectedSignalIndex < (int)m_wavePanel->m_displayedSignals2.size()) {
            if (signal_t* s = m_wavePanel->m_displayedSignals2[m_wavePanel->m_selectedSignalIndex])
                targets.push_back(s);
        } else if (!forceAllDisplayed && !m_wavePanel->m_highlightedSignals.empty()) {
            for (signal_t* s : m_wavePanel->m_highlightedSignals) {
                if (s)
                    targets.push_back(s);
            }
        } else {
            targets = m_wavePanel->m_displayedSignals2;
        }

        if (targets.empty()) {
            SetStatusText("No traces for radix — add signals or select a row first.");
            return;
        }

        for (signal_t* s : targets)
            m_wavePanel->m_signalDataFormats[s] = fmt;
        if (!targets.empty()) {
            m_wavePanel->RequestDrawCacheRebuild();
            m_wavePanel->Refresh();
        }

        SetStatusText(wxString::Format("Radix set on %zu trace(s).", targets.size()));
    }

    void OnReadSaveFile(wxCommandEvent&)
    {
        wxString defaultDir;
        if (!m_sessionFilePath.empty())
            defaultDir = wxFileName(m_sessionFilePath).GetPath();
        else if (!m_tracePathLabel.empty())
            defaultDir = wxFileName(m_tracePathLabel).GetPath();

        wxFileDialog openDialog(
            this,
            "Read session",
            defaultDir,
            "",
            SessionFileWildcard(),
            wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (openDialog.ShowModal() != wxID_OK)
            return;

        WaveformSessionData session;
        wxString err;
        if (!WaveformSession::Load(openDialog.GetPath(), session, err)) {
            wxMessageBox(err, "Session", wxOK | wxICON_ERROR);
            return;
        }

        if (!ApplySessionState(session, err)) {
            wxMessageBox(err, "Session", wxOK | wxICON_ERROR);
            return;
        }

        m_sessionFilePath = openDialog.GetPath();
        SetStatusText("Session loaded: " + m_sessionFilePath);
        if (!err.empty())
            wxLogWarning("%s", err);
    }
    
    void OnWriteSaveFile(wxCommandEvent& event)
    {
        if (m_sessionFilePath.empty()) {
            OnWriteSaveFileAs(event);
            return;
        }
        wxString err;
        if (!SaveSessionToPath(m_sessionFilePath, err))
            wxMessageBox(err, "Session", wxOK | wxICON_ERROR);
        else
            SetStatusText("Session saved: " + m_sessionFilePath);
    }
    
    void OnWriteSaveFileAs(wxCommandEvent&)
    {
        wxString defaultDir;
        wxString defaultName = "waveform.gtkw";
        if (!m_sessionFilePath.empty()) {
            defaultDir = wxFileName(m_sessionFilePath).GetPath();
            defaultName = wxFileName(m_sessionFilePath).GetFullName();
        } else if (!m_tracePathLabel.empty()) {
            defaultDir = wxFileName(m_tracePathLabel).GetPath();
            defaultName = wxFileName(m_tracePathLabel).GetName() + ".gtkw";
        }

        wxFileDialog saveDialog(
            this,
            "Write session as",
            defaultDir,
            defaultName,
            SessionFileWildcard(),
            wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
        if (saveDialog.ShowModal() != wxID_OK)
            return;

        wxString err;
        if (!SaveSessionToPath(saveDialog.GetPath(), err))
            wxMessageBox(err, "Session", wxOK | wxICON_ERROR);
        else
            SetStatusText("Session saved: " + m_sessionFilePath);
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
        if (!TclScriptEngine::IsAvailable()) {
            wxMessageBox(
                "This build was compiled without Tcl (libtcl).\n\n"
                "Install Tcl via vcpkg: vcpkg install tcl:x64-windows\n"
                "Then add preprocessor BEAR2WAVE_WITH_TCL, include path to tcl.h, "
                "and link tcl.lib (see docs/TCL_BUILD.md).",
                "Tcl not available",
                wxOK | wxICON_INFORMATION,
                this);
            return;
        }

        wxFileDialog openDialog(this, "Read Tcl Script File", "", "",
            "Tcl scripts (*.tcl)|*.tcl|All files (*.*)|*.*",
            wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (openDialog.ShowModal() != wxID_OK)
            return;

        const wxString path = openDialog.GetPath();
        WaveformCommandHandlers handlers = BuildScriptHandlers();
        std::string err;
        if (TclScriptEngine::RunFile(path.ToUTF8().data(), handlers, err)) {
            SetStatusText("Tcl script finished: " + path);
        } else {
            wxMessageBox(
                wxString::FromUTF8(err.c_str()),
                "Tcl script failed",
                wxOK | wxICON_ERROR,
                this);
            SetStatusText("Tcl script failed");
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
                file.Write(wxString::Format("Time range: %lld - %lld\n", m_wavePanel->m_timeOffset, m_wavePanel->m_timeOffset + m_wavePanel->m_displayTimeRange));
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
                        long long currentTime = m_wavePanel->m_timeOffset;
                        long long endTime = m_wavePanel->m_timeOffset + m_wavePanel->m_displayTimeRange;
                        wxString lastValue = "";
                        const long long stepA = std::max(1LL, m_wavePanel->m_displayTimeRange / 100);
                        
                        for (long long t = currentTime; t <= endTime; t += stepA)
                        {
                            const char* value = m_wavePanel->GetSignalValueAt(sig, t);
                        wxString valueStr = value;
                        if (valueStr != lastValue || t == currentTime)
                        {
                            file.Write(wxString::Format("%8lld: %s\n", t, value));
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
                long long currentTime = m_wavePanel->m_timeOffset;
                long long endTime = m_wavePanel->m_timeOffset + m_wavePanel->m_displayTimeRange;
                long long step = std::max(1LL, m_wavePanel->m_displayTimeRange / 100);
                
                // 确保至少有一个时间点
                if (currentTime > endTime)
                {
                    currentTime = 0;
                    endTime = m_wavePanel->m_maxTimestamp;
                    step = std::max(1LL, endTime / 100);
                }
                
                for (long long t = currentTime; t <= endTime; t += step)
                {
                    file.Write(wxString::Format("%lld", t));
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
                                const timestamp_t tt = (t < 0) ? 0 : (timestamp_t)t;
                                size_t c;
                                for (c = 0; c < sig->changes_count; c++) {
                                    if (sig->value_changes[c].timestamp > tt)
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
            memset(sig, 0, sizeof(*sig));
            sig->fst_var_type = -1;
            strncpy(sig->name, signalName.c_str(), VCD_NAME_SIZE - 1);
            sig->name[VCD_NAME_SIZE - 1] = '\0';
            strncpy(sig->module_path, "CSV", VCD_SIGNAL_SIZE - 1);
            sig->module_path[VCD_SIGNAL_SIZE - 1] = '\0';
            snprintf(sig->full_name, VCD_SIGNAL_SIZE, "CSV.%s", signalName.c_str());
            snprintf(sig->signal_id, VCD_NAME_SIZE, "%s", signalName.c_str());
            sig->size = 1;
            sig->changes_count = 0;
            sig->changes_capacity = 0;
            sig->value_changes = nullptr;
            
            // 填充信号值变化
            for (size_t row = 0; row < parser.GetRowCount(); row++) {
                std::string timeStr = parser.GetValue(row, timeHeader);
                std::string valueStr = parser.GetValue(row, signalName);
                
                if (!timeStr.empty() && !valueStr.empty()) {
                    try {
                        const long long tsll = std::stoll(timeStr);
                        const timestamp_t timestamp = (tsll < 0) ? 0 : (timestamp_t)tsll;
                        if (vcd_signal_append_change(sig, timestamp, valueStr.c_str()) != 0)
                            break;
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
                m_csvHeapSignals.push_back(sig);
            } else {
                signal_free_value_changes(sig);
                delete sig;
            }
        }
        
        // 更新波形面板的信号列表（与 VCD/FST 一致：默认波形区为空，由用户在列表中双击添加）
        m_wavePanel->m_allSignals = m_displayedSignals;
        m_wavePanel->m_displayedSignals2.clear();
        long long csvMax = 0;
        for (signal_t* s : m_displayedSignals) {
            if (!s || !s->value_changes || s->changes_count == 0) continue;
            csvMax = std::max(csvMax, (long long)s->value_changes[s->changes_count - 1].timestamp);
        }
        m_wavePanel->m_maxTimestamp = csvMax > 0 ? csvMax : 1000LL;
        m_wavePanel->m_displayTimeRange = m_wavePanel->m_maxTimestamp;
        m_wavePanel->m_timeOffset = 0;
        UpdateTraceSliderForWavePanel();
        m_wavePanel->AssignSignalColors();
        m_wavePanel->RequestDrawCacheRebuild();
    }
    
    void OnOpenNewLab(wxCommandEvent&)
    {
        wxFileDialog dlg(this, "Open File", "", "",
            "Traces (*.vcd;*.fst;*.vzt;*.lxt;*.lxt2;*.ghw)|*.vcd;*.fst;*.vzt;*.lxt;*.lxt2;*.ghw|"
            "VCD (*.vcd)|*.vcd|FST (*.fst)|*.fst|VZT (*.vzt)|*.vzt|LXT (*.lxt;*.lxt2)|*.lxt;*.lxt2|GHW (*.ghw)|*.ghw|"
            "CSV (*.csv)|*.csv|All files (*.*)|*.*",
            wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dlg.ShowModal() == wxID_OK)
        {
            wxString filePath = dlg.GetPath();
            wxFileName fileName(filePath);
            wxString extension = fileName.GetExt().Lower();
            
            m_wavePanel->ClearWavePanel();
            ReleaseCsvHeapSignals();
            m_displayedSignals.clear();
            m_moduleToSignals.clear();
            
            if (extension == "vcd")
            {
                // 加载VCD文件
                m_wavePanel->OpenVCDFile(filePath);
                UpdateTraceSliderForWavePanel();
                m_slider->SetValue(0);
                BuildSignalTreeView(m_signalTree, m_wavePanel->m_vcdData);
                
                // 将所有信号传递给AI分析面板
                SyncAiPanelFromWavePanel();
                
                wxMessageBox("Successfully loaded VCD file: " + filePath);
                SyncTimeRangeUI();
            }
            else if (extension == "fst")
            {
                m_wavePanel->OpenFSTFile(filePath);
                UpdateTraceSliderForWavePanel();
                m_slider->SetValue(0);
                BuildSignalTreeView(m_signalTree, m_wavePanel->m_vcdData);
                SyncAiPanelFromWavePanel();
                wxMessageBox("Successfully loaded FST file: " + filePath);
                SyncTimeRangeUI();
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
                    
                    UpdateTraceSliderForWavePanel();
                    m_slider->SetValue(0);
                    
                    // 将所有信号传递给AI分析面板
                    SyncAiPanelFromWavePanel();
                    
                    wxMessageBox("Successfully loaded CSV file: " + filePath);
                    SyncTimeRangeUI();
                }
                else
                {
                    wxMessageBox("Failed to load CSV file: " + filePath);
                }
            }
            else
            {
                wxMessageBox("Unsupported file format. Please select a VCD, FST, or CSV file.");
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
    long long m_lastTimeOffset = 0;
    long long m_lastDisplayTimeRange = 1000;
    
    void SaveCurrentView()
    {
        m_lastTimeOffset = m_wavePanel->m_timeOffset;
        m_lastDisplayTimeRange = m_wavePanel->m_displayTimeRange;
    }
    
    void OnMoveToTime(wxCommandEvent&)
    {
        wxString val = wxGetTextFromUser("Enter time:", "Move To Time", 
            wxString::Format("%lld", m_wavePanel->m_timeOffset));
        long long t = 0;
        if (val.ToLongLong(&t))
        {
            SaveCurrentView();
            m_wavePanel->m_timeOffset = t;
            if (m_wavePanel->m_timeOffset < 0) m_wavePanel->m_timeOffset = 0;
            if (m_wavePanel->m_timeOffset > m_wavePanel->m_maxTimestamp - m_wavePanel->m_displayTimeRange)
                m_wavePanel->m_timeOffset = m_wavePanel->m_maxTimestamp - m_wavePanel->m_displayTimeRange;
            m_wavePanel->RequestDrawCacheRebuild();
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
        m_wavePanel->RequestDrawCacheRebuild();
        m_wavePanel->Refresh();
        SyncTimeRangeUI();
    }
    
    void OnZoomLast(wxCommandEvent&)
    {
        long long tempOffset = m_wavePanel->m_timeOffset;
        long long tempRange = m_wavePanel->m_displayTimeRange;
        
        m_wavePanel->m_timeOffset = m_lastTimeOffset;
        m_wavePanel->m_displayTimeRange = m_lastDisplayTimeRange;
        
        m_lastTimeOffset = tempOffset;
        m_lastDisplayTimeRange = tempRange;
        
        m_wavePanel->RequestDrawCacheRebuild();
        m_wavePanel->Refresh();
        SyncTimeRangeUI();
    }
    
    void OnFetchMore(wxCommandEvent&)
    {
        SaveCurrentView();
        long long center = m_wavePanel->m_timeOffset + m_wavePanel->m_displayTimeRange / 2;
        m_wavePanel->m_displayTimeRange *= 2;
        if (m_wavePanel->m_displayTimeRange > m_wavePanel->m_maxTimestamp)
            m_wavePanel->m_displayTimeRange = m_wavePanel->m_maxTimestamp;
        m_wavePanel->m_timeOffset = center - m_wavePanel->m_displayTimeRange / 2;
        if (m_wavePanel->m_timeOffset < 0) m_wavePanel->m_timeOffset = 0;
        m_wavePanel->RequestDrawCacheRebuild();
        m_wavePanel->Refresh();
        SyncTimeRangeUI();
    }
    
    void OnFetchAll(wxCommandEvent&)
    {
        SaveCurrentView();
        m_wavePanel->m_timeOffset = 0;
        m_wavePanel->m_displayTimeRange = m_wavePanel->m_maxTimestamp;
        m_wavePanel->RequestDrawCacheRebuild();
        m_wavePanel->Refresh();
        SyncTimeRangeUI();
    }
    
    void OnDiscardToStart(wxCommandEvent&)
    {
        SaveCurrentView();
        long long newStart = m_wavePanel->m_timeOffset + m_wavePanel->m_displayTimeRange / 4;
        if (newStart < m_wavePanel->m_maxTimestamp)
        {
            m_wavePanel->m_timeOffset = newStart;
            m_wavePanel->m_displayTimeRange = m_wavePanel->m_maxTimestamp - newStart;
            m_wavePanel->RequestDrawCacheRebuild();
            m_wavePanel->Refresh();
            SyncTimeRangeUI();
        }
    }
    
    void OnDiscardToEnd(wxCommandEvent&)
    {
        SaveCurrentView();
        long long newEnd = m_wavePanel->m_timeOffset + m_wavePanel->m_displayTimeRange * 3 / 4;
        if (newEnd > 0)
        {
            m_wavePanel->m_displayTimeRange = newEnd;
            m_wavePanel->RequestDrawCacheRebuild();
            m_wavePanel->Refresh();
            SyncTimeRangeUI();
        }
    }
    
    void OnShiftLeft(wxCommandEvent&)
    {
        SaveCurrentView();
        long long shift = m_wavePanel->m_displayTimeRange / 4;
        m_wavePanel->m_timeOffset -= shift;
        if (m_wavePanel->m_timeOffset < 0) m_wavePanel->m_timeOffset = 0;
        m_wavePanel->RequestDrawCacheRebuild();
        m_wavePanel->Refresh();
        SyncTimeRangeUI();
    }
    
    void OnShiftRight(wxCommandEvent&)
    {
        SaveCurrentView();
        long long shift = m_wavePanel->m_displayTimeRange / 4;
        m_wavePanel->m_timeOffset += shift;
        if (m_wavePanel->m_timeOffset + m_wavePanel->m_displayTimeRange > m_wavePanel->m_maxTimestamp)
            m_wavePanel->m_timeOffset = m_wavePanel->m_maxTimestamp - m_wavePanel->m_displayTimeRange;
        m_wavePanel->RequestDrawCacheRebuild();
        m_wavePanel->Refresh();
        SyncTimeRangeUI();
    }
    
    void OnPageLeft(wxCommandEvent&)
    {
        if (!m_wavePanel) return;
        SaveCurrentView();
        m_wavePanel->PageLeft();
        SyncTimeRangeUI();
    }

    void OnPageRight(wxCommandEvent&)
    {
        if (!m_wavePanel) return;
        SaveCurrentView();
        m_wavePanel->PageRight();
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
        m_wavePanel->RequestDrawCacheRebuild();
        m_wavePanel->Refresh();
    }
    
    void OnInsertComment(wxCommandEvent&)
    {
        wxString commentText = wxGetTextFromUser("Enter comment text:", "Insert Comment", "", this);
        if (!commentText.IsEmpty()) {
            const int pos = m_wavePanel->m_selectedSignalIndex >= 0
                ? m_wavePanel->m_selectedSignalIndex
                : (int)m_wavePanel->m_displayedSignals2.size();
            m_wavePanel->ShiftRowCommentsOnInsert(pos);
            m_wavePanel->m_displayedSignals2.insert(m_wavePanel->m_displayedSignals2.begin() + pos, nullptr);
            m_wavePanel->m_rowComments[pos] = commentText;
            m_wavePanel->RequestDrawCacheRebuild();
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
                m_wavePanel->RequestDrawCacheRebuild();
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
            m_wavePanel->RequestDrawCacheRebuild();
            m_wavePanel->Refresh();
        }
    }
    
    void OnDelete(wxCommandEvent&)
    {
        if (m_wavePanel->m_selectedSignalIndex >= 0 && m_wavePanel->m_selectedSignalIndex < (int)m_wavePanel->m_displayedSignals2.size()) {
            // 从显示列表中删除
            m_wavePanel->m_displayedSignals2.erase(m_wavePanel->m_displayedSignals2.begin() + m_wavePanel->m_selectedSignalIndex);
            m_wavePanel->m_selectedSignalIndex = -1;
            m_wavePanel->RequestDrawCacheRebuild();
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
                
                m_wavePanel->RequestDrawCacheRebuild();
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
            
            m_wavePanel->RequestDrawCacheRebuild();
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
            
            m_wavePanel->RequestDrawCacheRebuild();
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
                m_wavePanel->RequestDrawCacheRebuild();
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
                m_wavePanel->RequestDrawCacheRebuild();
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
                
                m_wavePanel->RequestDrawCacheRebuild();
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
    
    void OnDataFormatBinary(wxCommandEvent&) { ApplyDataFormatToTargets(WaveformPanel::FORMAT_BINARY); }
    void OnDataFormatOctal(wxCommandEvent&) { ApplyDataFormatToTargets(WaveformPanel::FORMAT_OCTAL); }
    void OnDataFormatDecimal(wxCommandEvent&) { ApplyDataFormatToTargets(WaveformPanel::FORMAT_DECIMAL); }
    void OnDataFormatHexadecimal(wxCommandEvent&) { ApplyDataFormatToTargets(WaveformPanel::FORMAT_HEXADECIMAL); }
    void OnDataFormatASCII(wxCommandEvent&) { ApplyDataFormatToTargets(WaveformPanel::FORMAT_ASCII); }
    void OnDataFormatSignedDecimal(wxCommandEvent&) { ApplyDataFormatToTargets(WaveformPanel::FORMAT_SIGNED_DECIMAL); }
    void OnDataFormatReal(wxCommandEvent&) { ApplyDataFormatToTargets(WaveformPanel::FORMAT_REAL); }
    void OnDataFormatApplyAll(wxCommandEvent& e)
    {
        (void)e;
        if (!m_wavePanel || m_wavePanel->m_displayedSignals2.empty()) {
            SetStatusText("No displayed traces.");
            return;
        }
        WaveformPanel::DataFormat fmt = WaveformPanel::FORMAT_BINARY;
        if (m_wavePanel->m_selectedSignalIndex >= 0
            && m_wavePanel->m_selectedSignalIndex < (int)m_wavePanel->m_displayedSignals2.size()) {
            signal_t* ref = m_wavePanel->m_displayedSignals2[m_wavePanel->m_selectedSignalIndex];
            if (ref)
                fmt = m_wavePanel->DataFormatForSignal(ref);
        }
        ApplyDataFormatToTargets(fmt, true);
    }
    
    // Color Format 子菜单事件处理函数
    void OnColorFormatDefault(wxCommandEvent&)
    {
        m_wavePanel->m_globalColorFormat = WaveformPanel::COLOR_DEFAULT;
        m_wavePanel->AssignSignalColors();
        m_wavePanel->RequestDrawCacheRebuild();
        m_wavePanel->Refresh();
    }
    
    void OnColorFormatSignalName(wxCommandEvent&)
    {
        m_wavePanel->m_globalColorFormat = WaveformPanel::COLOR_SIGNAL_NAME;
        m_wavePanel->AssignSignalColors();
        m_wavePanel->RequestDrawCacheRebuild();
        m_wavePanel->Refresh();
    }
    
    void OnColorFormatValue(wxCommandEvent&)
    {
        m_wavePanel->m_globalColorFormat = WaveformPanel::COLOR_VALUE;
        m_wavePanel->AssignSignalColors();
        m_wavePanel->RequestDrawCacheRebuild();
        m_wavePanel->Refresh();
    }
    
    void OnColorFormatModule(wxCommandEvent&)
    {
        m_wavePanel->m_globalColorFormat = WaveformPanel::COLOR_MODULE;
        m_wavePanel->AssignSignalColors();
        m_wavePanel->RequestDrawCacheRebuild();
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
        m_wavePanel->RequestDrawCacheRebuild();
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
        m_wavePanel->RequestDrawCacheRebuild();
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
        
        long long currentTime = m_wavePanel->GetCursorSimTime();
        
        // 按信号当前值排序
        std::sort(m_wavePanel->m_displayedSignals2.begin(), m_wavePanel->m_displayedSignals2.end(), [&](signal_t* a, signal_t* b) {
            std::string valA = m_wavePanel->GetValueAt(a, currentTime);
            std::string valB = m_wavePanel->GetValueAt(b, currentTime);
            return valA < valB;
        });
        
        // 重新构建绘制缓存
        m_wavePanel->RequestDrawCacheRebuild();
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
        m_wavePanel->RequestDrawCacheRebuild();
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
                timeCtrl->SetValue(wxString::Format("%lld", mk.timestamp));
            }
        });
        
        // 显示对话框
        if (dialog->ShowModal() == wxID_OK) {
            int selected = markerList->GetSelection();
            if (selected >= 0 && selected < (int)m_wavePanel->m_markers.size()) {
                // 更新选中的标记
                m_wavePanel->m_markers[selected].label = nameCtrl->GetValue();
                long long timestamp = 0;
                if (timeCtrl->GetValue().ToLongLong(&timestamp)) {
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
            long long currentTime = m_wavePanel->GetCursorSimTime();
            m_wavePanel->AddMarker(currentTime, markerName);
            wxMessageBox(wxString::Format("Marker '%s' added at time %lld.", markerName, currentTime));
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

    static std::string NormalizeModulePathKey(const char* mp)
    {
        if (!mp || !mp[0])
            return "$root";
        return std::string(mp);
    }

    void FillSignalListForModulePath(const std::string& modulePath)
    {
        m_signalList->DeleteAllItems();
        auto it = m_moduleToSignals.find(modulePath);
        if (it == m_moduleToSignals.end() && modulePath == "$root") {
            it = m_moduleToSignals.find("");
            if (it == m_moduleToSignals.end())
                it = m_moduleToSignals.find("$root");
        }
        if (it == m_moduleToSignals.end())
            return;
        const int maxRows = WaveformPerf::MaxSignalListRows();
        int shown = 0;
        const size_t total = it->second.size();
        for (signal_t* sig : it->second) {
            if (!sig)
                continue;
            if (shown >= maxRows) {
                long idx = m_signalList->InsertItem(m_signalList->GetItemCount(), wxT("…"));
                m_signalList->SetItem(idx, 1,
                    wxString::Format(
                        wxT("(%zu signals total — showing first %d; narrow module or use search)"),
                        total,
                        maxRows));
                break;
            }
            long idx = m_signalList->InsertItem(m_signalList->GetItemCount(), "wire");
            m_signalList->SetItem(idx, 1, wxString::FromUTF8(sig->name));
            m_signalList->SetItemPtrData(idx, (wxUIntPtr)sig);
            ++shown;
        }
    }

    void FillSignalListAllFromModules()
    {
        m_signalList->DeleteAllItems();
        std::vector<signal_t*> flat;
        std::unordered_set<signal_t*> seen;
        for (const auto& kv : m_moduleToSignals) {
            for (signal_t* sig : kv.second) {
                if (!sig || seen.count(sig)) continue;
                seen.insert(sig);
                flat.push_back(sig);
            }
        }
        std::sort(flat.begin(), flat.end(), [](const signal_t* a, const signal_t* b) {
            return std::strcmp(a->full_name, b->full_name) < 0;
        });
        const int maxRows = WaveformPerf::MaxSignalListRows();
        const size_t total = flat.size();
        int shown = 0;
        for (signal_t* sig : flat) {
            if (shown >= maxRows) {
                long idx = m_signalList->InsertItem(m_signalList->GetItemCount(), wxT("…"));
                m_signalList->SetItem(idx, 1,
                    wxString::Format(
                        wxT("(%zu signals — first %d shown; select a module in the tree)"),
                        total,
                        maxRows));
                break;
            }
            long idx = m_signalList->InsertItem(m_signalList->GetItemCount(), "wire");
            m_signalList->SetItem(idx, 1, wxString::FromUTF8(sig->full_name[0] ? sig->full_name : sig->name));
            m_signalList->SetItemPtrData(idx, (wxUIntPtr)sig);
            ++shown;
        }
    }

    // 点击模块 → 显示直属信号（不含子模块）
    void OnTreeSelect(wxTreeListEvent& event)
    {
        wxTreeListItem item = event.GetItem();
        if (!item.IsOk()) return;

        std::string modulePath = GetFullPath(m_signalTree, item);
        if (modulePath.empty()) {
            FillSignalListAllFromModules();
            return;
        }
        FillSignalListForModulePath(modulePath);
    }

    std::string GetFullPath(wxTreeListCtrl* tree, wxTreeListItem item) const
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
            m_moduleToSignals.clear();
            for (signal_node_t* node = vcd->signals_head; node; node = node->next) {
                signal_t* sig = &node->signal;
                const std::string modulePath = NormalizeModulePathKey(sig->module_path);
                m_moduleToSignals[modulePath].push_back(sig);
            }
            for (const auto& pair : m_moduleToSignals) {
                const std::string& modulePath = pair.first;
                std::string fullPath;
                wxTreeListItem parent = rootId;
                size_t start = 0;
                while (start < modulePath.size()) {
                    size_t dot = modulePath.find('.', start);
                    std::string part = (dot == std::string::npos)
                        ? modulePath.substr(start)
                        : modulePath.substr(start, dot - start);
                    start = (dot == std::string::npos) ? modulePath.size() : dot + 1;
                    if (part.empty())
                        continue;
                    if (!fullPath.empty())
                        fullPath += ".";
                    fullPath += part;

                    if (pathMap.count(fullPath)) {
                        parent = pathMap[fullPath];
                        continue;
                    }
                    wxTreeListItem item = tree->AppendItem(parent, wxString::FromUTF8(part));
                    pathMap[fullPath] = item;
                    parent = item;
                }
            }
            FillSignalListAllFromModules();
        }
        else
        {
            // 处理CSV文件
            for (const auto& pair : m_moduleToSignals)
            {
                const std::string modulePath = NormalizeModulePathKey(pair.first.c_str());

                std::string fullPath;
                wxTreeListItem parent = rootId;
                size_t start = 0;
                while (start < modulePath.size()) {
                    size_t dot = modulePath.find('.', start);
                    std::string part = (dot == std::string::npos) ? modulePath.substr(start)
                                                                 : modulePath.substr(start, dot - start);
                    start = (dot == std::string::npos) ? modulePath.size() : dot + 1;
                    if (part.empty()) continue;
                    if (!fullPath.empty()) fullPath += ".";
                    fullPath += part;

                    if (pathMap.count(fullPath))
                    {
                        parent = pathMap[fullPath];
                        continue;
                    }
                    wxTreeListItem item = tree->AppendItem(parent, wxString::FromUTF8(part));
                    pathMap[fullPath] = item;
                    parent = item;
                }
            }
            FillSignalListAllFromModules();
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
        long long mx = m_wavePanel->m_maxTimestamp;
        long long cur = SliderValueToTraceTime(m_slider->GetValue());
        if (cur < mx) {
            long long step = std::max(1LL, m_wavePanel->m_displayTimeRange / 100);
            cur = std::min(mx, cur + step);
            static unsigned s_dbgPlayTick = 0;
            if ((++s_dbgPlayTick % 8u) == 0u) {
                wxLogDebug(
                    "[Bear2Wave][Frame] OnTimer: cur=%lld step=%lld mx=%lld slider=%d",
                    cur,
                    step,
                    mx,
                    m_slider->GetValue());
            }
            m_wavePanel->SetCurrentTimestamp(cur, false);
            m_suppressSliderPlayheadSync = true;
            m_slider->SetValue(TraceTimeToSliderValue(cur));
            m_suppressSliderPlayheadSync = false;
        }
        else {
            m_timer->Stop();
            m_playBtn->SetLabel("Play");
        }
        SyncTimeRangeUI();
    }

    void OnSlide(wxCommandEvent&)
    {
        if (m_suppressSliderPlayheadSync) return;
        if (!m_wavePanel) return;
        const int pos = m_slider->GetValue();
        const long long t = SliderValueToTraceTime(pos);
        static long long s_dbgLastSlideT = LLONG_MIN;
        if (t != s_dbgLastSlideT) {
            s_dbgLastSlideT = t;
            wxLogDebug(
                "[Bear2Wave][Frame] OnSlide: pos=%d/%d -> t=%lld maxTs=%lld",
                pos,
                m_slider->GetMax(),
                t,
                m_wavePanel->m_maxTimestamp);
        }
        m_wavePanel->SetCurrentTimestamp(t, false);
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

        m_tracePathLabel = filePath;

        wxFileName fileName(filePath);
        wxString extension = fileName.GetExt().Lower();

        // 检查m_wavePanel是否为nullptr
        if (!m_wavePanel)
        {
            // 使用日志代替消息框，避免wxMessageBox的问题
            wxLogError("Waveform panel not initialized!");
            return;
        }

        m_wavePanel->ClearWavePanel();
        ReleaseCsvHeapSignals();
        m_displayedSignals.clear();
        m_moduleToSignals.clear();

        if (extension == "vcd" || extension == "fst" || extension == "vzt" || extension == "lxt"
            || extension == "lxt2" || extension == "ghw")
        {
            m_wavePanel->OpenTraceFile(filePath);
            if (!m_wavePanel->m_vcdData) {
                SetStatusText("Trace load failed: " + filePath);
                wxLogError("trace_load failed for %s", filePath);
                return;
            }
            FinishTraceLoadUI(filePath, extension);
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

                UpdateTraceSliderForWavePanel();
                m_slider->SetValue(0);

                // 将所有信号传递给AI分析面板
                SyncAiPanelFromWavePanel();

                SetStatusText("Successfully loaded CSV file: " + filePath);
                SyncTimeRangeUI();
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

        UpdateWindowTitle();
    }

    void UpdateTraceSliderForWavePanel()
    {
        if (!m_wavePanel || !m_slider) return;
        long long mx = m_wavePanel->m_maxTimestamp;
        if (mx < 1) mx = 1;
        /* One linear map: slider 0 .. divisions <-> sim time 0 .. m_maxTimestamp (matches play step). */
        m_slider->SetRange(0, BEAR2WAVE_SLIDER_DIVISIONS);
    }

    long long SliderValueToTraceTime(int sliderVal) const
    {
        if (!m_wavePanel) return 0;
        long long mx = m_wavePanel->m_maxTimestamp;
        if (mx < 1) return 0;
        const int smax = m_slider ? m_slider->GetMax() : BEAR2WAVE_SLIDER_DIVISIONS;
        const double ratio = smax > 0 ? (double)sliderVal / (double)smax : 0.0;
        long long t = (long long)(ratio * (double)mx + 0.5);
        if (t < 0) t = 0;
        if (t > mx) t = mx;
        return t;
    }

    int TraceTimeToSliderValue(long long t) const
    {
        if (!m_wavePanel || !m_slider) return 0;
        long long mx = m_wavePanel->m_maxTimestamp;
        if (mx < 1) return 0;
        const int smax = m_slider->GetMax();
        int v = (int)((double)t / (double)mx * (double)smax + 0.5);
        if (v < 0) v = 0;
        if (v > smax) v = smax;
        return v;
    }
};
