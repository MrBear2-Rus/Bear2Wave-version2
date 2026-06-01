#include "core/WaveformSessionController.h"

#include "core/TraceDocument.h"
#include "core/WaveformRadix.h"
#include "panels/WaveformPanel.hpp"
#include "ui/SignalModuleTree.h"

#include <wx/filename.h>
#include <wx/splitter.h>

WaveformSessionController::WaveformSessionController(
    TraceDocument& document,
    WaveformPanel& panel,
    SignalModuleTree& moduleTree,
    Host host)
    : m_document(document)
    , m_panel(panel)
    , m_moduleTree(moduleTree)
    , m_host(std::move(host))
{
}

const char* WaveformSessionController::FileWildcard()
{
    return "Session (*.bwv;*.gtkw;*.b2w)|*.bwv;*.gtkw;*.b2w|"
           "Bear2Wave (*.bwv;*.b2w)|*.bwv;*.b2w|"
           "GTKWave (*.gtkw)|*.gtkw|All (*.*)|*.*";
}

void WaveformSessionController::CollectWindowLayout(WaveformSessionData& d) const
{
    if (m_host.aiPanel)
        d.aiPanelVisible = m_host.aiPanel->IsShown();

    if (m_host.frame) {
        const wxSize sz = m_host.frame->GetSize();
        const wxPoint pos = m_host.frame->GetPosition();
        d.windowW = sz.GetWidth();
        d.windowH = sz.GetHeight();
        d.windowX = pos.x;
        d.windowY = pos.y;
    }

    if (m_host.splitterTreeWave && m_host.splitterTreeWave->IsSplit())
        d.splitterTreeWave = m_host.splitterTreeWave->GetSashPosition();
    if (m_host.splitterMainAi && m_host.splitterMainAi->IsSplit())
        d.splitterMainAi = m_host.splitterMainAi->GetSashPosition();
    if (m_host.splitterTreeList && m_host.splitterTreeList->IsSplit())
        d.splitterTreeList = m_host.splitterTreeList->GetSashPosition();
}

void WaveformSessionController::ApplyWindowLayout(const WaveformSessionData& session) const
{
    if (m_host.frame) {
        if (session.windowW > 0 && session.windowH > 0)
            m_host.frame->SetSize(session.windowW, session.windowH);
        if (session.windowX >= 0 && session.windowY >= 0)
            m_host.frame->SetPosition(wxPoint(session.windowX, session.windowY));
    }

    if (m_host.splitterTreeWave && m_host.splitterTreeWave->IsSplit() && session.splitterTreeWave > 0)
        m_host.splitterTreeWave->SetSashPosition(session.splitterTreeWave);
    if (m_host.splitterMainAi && m_host.splitterMainAi->IsSplit() && session.splitterMainAi > 0)
        m_host.splitterMainAi->SetSashPosition(session.splitterMainAi);
    if (m_host.splitterTreeList && m_host.splitterTreeList->IsSplit() && session.splitterTreeList > 0)
        m_host.splitterTreeList->SetSashPosition(session.splitterTreeList);

    if (m_host.aiPanel) {
        if (session.aiPanelVisible) {
            if (!m_host.aiPanel->IsShown())
                m_host.aiPanel->Show();
        } else if (m_host.aiPanel->IsShown()) {
            m_host.aiPanel->Hide();
        }
    }

    if (m_host.splitterMainAi)
        m_host.splitterMainAi->Layout();
}

WaveformSessionData WaveformSessionController::Collect() const
{
    WaveformSessionData d;
    d.formatVersion = 3;
    if (!m_document.TracePath().empty())
        d.tracePath = m_document.TracePath().ToUTF8().data();

    d.timeOffset = m_panel.m_timeOffset;
    d.displayRange = m_panel.m_displayTimeRange;
    d.playhead = m_panel.m_currentTimestamp;
    d.showCursorValue = m_panel.m_showCursorValue;
    d.selectedSignalIndex = m_panel.m_selectedSignalIndex;
    d.hasMeasureMarkers = m_panel.m_hasMarkerA;
    d.measureMarkerA = m_panel.m_markerA;
    d.measureMarkerB = m_panel.m_markerB;
    d.hasBaseline = m_panel.m_hasBaseline;
    d.baselineTimestamp = m_panel.m_baselineTimestamp;

    for (size_t i = 0; i < m_panel.m_displayedSignals2.size(); ++i) {
        const auto commentIt = m_panel.m_rowComments.find((int)i);
        if (commentIt != m_panel.m_rowComments.end()) {
            SessionDisplayRow row;
            row.kind = SessionRowKind::Comment;
            row.text = commentIt->second.ToUTF8().data();
            d.displayRows.push_back(std::move(row));
            continue;
        }

        signal_t* sig = m_panel.m_displayedSignals2[i];
        if (!sig) {
            SessionDisplayRow row;
            row.kind = SessionRowKind::Blank;
            d.displayRows.push_back(std::move(row));
            continue;
        }

        SessionDisplayRow row;
        row.kind = SessionRowKind::Signal;
        row.text = sig->full_name[0] ? sig->full_name : sig->name;
        const int code = m_panel.GtkwaveRadixCodeForSignal(sig);
        row.gtkwaveRadixCode = (code == 0) ? -1 : code;
        d.displayRows.push_back(row);

        SessionTrace tr;
        tr.name = row.text;
        tr.gtkwaveRadixCode = row.gtkwaveRadixCode;
        d.traces.push_back(std::move(tr));
    }

    for (const auto& kv : m_panel.m_signalAliases) {
        if (!kv.first || kv.second.IsEmpty())
            continue;
        SessionAlias alias;
        alias.signalName = kv.first->full_name[0] ? kv.first->full_name : kv.first->name;
        alias.alias = kv.second.ToUTF8().data();
        d.aliases.push_back(std::move(alias));
    }

    for (const auto& mk : m_panel.m_markers) {
        SessionNamedMarker sm;
        sm.timestamp = mk.timestamp;
        sm.label = mk.label.ToUTF8().data();
        d.namedMarkers.push_back(std::move(sm));
    }

    CollectWindowLayout(d);
    m_moduleTree.CollectExpandedPaths(d.treeOpenPaths);
    return d;
}

