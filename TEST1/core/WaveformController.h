#pragma once

#include <wx/string.h>

class TraceDocument;
class WaveformPanel;

/** Viewer commands: trace load + zoom/playhead (E5-1 phase 2). */
class WaveformController {
public:
    WaveformController(TraceDocument& document, WaveformPanel& panel);

    TraceDocument& Document() { return m_document; }
    const TraceDocument& Document() const { return m_document; }
    WaveformPanel& Panel() { return m_panel; }

    bool LoadTrace(const wxString& path, wxString& errOut, wxString* warnOut = nullptr);
    void ClearTraceView();

    void ZoomIn();
    void ZoomOut();
    void ZoomFull();
    void PageLeft();
    void PageRight();
    void SetPlayhead(long long t, bool centerView = true);

private:
    TraceDocument& m_document;
    WaveformPanel& m_panel;
};
