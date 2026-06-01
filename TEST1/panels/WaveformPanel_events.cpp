#include "panels/WaveformPanel.hpp"

#include <wx/log.h>

void WaveformPanel::OnKeyDown(wxKeyEvent& event)
{
    switch (event.GetKeyCode()) {
    case WXK_PAGEUP:
        if (event.ShiftDown()) {
            ScrollSignalRows(-VisibleSignalRowCount());
            return;
        }
        PageLeft();
        return;
    case WXK_PAGEDOWN:
        if (event.ShiftDown()) {
            ScrollSignalRows(VisibleSignalRowCount());
            return;
        }
        PageRight();
        return;
    case WXK_UP:
        ScrollSignalRows(-1);
        if (m_selectedSignalIndex > 0) {
            --m_selectedSignalIndex;
            EnsureSignalRowVisible(m_selectedSignalIndex);
        }
        return;
    case WXK_DOWN:
        ScrollSignalRows(1);
        if (m_selectedSignalIndex + 1 < static_cast<int>(m_displayedSignals2.size())) {
            if (m_selectedSignalIndex < 0)
                m_selectedSignalIndex = 0;
            else
                ++m_selectedSignalIndex;
            EnsureSignalRowVisible(m_selectedSignalIndex);
        }
        return;
    case WXK_HOME:
        if (event.ControlDown()) {
            m_signalScrollRow = 0;
            ClampSignalScroll();
            Refresh(false);
            return;
        }
        break;
    case WXK_END:
        if (event.ControlDown()) {
            m_signalScrollRow = MaxSignalScrollRow();
            ClampSignalScroll();
            Refresh(false);
            return;
        }
        break;
    default:
        break;
    }
    event.Skip();
}

void WaveformPanel::OnMouseWheel(wxMouseEvent& event)
{
    const int x = event.GetX();
    if (event.AltDown() && x >= LEFT_MARGIN && !m_displayedSignals2.empty()) {
        const bool forward = event.GetWheelRotation() < 0;
        const long long cur = GetCursorSimTime();
        long long next = cur;
        if (m_selectedSignalIndex >= 0 && m_selectedSignalIndex < (int)m_displayedSignals2.size()) {
            signal_t* sig = m_displayedSignals2[m_selectedSignalIndex];
            if (sig) {
                for (int r = 0; r < m_patternSearchRepeatCount; ++r) {
                    const long long step = forward
                        ? FindNextEdgeOnSignal(sig, next)
                        : FindPrevEdgeOnSignal(sig, next);
                    if (step == next)
                        break;
                    next = step;
                }
            }
        } else {
            next = forward ? FindNextEdgeWithRepeat(cur) : FindPrevEdgeWithRepeat(cur);
        }
        if (m_maxTimestamp > 0)
            next = std::min(m_maxTimestamp, next);
        next = std::max(0LL, next);
        SetCurrentTimestamp(next, true);
        return;
    }

    const bool wheelScrollSignals =
        m_waveScrollingEnabled || event.ControlDown() || event.GetX() < LEFT_MARGIN;
    if (wheelScrollSignals && !m_displayedSignals2.empty() && VerticalScrollNeeded()) {
        int delta = event.GetWheelRotation();
        if (event.GetWheelDelta() != 0)
            delta /= event.GetWheelDelta();
        if (delta == 0)
            delta = (event.GetWheelRotation() > 0) ? 1 : -1;
        ScrollSignalRows(-delta);
        return;
    }

    if (m_alternateWheelMode) {
        long long currentTime = GetCursorSimTime();
        long long delta = event.GetWheelRotation() > 0 ? -10 : 10;
        long long newTime = currentTime + delta;
        newTime = std::max(0LL, newTime);
        if (m_maxTimestamp > 0) {
            newTime = std::min(m_maxTimestamp, newTime);
        }
        SetCurrentTimestamp(newTime, true);
    } else {
        int mouseX = event.GetX();
        int viewW = GetSize().x - LEFT_MARGIN - WAVE_PADDING;
        if (viewW < 1) viewW = 1;
        double scale = (double)viewW / m_displayTimeRange;
        long long mouseTime = m_timeOffset + (long long)((mouseX - LEFT_MARGIN) / scale);

        m_displayTimeRange *= (event.GetWheelRotation() > 0) ? 0.8 : 1.25;
        if (m_displayTimeRange < 10) m_displayTimeRange = 10;
        if (m_displayTimeRange > m_maxTimestamp) m_displayTimeRange = m_maxTimestamp;

        double newScale = (double)viewW / m_displayTimeRange;
        m_timeOffset = mouseTime - (long long)((mouseX - LEFT_MARGIN) / newScale);
        ClampViewToLimit();
        if (m_timeOffset > m_maxTimestamp - m_displayTimeRange) m_timeOffset = m_maxTimestamp - m_displayTimeRange;
        if (m_timeOffset < 0) m_timeOffset = 0;

        RequestDrawCacheRebuild(true);
        Refresh();
        EmitTimeViewChanged();
    }
}

void WaveformPanel::OnRightDown(wxMouseEvent& event)
{
    const int x = event.GetX();
    const int y = event.GetY();
    if (x < LEFT_MARGIN) {
        const int row = HitTestDisplayedSignalRow(x, y);
        if (row >= 0) {
            m_selectedSignalIndex = row;
            EnsureSignalRowVisible(row);
            Refresh(false);
            ShowTraceContextMenu(event.GetPosition());
        }
        return;
    }
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

void WaveformPanel::OnRightUp(wxMouseEvent& event)
{
    if (!m_isSelecting) return;

    int x1 = m_selectStartX, x2 = m_selectEndX;
    EndMouseInteraction(true);
    if (abs(x2 - x1) < 5) return;
    if (x1 > x2) std::swap(x1, x2);

    int viewW = GetSize().x - LEFT_MARGIN - WAVE_PADDING;
    if (viewW < 1) viewW = 1;
    double scale = (double)viewW / m_displayTimeRange;
    long long t1 = m_timeOffset + (long long)((x1 - LEFT_MARGIN) / scale);
    long long t2 = m_timeOffset + (long long)((x2 - LEFT_MARGIN) / scale);
    if (t2 <= t1) return;

    m_timeOffset = t1;
    m_displayTimeRange = t2 - t1;
    if (m_displayTimeRange < 10) m_displayTimeRange = 10;
    ClampViewToLimit();
    RequestDrawCacheRebuild(true);
    Refresh();
    EmitTimeViewChanged();
}
