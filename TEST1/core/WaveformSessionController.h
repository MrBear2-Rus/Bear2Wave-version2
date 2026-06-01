#pragma once

#include "core/WaveformSession.h"
#include "vcd.h"

#include <functional>

class TraceDocument;
class WaveformPanel;
class SignalModuleTree;
class wxFrame;
class wxSplitterWindow;
class wxWindow;

/** Collect/apply viewer session state (.gtkw / .bwv / .b2w) — E5-1 phase 3. */
class WaveformSessionController {
public:
    struct Host {
        wxFrame* frame = nullptr;
        wxSplitterWindow* splitterTreeWave = nullptr;
        wxSplitterWindow* splitterMainAi = nullptr;
        wxSplitterWindow* splitterTreeList = nullptr;
        wxWindow* aiPanel = nullptr;
        std::function<void(const wxString&)> loadTracePath;
        std::function<signal_t*(const std::string&)> findSignal;
        std::function<void()> syncTimeRangeUI;
        std::function<void()> syncAiPanel;
    };

    WaveformSessionController(
        TraceDocument& document,
        WaveformPanel& panel,
        SignalModuleTree& moduleTree,
        Host host);

    WaveformSessionData Collect() const;
    bool Apply(const WaveformSessionData& session, wxString& err);
    bool SaveToPath(const wxString& path, wxString& err);

    static const char* FileWildcard();

private:
    void CollectWindowLayout(WaveformSessionData& d) const;
    void ApplyWindowLayout(const WaveformSessionData& session) const;

    TraceDocument& m_document;
    WaveformPanel& m_panel;
    SignalModuleTree& m_moduleTree;
    Host m_host;
};
