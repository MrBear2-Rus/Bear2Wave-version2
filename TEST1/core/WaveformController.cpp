#include "core/WaveformController.h"

#include "core/TraceDocument.h"
#include "panels/WaveformPanel.hpp"

WaveformController::WaveformController(TraceDocument& document, WaveformPanel& panel)
    : m_document(document)
    , m_panel(panel)
{
}

bool WaveformController::LoadTrace(const wxString& path, wxString& errOut, wxString* warnOut)
{
    m_document.ResetForNewFile();
    m_panel.ClearWavePanel();
    if (!m_document.OpenTrace(path, errOut, warnOut))
        return false;
    m_panel.SetVcdData(m_document.Vcd());
    return true;
}

void WaveformController::ClearTraceView()
{
    m_document.ResetForNewFile();
    m_panel.ClearWavePanel();
}

void WaveformController::ZoomIn()
{
    m_panel.ZoomIn();
}

void WaveformController::ZoomOut()
{
    m_panel.ZoomOut();
}

void WaveformController::ZoomFull()
{
    m_panel.ZoomReset();
}

void WaveformController::PageLeft()
{
    m_panel.PageLeft();
}

void WaveformController::PageRight()
{
    m_panel.PageRight();
}

void WaveformController::SetPlayhead(long long t, bool centerView)
{
    m_panel.SetCurrentTimestamp(t, centerView);
}