bool WaveformSessionController::Apply(const WaveformSessionData& session, wxString& err)
{
    if (!session.tracePath.empty()) {
        const wxString tracePath = wxString::FromUTF8(session.tracePath.c_str());
        if (!wxFileName::FileExists(tracePath)) {
            err = "trace file not found: " + tracePath;
            return false;
        }
        if (!m_host.loadTracePath) {
            err = "internal error: trace loader not configured";
            return false;
        }
        m_host.loadTracePath(tracePath);
    }

    m_panel.ClearDisplaySignals();
    m_panel.m_rowComments.clear();
    m_panel.m_signalAliases.clear();
    m_panel.m_markers.clear();
    m_panel.CancelMeasurement();

    int added = 0;
    int missing = 0;
    const std::vector<SessionDisplayRow> rows = WaveformSession::EffectiveDisplayRows(session);
    for (const SessionDisplayRow& row : rows) {
        if (row.kind == SessionRowKind::Blank) {
            m_panel.m_displayedSignals2.push_back(nullptr);
            continue;
        }
        if (row.kind == SessionRowKind::Comment) {
            const int pos = (int)m_panel.m_displayedSignals2.size();
            m_panel.m_displayedSignals2.push_back(nullptr);
            m_panel.m_rowComments[pos] = wxString::FromUTF8(row.text.c_str());
            continue;
        }

        signal_t* sig = nullptr;
        if (m_host.findSignal)
            sig = m_host.findSignal(row.text);
        if (!sig) {
            ++missing;
            continue;
        }
        m_panel.m_displayedSignals2.push_back(sig);
        if (row.gtkwaveRadixCode >= 0) {
            const WaveformRadix::Radix r = WaveformRadix::FromGtkwaveCode(row.gtkwaveRadixCode);
            m_panel.m_signalDataFormats[sig] = WaveformPanel::FromWaveformRadix(r);
        }
        ++added;
    }

    for (const SessionAlias& alias : session.aliases) {
        signal_t* sig = m_host.findSignal ? m_host.findSignal(alias.signalName) : nullptr;
        if (sig)
            m_panel.m_signalAliases[sig] = wxString::FromUTF8(alias.alias.c_str());
    }

    for (const SessionNamedMarker& mk : session.namedMarkers) {
        m_panel.m_markers.push_back({mk.timestamp, wxString::FromUTF8(mk.label.c_str())});
    }

    if (session.hasMeasureMarkers) {
        m_panel.m_hasMarkerA = true;
        m_panel.m_markerA = session.measureMarkerA;
        m_panel.m_markerB = session.measureMarkerB;
        m_panel.m_isMeasuring = true;
    }

    if (session.hasBaseline) {
        m_panel.m_hasBaseline = true;
        m_panel.m_baselineTimestamp = session.baselineTimestamp;
    } else {
        m_panel.m_hasBaseline = false;
        m_panel.m_baselineTimestamp = -1;
    }

    if (session.displayRange > 0)
        m_panel.ApplyVisibleTimeRange(session.timeOffset, session.timeOffset + session.displayRange);
    else if (session.timeOffset > 0)
        m_panel.m_timeOffset = session.timeOffset;

    m_panel.SetCurrentTimestamp(session.playhead, true);
    m_panel.m_showCursorValue = session.showCursorValue;
    if (session.selectedSignalIndex >= 0
        && session.selectedSignalIndex < (int)m_panel.m_displayedSignals2.size()) {
        m_panel.m_selectedSignalIndex = session.selectedSignalIndex;
    } else {
        m_panel.m_selectedSignalIndex = -1;
    }

    m_panel.AssignSignalColors();
    m_panel.RefreshDisplayListFromSession();
    if (m_host.syncTimeRangeUI)
        m_host.syncTimeRangeUI();
    if (m_host.syncAiPanel)
        m_host.syncAiPanel();

    ApplyWindowLayout(session);

    if (!session.treeOpenPaths.empty())
        m_moduleTree.ApplyOpenPaths(session.treeOpenPaths);

    if (added == 0 && !rows.empty())
        err = wxString::Format("session loaded but no trace names matched this dump (%d missing).", missing);
    else if (missing > 0)
        err = wxString::Format("session loaded; %d trace name(s) not found in this dump.", missing);
    return true;
}

bool WaveformSessionController::SaveToPath(const wxString& path, wxString& err)
{
    WaveformSessionData data = Collect();
    if (data.tracePath.empty() && !m_document.TracePath().empty())
        data.tracePath = m_document.TracePath().ToUTF8().data();
    return WaveformSession::Save(path, data, err);
}
