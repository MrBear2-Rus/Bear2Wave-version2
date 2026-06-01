#include "panels/WaveformPainter.h"
#include "core/trace_gui_debug.h"
#include "core/ui_theme.h"
#include "panels/WaveformPanel.hpp"
#include "panels/WaveformRenderer.h"
#include "waveform_constants.h"
#include "core/trace_vc.h"
#include "core/trace_blackout.h"
#include "panels/WaveformTextCanvas.h"

#include <wx/dcbuffer.h>
#include <wx/dcmemory.h>
#include <wx/image.h>
#include <wx/settings.h>
#include <wx/textdlg.h>
#include <wx/log.h>

#include <algorithm>
#include <cmath>
#include <climits>
#include <mutex>

namespace {

static void DrawBusSegmentLabelDc(wxDC& dc, const wxRect& rect, const std::string& label, const wxColour& fg)
{
    if (label.empty() || rect.width <= 25)
        return;
    const wxString wxLabel = wxString::FromUTF8(label.c_str());
    const wxSize sz = dc.GetTextExtent(wxLabel);
    const int ty = rect.y + (rect.height - sz.y) / 2;
    dc.SetTextForeground(fg);
    if (sz.x <= rect.width - 8) {
        dc.DrawText(wxLabel, rect.x + (rect.width - sz.x) / 2, ty);
        return;
    }
    wxDCClipper clip(dc, rect);
    dc.DrawText(wxLabel, rect.x + 2, ty);
}

static void DrawBusSegmentLabel(WaveformTextCanvas& canvas, const wxRect& rect, const std::string& label, const wxColour& fg)
{
    if (label.empty() || rect.width <= 25)
        return;
    const wxString wxLabel = wxString::FromUTF8(label.c_str());
    const wxSize sz = canvas.GetTextExtent(wxLabel);
    const int ty = rect.y + (rect.height - sz.y) / 2;
    canvas.SetTextForeground(fg);
    if (sz.x <= rect.width - 8) {
        canvas.DrawTextAt(wxLabel, rect.x + (rect.width - sz.x) / 2, ty);
        return;
    }
    canvas.PushClipRect(rect);
    canvas.DrawTextAt(wxLabel, rect.x + 2, ty);
    canvas.PopClip();
}

} // namespace

#ifdef BEAR2WAVE_RENDER_OPENGL
#include "panels/WaveformGLRenderer.h"

// 离屏白底文字层（Windows 上比透明 bitmap 更稳定，UploadTextOverlay 做白底抠图）
static wxBitmap MakeTextLayerBitmap(int w, int h)
{
    if (w <= 0 || h <= 0 || w > 8192 || h > 8192)
        return wxBitmap(1, 1, 24);
    wxBitmap bmp(w, h, 24);
    wxMemoryDC dc(bmp);
    dc.SetBackground(*wxWHITE_BRUSH);
    dc.Clear();
    dc.SelectObject(wxNullBitmap);
    return bmp;
}

static void CompositeTextLayerGL(WaveformPanel& panel, WaveformGLRenderer& gl, wxSize& size,
                                 int viewW, double scale, int cursorX, int scrollPx,
                                 int firstRow, int lastRow)
{
    if (size.x <= 0 || size.y <= 0)
        return;

    auto paintOverlay = [&](WaveformTextCanvas& canvas) {
        canvas.SetFont(wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT));
        WaveformPainter::PaintTextOverlay(panel, canvas, size, viewW, scale, cursorX, scrollPx, firstRow, lastRow);
    };

    std::vector<std::uint8_t> rgba;
    auto canvas = CreateTextCanvas(size.x, size.y);
    if (!canvas)
        return;
    paintOverlay(*canvas);
    const bool exportOk = canvas->ExportRgba(rgba) && TextRgbaHasVisiblePixels(rgba);

#if defined(_WIN32)
    if (!exportOk && DirectWriteTextEnabled()) {
        canvas = CreateWxTextCanvas(size.x, size.y);
        if (!canvas)
            return;
        paintOverlay(*canvas);
        if (!canvas->ExportRgba(rgba))
            return;
    } else if (!exportOk)
#else
    if (!exportOk)
#endif
        return;

    gl.UploadTextOverlayRgba(canvas->Width(), canvas->Height(), rgba.data());
    gl.DrawTextOverlayQuad();
}
#endif

namespace {

static const UiThemeColors& Th() { return CurrentTheme(); }

static wxColour ThemePlotBg() { return Th().plotBg; }
static wxColour ThemeRowStripe() { return Th().rowStripe; }
static wxColour ThemeAxisBand() { return Th().axisBand; }

static wxString WxFormatTimeLabel(double t)
{
    return wxString::FromUTF8(WaveformRenderer::FormatTimeLabel(t).c_str());
}

// ---- 辅助：从 wxColour 提取 RGBA 分量 ----
static void UnpackColour(const wxColour& c, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b)
{
    r = static_cast<std::uint8_t>(c.Red());
    g = static_cast<std::uint8_t>(c.Green());
    b = static_cast<std::uint8_t>(c.Blue());
}
static void UnpackColourA(const wxColour& c, std::uint8_t& r, std::uint8_t& g, std::uint8_t& b, std::uint8_t& a)
{
    r = static_cast<std::uint8_t>(c.Red());
    g = static_cast<std::uint8_t>(c.Green());
    b = static_cast<std::uint8_t>(c.Blue());
    a = static_cast<std::uint8_t>(c.Alpha());
}

// ---- Measurement bar and Minimap (shared by both Paint and PaintGL) ----

static void DrawMarkerMeasurementBarDc(WaveformPanel& panel, wxDC& dc, wxSize& size, double scale)
{
    if (panel.m_markers.size() < 2) return;
    int yBar = 45, hBar = 22;
    dc.SetBrush(Th().scrollBarBg);
    dc.SetPen(wxPen(wxColour(100, 140, 200)));
    dc.DrawRectangle(LEFT_MARGIN, yBar - hBar / 2, size.x - LEFT_MARGIN - 20, hBar);

    wxFont font(9, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
    dc.SetFont(font);
    dc.SetTextForeground(wxColour(0, 80, 160));

    long long lastT = panel.m_markers[0].timestamp;
    for (size_t i = 1; i < panel.m_markers.size(); i++)
    {
        long long t = panel.m_markers[i].timestamp;
        long long delta = t - lastT;
        int x1 = panel.TimeToX(lastT, scale);
        int x2 = panel.TimeToX(t, scale);
        wxString s = wxString::Format("ΔT = %lld", (long long)delta);
        wxSize sz = dc.GetTextExtent(s);
        int tx = (x1 + x2 - sz.x) / 2;
        dc.DrawText(s, tx, yBar - sz.y / 2);
        lastT = t;
    }
}

static void DrawMarkerMeasurementBar(WaveformPanel& panel, WaveformTextCanvas& canvas, wxSize& size, double scale)
{
    if (panel.m_markers.size() < 2) return;
    int yBar = 45, hBar = 22;
    canvas.SetFillColour(Th().scrollBarBg);
    canvas.SetStrokeColour(wxColour(100, 140, 200));
    canvas.DrawRectangle(LEFT_MARGIN, yBar - hBar / 2, size.x - LEFT_MARGIN - 20, hBar);

    wxFont font(9, wxFONTFAMILY_TELETYPE, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD);
    canvas.SetFont(font);
    canvas.SetTextForeground(wxColour(0, 80, 160));

    long long lastT = panel.m_markers[0].timestamp;
    for (size_t i = 1; i < panel.m_markers.size(); i++)
    {
        long long t = panel.m_markers[i].timestamp;
        long long delta = t - lastT;
        int x1 = panel.TimeToX(lastT, scale);
        int x2 = panel.TimeToX(t, scale);
        wxString s = wxString::Format("ΔT = %lld", (long long)delta);
        wxSize sz = canvas.GetTextExtent(s);
        int tx = (x1 + x2 - sz.x) / 2;
        canvas.DrawTextAt(s, tx, yBar - sz.y / 2);
        lastT = t;
    }
}

#ifdef BEAR2WAVE_RENDER_OPENGL
static void DrawMarkerMeasurementBarGL(WaveformPanel& panel, WaveformGLRenderer& gl, wxSize& size, double scale)
{
    if (panel.m_markers.size() < 2) return;
    int yBar = 45, hBar = 22;
    std::uint8_t r, g, b, a;
    UnpackColourA(Th().scrollBarBg, r, g, b, a);
    gl.AddMeasureBarBg(LEFT_MARGIN, yBar - hBar / 2, size.x - LEFT_MARGIN - 20, hBar, r, g, b, a);
}
#endif

static void DrawMiniMap(WaveformPanel& panel, wxDC& dc, wxSize size)
{
    int w = 200, h = 80;
    int x = size.x - w - 10, y = 10;
    panel.m_minimapRect = wxRect(x, y, w, h);
    dc.SetBrush(wxColour(30, 30, 30));
    dc.SetPen(wxPen(wxColour(100, 100, 100)));
    dc.DrawRectangle(panel.m_minimapRect);
    if (panel.m_allSignals.empty() || panel.m_maxTimestamp <= 0) return;

    const double inv = 1.0 / (double)panel.m_maxTimestamp;
    const double scale = (double)w * inv;
    int maxSignals = std::min(20, (int)panel.m_displayedSignals2.size());
    const int rowH = maxSignals > 0 ? (h / maxSignals) : h;
    for (int i = 0; i < maxSignals; i++)
    {
        signal_t* sig = panel.m_displayedSignals2[i];
        if (!sig || trace_vc_count(sig) < 2)
            continue;
        int yBase = y + 5 + i * rowH;
        const size_t mstep = std::max((size_t)1, trace_vc_count(sig) / 500u);
        for (size_t c = 1; c < trace_vc_count(sig); c += mstep)
        {
            long long t1 = (long long)trace_vc_timestamp(sig, c - 1);
            long long t2 = (long long)trace_vc_timestamp(sig, c);
            int x1 = x + (int)((double)t1 * scale);
            int x2 = x + (int)((double)t2 * scale);
            dc.SetPen(wxPen(wxColour(100, 200, 255), 1));
            dc.DrawLine(x1, yBase, x2, yBase);
        }
    }

    int viewX1 = x + (int)((double)panel.m_timeOffset * inv * (double)w);
    int viewX2 = x + (int)((double)(panel.m_timeOffset + panel.m_displayTimeRange) * inv * (double)w);
    dc.SetBrush(*wxTRANSPARENT_BRUSH);
    dc.SetPen(wxPen(Th().playhead, 2));
    dc.DrawRectangle(viewX1, y, viewX2 - viewX1, h);
}

#ifdef BEAR2WAVE_RENDER_OPENGL
static void DrawMiniMapGL(WaveformPanel& panel, WaveformGLRenderer& gl, wxSize size)
{
    int w = 200, h = 80;
    int x = size.x - w - 10, y = 10;
    panel.m_minimapRect = wxRect(x, y, w, h);

    std::uint8_t r, g, b, a;
    UnpackColourA(wxColour(30, 30, 30), r, g, b, a);
    gl.AddMinimapBg(x, y, w, h, r, g, b, a);
    UnpackColour(wxColour(100, 100, 100), r, g, b);
    gl.AddMinimapBorder(x, y, w, h, r, g, b);

    if (panel.m_allSignals.empty() || panel.m_maxTimestamp <= 0) return;

    const double inv = 1.0 / (double)panel.m_maxTimestamp;
    const double mscale = (double)w * inv;
    int maxSignals = std::min(20, (int)panel.m_displayedSignals2.size());
    const int rowH = maxSignals > 0 ? (h / maxSignals) : h;

    UnpackColour(wxColour(100, 200, 255), r, g, b);
    for (int i = 0; i < maxSignals; i++)
    {
        signal_t* sig = panel.m_displayedSignals2[i];
        if (!sig || trace_vc_count(sig) < 2)
            continue;
        int yBase = y + 5 + i * rowH;
        const size_t mstep = std::max((size_t)1, trace_vc_count(sig) / 500u);
        for (size_t c = 1; c < trace_vc_count(sig); c += mstep)
        {
            long long t1 = (long long)trace_vc_timestamp(sig, c - 1);
            long long t2 = (long long)trace_vc_timestamp(sig, c);
            int x1 = x + (int)((double)t1 * mscale);
            int x2 = x + (int)((double)t2 * mscale);
            gl.AddMinimapSignalLine(x1, yBase, x2, r, g, b);
        }
    }

    int viewX1 = x + (int)((double)panel.m_timeOffset * inv * (double)w);
    int viewX2 = x + (int)((double)(panel.m_timeOffset + panel.m_displayTimeRange) * inv * (double)w);
    UnpackColour(Th().playhead, r, g, b);
    gl.AddMinimapViewRect(viewX1, y, viewX2 - viewX1, h, r, g, b);
}
#endif

} // namespace

namespace WaveformPainter {

// ============================================================================
// 原始 wxDC 渲染路径（保持不变）
// ============================================================================

void Paint(WaveformPanel& panel)
{
    wxAutoBufferedPaintDC dc(&panel);
    wxSize size = panel.GetSize();
    dc.SetBrush(ThemePlotBg());
    dc.SetPen(*wxTRANSPARENT_PEN);
    dc.DrawRectangle(0, 0, size.x, size.y);
    if (panel.m_allSignals.empty())
    {
        dc.SetTextForeground(wxColour(90, 96, 110));
        if (!panel.GetVcd())
            dc.DrawText("Please Start Simulation first", size.x / 2 - 80, size.y / 2);
        else
            dc.DrawText("No signals in trace (check FST/VCD parse / hierarchy)", size.x / 2 - 160, size.y / 2);
        return;
    }

    int viewW = size.x - LEFT_MARGIN - WAVE_PADDING;
    if (viewW < 100) viewW = 100;
    double scale = (double)viewW / panel.m_displayTimeRange;
    const int timeAxisY = 25;
    int cursorX = panel.TimeToX(panel.m_currentTimestamp, scale);
    cursorX = std::max(LEFT_MARGIN, std::min(cursorX, size.x - WAVE_PADDING - 1));
    double targetPixel = 80.0;
    double rawStep = targetPixel / scale;
    double step = panel.NiceStep(rawStep);
    double start = floor((double)panel.m_timeOffset / step) * step;

    /* Time ruler band */
    dc.SetBrush(ThemeAxisBand());
    dc.SetPen(wxPen(wxColour(210, 216, 226), 1));
    dc.DrawRectangle(0, 0, size.x, timeAxisY + 8);
    dc.DrawLine(LEFT_MARGIN, timeAxisY + 8, size.x - WAVE_PADDING, timeAxisY + 8);

    dc.SetFont(wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT));
    int lastTextX = -1000;
    int gi = 0;
    for (double ts = start; ts <= (double)panel.m_timeOffset + (double)panel.m_displayTimeRange; ts += step, ++gi)
    {
        const int x = panel.TimeToX((long long)ts, scale);
        const bool major = (gi % 5 == 0);
        if (major && abs(x - lastTextX) > 56)
        {
            dc.SetTextForeground(wxColour(55, 65, 80));
            dc.DrawText(WxFormatTimeLabel(ts), std::max(LEFT_MARGIN + 2, x - 14), 4);
            lastTextX = x;
        }
        dc.SetPen(major ? wxPen(wxColour(188, 196, 210), 1) : wxPen(wxColour(220, 226, 236), 1, wxPENSTYLE_DOT));
        dc.DrawLine(x, timeAxisY + 8, x, size.y);
    }

    if (panel.m_vcdData && panel.m_vcdData->trace_blackout) {
        const TraceBlackoutStore* blackout =
            static_cast<const TraceBlackoutStore*>(panel.m_vcdData->trace_blackout);
        const size_t span_count = trace_blackout_span_count(blackout);
        const long long vis0 = panel.m_timeOffset;
        const long long vis1 = panel.m_timeOffset + panel.m_displayTimeRange;
        dc.SetBrush(wxColour(180, 180, 180, 80));
        dc.SetPen(*wxTRANSPARENT_PEN);
        for (size_t si = 0; si < span_count; ++si) {
            uint64_t t0 = 0, t1 = 0;
            if (!trace_blackout_span_at(blackout, si, &t0, &t1))
                continue;
            if ((long long)t1 < vis0 || (long long)t0 > vis1)
                continue;
            const int x0 = panel.TimeToX((long long)std::max<uint64_t>(t0, (uint64_t)vis0), scale);
            const int x1 = panel.TimeToX((long long)std::min<uint64_t>(t1, (uint64_t)vis1), scale);
            if (x1 > x0)
                dc.DrawRectangle(x0, timeAxisY + 8, x1 - x0, size.y - timeAxisY - 8);
        }
    }

    /* Hover tick + time (plot area) */
    if (panel.m_hoverPlotX >= LEFT_MARGIN && panel.m_hoverPlotX < size.x - WAVE_PADDING && !panel.m_rulerHoverLabel.empty())
    {
        dc.SetPen(wxPen(wxColour(110, 128, 152), 1));
        dc.DrawLine(panel.m_hoverPlotX, timeAxisY + 2, panel.m_hoverPlotX, timeAxisY + 10);
        dc.SetTextForeground(wxColour(75, 85, 105));
        wxSize tw = dc.GetTextExtent(panel.m_rulerHoverLabel);
        int tx = std::min(panel.m_hoverPlotX + 6, size.x - (int)tw.x - WAVE_PADDING - 6);
        tx = std::max(LEFT_MARGIN + 2, tx);
        dc.DrawText(panel.m_rulerHoverLabel, tx, 4);
    }

    wxRect clip;
    dc.GetClippingBox(&clip.x, &clip.y, &clip.width, &clip.height);
    std::unique_lock<std::mutex> lock(panel.m_cacheMutex);
    int safeCount = std::min((int)panel.m_displayedSignals2.size(), (int)panel.m_cachedSegments.size());
    const int scrollPx = panel.SignalRowScrollOffsetPx();
    const int firstRow = panel.m_signalScrollRow;
    const int lastRow = std::min(safeCount, firstRow + panel.VisibleSignalRowCount());
    const bool cacheOk = panel.m_cacheKeyOffset != LLONG_MIN
        && !panel.m_cachedSegments.empty()
        && panel.m_cacheKeyOffset == panel.m_timeOffset
        && panel.m_cacheKeyRange == panel.m_displayTimeRange
        && panel.m_cacheKeyViewW == viewW
        && panel.m_cacheKeyScrollRow == panel.m_signalScrollRow
        && panel.m_cacheKeyTransformEpoch == panel.m_cacheTransformEpoch;

    if (safeCount == 0) {
        dc.SetTextForeground(wxColour(120, 125, 135));
        dc.DrawText(wxString::FromUTF8(
            "请展开左侧模块树，在信号列表中双击信号即可显示波形"),
            LEFT_MARGIN + 16, panel.SignalPlotTopY() + 36);
    }

    /* 调试：播放头变化时打一行 */
    if (panel.m_showCursorValue && safeCount > 0) {
        static long long s_dbgLastPaintPlayhead = LLONG_MIN;
        if (panel.m_currentTimestamp != s_dbgLastPaintPlayhead) {
            s_dbgLastPaintPlayhead = panel.m_currentTimestamp;
            signal_t* s0 = panel.m_displayedSignals2[0];
            if (s0) {
                lock.unlock();
                const std::string sample = panel.GetValueAt(s0, panel.m_currentTimestamp);
                lock.lock();
                const size_t nc = trace_vc_count(s0);
                timestamp_t tmin = nc ? static_cast<timestamp_t>(trace_vc_timestamp(s0, 0)) : 0,
                            tmax = tmin;
                for (size_t c = 1; c < nc; ++c) {
                    const timestamp_t tv = static_cast<timestamp_t>(trace_vc_timestamp(s0, c));
                    if (tv < tmin) tmin = tv;
                    if (tv > tmax) tmax = tv;
                }
                wxLogDebug(
                    "[Bear2Wave][Paint] ph=%lld cursorX=%d off=%lld range=%lld scale=%.8f | row0=%s "
                    "sample=\"%s\" chg=%zu tmin=%llu tmax=%llu",
                    panel.m_currentTimestamp, cursorX, panel.m_timeOffset,
                    panel.m_displayTimeRange, scale, s0->full_name, sample.c_str(),
                    nc, (unsigned long long)tmin, (unsigned long long)tmax);
            }
        }
    }

    for (int i = firstRow; i < lastRow; i++)
    {
        signal_t* sig = panel.m_displayedSignals2[i];
        const auto commentIt = panel.m_rowComments.find(i);
        const bool isCommentRow = (commentIt != panel.m_rowComments.end());

        int yBase = panel.RowYBase(i);
        int height = 15 + (panel.m_analogHeightExtension * 15 / 100);
        int yH = yBase - height, yL = yBase + height;

        if (i & 1) {
            dc.SetBrush(ThemeRowStripe());
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.DrawRectangle(0, yBase - 28, size.x, SIGNAL_ROW_HEIGHT - 2);
        }

        if (isCommentRow) {
            dc.SetBrush(Th().selectedRow);
            dc.SetPen(wxPen(wxColour(200, 180, 100), 1));
            dc.DrawRectangle(0, yBase - 28, size.x, SIGNAL_ROW_HEIGHT - 2);
            if (i == panel.m_selectedSignalIndex) {
                dc.SetPen(wxPen(wxColour(0, 120, 255), 2));
                dc.DrawRectangle(0, yBase - 20, LEFT_MARGIN, SIGNAL_ROW_HEIGHT);
            }
            wxFont f = dc.GetFont();
            wxFont italic = f.Italic();
            dc.SetFont(italic);
            dc.SetTextForeground(wxColour(100, 75, 10));
            dc.DrawText("// " + commentIt->second, 10, yBase - 8);
            dc.SetFont(f);
            dc.SetPen(wxPen(wxColour(220, 210, 170), 1, wxPENSTYLE_DOT));
            dc.DrawLine(LEFT_MARGIN, yBase, size.x - WAVE_PADDING, yBase);
            continue;
        }

        if (!sig)
            continue;

        if (panel.IsSyntheticTransactionSignal(sig)) {
            wxColour bg(245, 240, 255);
            auto colorIt = panel.m_transactionRowColors.find(sig);
            if (colorIt != panel.m_transactionRowColors.end())
                bg = WaveformPanel::TransactionRowColorFromName(colorIt->second);
            dc.SetBrush(bg);
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.DrawRectangle(0, yBase - 28, size.x, SIGNAL_ROW_HEIGHT - 2);
        }

        if (i == panel.m_selectedSignalIndex)
        {
            dc.SetBrush(wxBrush(wxColour(220, 240, 255)));
            dc.SetPen(wxPen(wxColour(0, 120, 255), 2));
            dc.DrawRectangle(0, yBase - 20, LEFT_MARGIN, SIGNAL_ROW_HEIGHT);
        }

        wxString displayName = panel.TraceRowDisplayName(sig);

        bool isMatch = !panel.m_searchKeyword.empty() && panel.m_searchMatchedSignals.count(sig->signal_id);
        dc.SetTextForeground(isMatch ? wxColour(255, 60, 60) : Th().signalNameText);
        dc.DrawText(displayName, 10, yBase - 8);

        auto& segments = panel.m_cachedSegments[i];
        const int tf = [&]() {
            auto it = panel.m_signalTransforms.find(sig);
            return it != panel.m_signalTransforms.end() ? it->second : 0;
        }();
        if (tf & WaveformRenderer::TR_RANGE_FILL) {
            dc.SetBrush(Th().commentRow);
            dc.SetPen(*wxTRANSPARENT_PEN);
            dc.DrawRectangle(LEFT_MARGIN, yBase - 15, size.x - LEFT_MARGIN - WAVE_PADDING, 30);
        }
        const WaveformPanel::WaveTraceKind rowKind = segments.empty() ? WaveformPanel::ClassifyTraceKind(sig) : segments[0].traceKind;
        if (cacheOk) {
        for (size_t k = 0; k < segments.size(); k++)
        {
            auto& seg = segments[k];
            int drawX1 = std::max(seg.x1, clip.x);
            int drawX2 = std::min(seg.x2, clip.x + clip.width);
            if (drawX2 <= drawX1) continue;

            bool isHover = (i == panel.m_hoverSignal && k == panel.m_hoverSegment);
            if (rowKind == WaveformPanel::WaveTraceKind::BusBits
                || rowKind == WaveformPanel::WaveTraceKind::TextString
                || rowKind == WaveformPanel::WaveTraceKind::TransactionEvent)
            {
                const int segY = seg.y - scrollPx;
                wxRect rect(drawX1, segY - 9, drawX2 - drawX1, 18);
                const bool isTx = (rowKind == WaveformPanel::WaveTraceKind::TransactionEvent);
                const bool isText = (rowKind == WaveformPanel::WaveTraceKind::TextString);
                dc.SetBrush(isHover ? wxColour(255, 220, 150)
                                    : (isTx ? wxColour(240, 210, 255)
                                            : (isText ? wxColour(235, 225, 200) : wxColour(200, 230, 255))));
                dc.SetPen(isHover ? wxPen(wxColour(255, 140, 0), 2)
                                  : wxPen(isTx ? wxColour(140, 60, 180) : wxColour(0, 100, 200), 1));
                dc.DrawRectangle(rect);
                if (!seg.text.empty() && rect.width > 25)
                {
                    const std::string label = panel.BusSegmentDisplayText(sig, seg, scale);
                    const wxColour fg = isTx ? wxColour(90, 20, 120) : wxColour(0, 80, 0);
                    DrawBusSegmentLabelDc(dc, rect, label, fg);
                }
                continue;
            }

            if (rowKind == WaveformPanel::WaveTraceKind::RealAnalog)
            {
                wxColour color = panel.TraceColourForSignal(sig, isMatch);
                if (!panel.m_searchKeyword.empty() && isMatch)
                    color = wxColour(255, 60, 60);
                else if (!panel.m_searchKeyword.empty())
                    color = Th().signalNameDim;
                else if (panel.m_signalColors.find(sig->signal_id) == panel.m_signalColors.end())
                    color = wxColour(0, 120, 200);
                dc.SetPen(isHover ? wxPen(Th().hoverHighlight, 2) : wxPen(color, 2));
                const int segY = seg.y - scrollPx;
                if (seg.yEnd >= 0) {
                    const int segY2 = seg.yEnd - scrollPx;
                    dc.DrawLine(drawX1, segY, drawX2, segY2);
                } else {
                    dc.DrawLine(drawX1, segY, drawX2, segY);
                    if (k > 0) {
                        auto& prev = segments[k - 1];
                        const int prevY = prev.yEnd >= 0 ? prev.yEnd - scrollPx : prev.y - scrollPx;
                        if (prevY != segY)
                            dc.DrawLine(drawX1, prevY, drawX1, segY);
                    }
                }
                if (isHover && !seg.text.empty())
                    dc.DrawText(wxString::Format("Value: %s", seg.text.c_str()), seg.x1, segY - 20);
                continue;
            }

            int yCurr = (seg.value == '0') ? yL : yH;
            wxColour color = panel.TraceColourForSignal(sig, isMatch);

            dc.SetPen(isHover ? wxPen(Th().hoverHighlight, 2) : wxPen(color, 2));
            dc.DrawLine(drawX1, yCurr, drawX2, yCurr);
            if (k > 0)
            {
                auto& prev = segments[k - 1];
                int yPrev = (prev.value == '0') ? yL : yH;
                if (prev.value != seg.value) dc.DrawLine(drawX1, yPrev, drawX1, yCurr);
            }
            if (isHover) dc.DrawText(wxString::Format("Value: %c", seg.value), seg.x1, yCurr - 20);
        }
        }

        if (panel.m_showCursorValue || panel.m_measureMode != WaveformPanel::MEASURE_NONE) {
            lock.unlock();
            if (panel.m_showCursorValue) {
                const std::string val = panel.GetValueAt(sig, panel.m_currentTimestamp);
                if (!val.empty()) {
                    wxString text = wxString::Format("%s = %s", sig->full_name, val.c_str());
                    int tx = cursorX + 5;
                    int ty = yBase - 8;
                    if (tx > size.x - 150) tx = cursorX - 140;
                    wxSize sz = dc.GetTextExtent(text);
                    dc.SetBrush(Th().cursorValueBg);
                    dc.SetPen(wxPen(wxColour(200, 200, 100)));
                    dc.DrawRectangle(tx - 2, ty - 1, sz.x + 4, sz.y + 2);
                    dc.SetTextForeground(*wxBLACK);
                    dc.DrawText(text, tx, ty);
                }
            }
            if (panel.m_measureMode != WaveformPanel::MEASURE_NONE) {
                double val = 0;
                wxString text;
                if (panel.m_measureMode == WaveformPanel::MEASURE_FREQ) {
                    val = panel.ComputeFrequency(sig);
                    text = wxString::Format("F=%.3f", val);
                } else if (panel.m_measureMode == WaveformPanel::MEASURE_DUTY) {
                    val = panel.ComputeDuty(sig);
                    text = wxString::Format("D=%.2f%%", val * 100);
                }
                dc.SetTextForeground(wxColour(120, 0, 120));
                dc.DrawText(text, size.x - 120, yBase - 10);
            }
            lock.lock();
        }
    }

    if (cacheOk && panel.m_hoverSignal >= 0 && panel.m_hoverSegment >= 0
        && panel.m_hoverSignal < (int)panel.m_cachedSegments.size()
        && panel.m_hoverSegment < (int)panel.m_cachedSegments[panel.m_hoverSignal].size())
    {
        auto& seg = panel.m_cachedSegments[panel.m_hoverSignal][panel.m_hoverSegment];
        if (!seg.text.empty())
        {
            wxString info = "Value: " + seg.text;
            int mx, my;
            wxGetMousePosition(&mx, &my);
            panel.ScreenToClient(&mx, &my);
            wxSize sz = dc.GetTextExtent(info);
            dc.SetBrush(Th().cursorValueBg);
            dc.SetPen(wxPen(wxColour(200, 200, 100)));
            dc.DrawRectangle(mx + 10, my + 10, sz.x + 10, sz.y + 6);
            dc.SetTextForeground(*wxBLACK);
            dc.DrawText(info, mx + 15, my + 13);
        }
    }

    for (size_t i = 0; i < panel.m_markers.size(); i++)
    {
        auto& mk = panel.m_markers[i];
        int x = panel.TimeToX(mk.timestamp, scale);
        bool isHover = ((int)i == panel.m_hoverMarkerIndex);
        bool isDrag = ((int)i == panel.m_draggingMarkerIndex);

        dc.SetPen(isDrag ? wxPen(Th().measureLine, 2) :
            isHover ? wxPen(Th().hoverHighlight, 2) :
            wxPen(wxColour(0, 180, 0), 2));
        dc.DrawLine(x, 0, x, size.y);

        wxSize sz = dc.GetTextExtent(mk.label);
        wxColour bg = isDrag ? wxColour(255, 220, 220) :
            isHover ? wxColour(255, 240, 200) :
            wxColour(200, 255, 200);
        wxColour bd = isDrag ? wxColour(200, 0, 0) :
            isHover ? Th().hoverHighlight :
            wxColour(0, 120, 0);
        dc.SetBrush(bg);
        dc.SetPen(bd);
        dc.DrawRectangle(x + 3, 5, sz.x + 6, sz.y + 4);
        dc.DrawText(mk.label, x + 6, 7);
        if (isDrag) dc.DrawText(wxString::Format("T=%lld", mk.timestamp), x + 5, sz.y + 15);
    }

    for (long long pt : panel.m_patternMarkTimes)
    {
        const int x = panel.TimeToX(pt, scale);
        dc.SetPen(wxPen(wxColour(160, 70, 210), 1, wxPENSTYLE_DOT));
        dc.DrawLine(x, 0, x, size.y);
    }

    if (panel.m_hasMarkerA && panel.m_markerA >= 0 && panel.m_markerB >= 0)
    {
        int xA = panel.TimeToX(panel.m_markerA, scale);
        int xB = panel.TimeToX(panel.m_markerB, scale);
        dc.SetPen(wxPen(Th().measureLine, 2));
        dc.DrawLine(xA, 0, xA, size.y);
        dc.DrawLine(xB, 0, xB, size.y);
        wxString text = wxString::Format("ΔT = %lld", (long long)(panel.m_markerB - panel.m_markerA));
        dc.SetTextForeground(wxColour(200, 0, 0));
        dc.DrawText(text, (xA + xB) / 2, 10);
    }

    if (panel.m_isSelecting)
    {
        int x1 = panel.m_selectStartX, x2 = panel.m_selectEndX;
        if (x1 > x2) std::swap(x1, x2);
        dc.SetBrush(wxColour(100, 150, 255, 60));
        dc.SetPen(wxPen(wxColour(50, 100, 200), 1));
        dc.DrawRectangle(x1, 0, x2 - x1, size.y);
    }

    DrawMarkerMeasurementBarDc(panel, dc, size, scale);
    if (panel.m_isEditingMarker && panel.m_editingMarkerIndex >= 0)
    {
        wxTextEntryDialog dlg(&panel, "Edit label:", "Marker Name", panel.m_editingMarkerText);
        if (dlg.ShowModal() == wxID_OK) panel.m_markers[panel.m_editingMarkerIndex].label = dlg.GetValue();
        panel.m_isEditingMarker = false;
        panel.m_editingMarkerIndex = -1;
    }

    if (panel.m_ghostMarkerActive && panel.m_ghostMarkerTimestamp >= 0) {
        const int gx = panel.TimeToX(panel.m_ghostMarkerTimestamp, scale);
        dc.SetPen(wxPen(wxColour(160, 160, 170), 1, wxPENSTYLE_DOT));
        dc.DrawLine(gx, timeAxisY + 8, gx, size.y);
    }

    if (panel.m_hasBaseline && panel.m_baselineTimestamp >= 0) {
        const int bx = panel.TimeToX(panel.m_baselineTimestamp, scale);
        dc.SetPen(wxPen(wxColour(90, 90, 110), 2, wxPENSTYLE_LONG_DASH));
        dc.DrawLine(bx, timeAxisY + 8, bx, size.y);
        const long long delta = panel.m_currentTimestamp - panel.m_baselineTimestamp;
        wxString s = wxString::Format("Baseline ΔT = %lld", (long long)delta);
        dc.SetTextForeground(wxColour(70, 70, 90));
        dc.DrawText(s, std::min(bx + 4, size.x - 160), 30);
    }

    dc.SetPen(wxPen(wxColour(66, 133, 244), 2, wxPENSTYLE_SHORT_DASH));
    dc.DrawLine(cursorX, timeAxisY + 8, cursorX, size.y);

    if (panel.VerticalScrollNeeded()) {
        const wxRect track = panel.SignalScrollTrackRect();
        const int thumbH = panel.SignalScrollThumbHeight();
        const int thumbY = panel.SignalScrollThumbY();
        dc.SetBrush(wxColour(228, 232, 240));
        dc.SetPen(wxPen(wxColour(190, 198, 210), 1));
        dc.DrawRectangle(track);
        dc.SetBrush(panel.m_scrollBarDragging ? wxColour(100, 120, 150) : wxColour(140, 155, 175));
        dc.DrawRectangle(track.x + 1, thumbY, track.width - 2, thumbH);
    }

    DrawMiniMap(panel, dc, size);
}

// ============================================================================
// OpenGL 渲染路径
// ============================================================================

#ifdef BEAR2WAVE_RENDER_OPENGL

void PaintGL(WaveformPanel& panel, WaveformGLRenderer& gl)
{
    // 满足 wxEVT_PAINT；文字不再直接画到窗口 DC（避免 SwapBuffers 后闪烁）
    wxPaintDC paintDC(&panel);
    (void)paintDC;
    wxSize size = panel.GetSize();
    if (size.x <= 0 || size.y <= 0)
        return;
    if (trace_fst_debug_enabled()) {
        static vcd_t* s_glVcd = nullptr;
        static int s_glPaintN = 0;
        if (panel.m_vcdData != s_glVcd) {
            s_glVcd = panel.m_vcdData;
            s_glPaintN = 0;
        }
        if (s_glPaintN < 8) {
            trace_fst_log("PAINT_GL", "n=%d size=%dx%d disp=%zu",
                s_glPaintN, size.x, size.y, panel.m_displayedSignals2.size());
            ++s_glPaintN;
        }
    }

    // 2. 开始 OpenGL 帧
    gl.BeginFrame(size.x, size.y);

    if (panel.m_allSignals.empty())
    {
        auto drawEmptyHint = [](WaveformTextCanvas& canvas, WaveformPanel& p, wxSize& sz) {
            canvas.SetTextForeground(wxColour(90, 96, 110));
            if (!p.GetVcd())
                canvas.DrawTextAt("Please Start Simulation first", sz.x / 2 - 80, sz.y / 2);
            else
                canvas.DrawTextAt("No signals in trace (check FST/VCD parse / hierarchy)", sz.x / 2 - 160, sz.y / 2);
        };

        std::vector<std::uint8_t> rgba;
        auto canvas = CreateTextCanvas(size.x, size.y);
        if (canvas) {
            drawEmptyHint(*canvas, panel, size);
            bool ok = canvas->ExportRgba(rgba) && TextRgbaHasVisiblePixels(rgba);
#if defined(_WIN32)
            if (!ok && DirectWriteTextEnabled()) {
                canvas = CreateWxTextCanvas(size.x, size.y);
                if (canvas) {
                    drawEmptyHint(*canvas, panel, size);
                    ok = canvas->ExportRgba(rgba);
                }
            }
#endif
            if (ok) {
                gl.EndFrame();
                gl.UploadTextOverlayRgba(canvas->Width(), canvas->Height(), rgba.data());
                gl.DrawTextOverlayQuad();
                panel.SwapBuffers();
                return;
            }
        }
        gl.EndFrame();
        panel.SwapBuffers();
        return;
    }

    int viewW = size.x - LEFT_MARGIN - WAVE_PADDING;
    if (viewW < 100) viewW = 100;
    wxBitmap measureBmp(1, 1);
    wxMemoryDC measureDC(measureBmp);
    measureDC.SetFont(wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT));

    double scale = (double)viewW / panel.m_displayTimeRange;
    const int timeAxisY = 25;
    int cursorX = panel.TimeToX(panel.m_currentTimestamp, scale);
    cursorX = std::max(LEFT_MARGIN, std::min(cursorX, size.x - WAVE_PADDING - 1));
    double targetPixel = 80.0;
    double rawStep = targetPixel / scale;
    double step = panel.NiceStep(rawStep);
    double start = floor((double)panel.m_timeOffset / step) * step;

    // ---- 背景 ----
    std::uint8_t r, g, b, a;
    UnpackColourA(ThemePlotBg(), r, g, b, a);
    // (背景色已在 BeginFrame 用 glClearColor 设置，无需额外绘制)

    // ---- Time axis band ----
    UnpackColourA(ThemeAxisBand(), r, g, b, a);
    gl.AddAxisBand(0, 0, size.x, timeAxisY + 8, r, g, b, a);

    // ---- 网格竖线 ----
    int lastTextX = -1000;
    int gi = 0;
    for (double ts = start; ts <= (double)panel.m_timeOffset + (double)panel.m_displayTimeRange; ts += step, ++gi)
    {
        const int x = panel.TimeToX((long long)ts, scale);
        const bool major = (gi % 5 == 0);
        std::uint8_t gr, gg, gb;
        if (major) {
            UnpackColour(wxColour(188, 196, 210), gr, gg, gb);
        } else {
            UnpackColour(wxColour(220, 226, 236), gr, gg, gb);
        }
        gl.AddGridLine(x, timeAxisY + 8, size.y, gr, gg, gb);
    }

    // ---- Blackout overlay ----
    if (panel.m_vcdData && panel.m_vcdData->trace_blackout) {
        const TraceBlackoutStore* blackout =
            static_cast<const TraceBlackoutStore*>(panel.m_vcdData->trace_blackout);
        const size_t span_count = trace_blackout_span_count(blackout);
        const long long vis0 = panel.m_timeOffset;
        const long long vis1 = panel.m_timeOffset + panel.m_displayTimeRange;
        for (size_t si = 0; si < span_count; ++si) {
            uint64_t t0 = 0, t1 = 0;
            if (!trace_blackout_span_at(blackout, si, &t0, &t1))
                continue;
            if ((long long)t1 < vis0 || (long long)t0 > vis1)
                continue;
            const int x0 = panel.TimeToX((long long)std::max<uint64_t>(t0, (uint64_t)vis0), scale);
            const int x1 = panel.TimeToX((long long)std::min<uint64_t>(t1, (uint64_t)vis1), scale);
            if (x1 > x0)
                gl.AddBlackoutRect(x0, timeAxisY + 8, x1 - x0, size.y - timeAxisY - 8, 180, 180, 180, 80);
        }
    }

    // ---- Hover tick (plot area) ----
    if (panel.m_hoverPlotX >= LEFT_MARGIN && panel.m_hoverPlotX < size.x - WAVE_PADDING && !panel.m_rulerHoverLabel.empty())
    {
        gl.AddGridLine(panel.m_hoverPlotX, timeAxisY + 2, timeAxisY + 10, 110, 128, 152);
    }

    // ---- 信号行循环 ----
    int safeCount = 0;
    int scrollPx = 0;
    int firstRow = 0;
    int lastRow = 0;
    {
        std::lock_guard<std::mutex> lock(panel.m_cacheMutex);
        safeCount = std::min((int)panel.m_displayedSignals2.size(), (int)panel.m_cachedSegments.size());
        scrollPx = panel.SignalRowScrollOffsetPx();
        firstRow = panel.m_signalScrollRow;
        lastRow = std::min(safeCount, firstRow + panel.VisibleSignalRowCount());
        const bool cacheOk = panel.m_cacheKeyOffset != LLONG_MIN
            && !panel.m_cachedSegments.empty()
            && panel.m_cacheKeyOffset == panel.m_timeOffset
            && panel.m_cacheKeyRange == panel.m_displayTimeRange
            && panel.m_cacheKeyViewW == viewW
            && panel.m_cacheKeyScrollRow == panel.m_signalScrollRow
            && panel.m_cacheKeyTransformEpoch == panel.m_cacheTransformEpoch;

        for (int i = firstRow; i < lastRow; i++)
        {
            signal_t* sig = panel.m_displayedSignals2[i];
        const auto commentIt = panel.m_rowComments.find(i);
        const bool isCommentRow = (commentIt != panel.m_rowComments.end());

        int yBase = panel.RowYBase(i);
        int height = 15 + (panel.m_analogHeightExtension * 15 / 100);
        int yH = yBase - height, yL = yBase + height;

        // 行条纹
        if (i & 1) {
            UnpackColour(ThemeRowStripe(), r, g, b);
            gl.AddRowStripe(0, yBase - 28, size.x, SIGNAL_ROW_HEIGHT - 2, r, g, b);
        }

        // Comment 行
        if (isCommentRow) {
            gl.AddCommentRowBg(0, yBase - 28, size.x, SIGNAL_ROW_HEIGHT - 2, 255, 252, 220);
            if (i == panel.m_selectedSignalIndex) {
                gl.AddSelectedHighlight(0, yBase - 20, LEFT_MARGIN, SIGNAL_ROW_HEIGHT, 220, 240, 255);
            }
            gl.AddGridLine(LEFT_MARGIN, yBase, size.x - WAVE_PADDING, yBase, 220, 210, 170);
            continue;
        }

        if (!sig)
            continue;

        if (panel.IsSyntheticTransactionSignal(sig)) {
            gl.AddCommentRowBg(0, yBase - 28, size.x, SIGNAL_ROW_HEIGHT - 2, 245, 240, 255);
        }

        // 选中高亮
        if (i == panel.m_selectedSignalIndex) {
            gl.AddSelectedHighlight(0, yBase - 20, LEFT_MARGIN, SIGNAL_ROW_HEIGHT, 220, 240, 255);
        }

        bool isMatch = !panel.m_searchKeyword.empty() && panel.m_searchMatchedSignals.count(sig->signal_id);

        // Range-fill
        const int tf = [&]() {
            auto it = panel.m_signalTransforms.find(sig);
            return it != panel.m_signalTransforms.end() ? it->second : 0;
        }();
        if (tf & WaveformRenderer::TR_RANGE_FILL) {
            gl.AddRangeFill(LEFT_MARGIN, yBase - 15, size.x - LEFT_MARGIN - WAVE_PADDING, 30, 235, 245, 255);
        }

        auto& segments = panel.m_cachedSegments[i];
        const WaveformPanel::WaveTraceKind rowKind = segments.empty() ? WaveformPanel::ClassifyTraceKind(sig) : segments[0].traceKind;

        wxColour baseColor = panel.TraceColourForSignal(sig, isMatch);
        if (!panel.m_searchKeyword.empty() && isMatch)
            baseColor = wxColour(255, 60, 60);
        else if (!panel.m_searchKeyword.empty())
            baseColor = wxColour(180, 180, 180);
        else if (panel.m_signalColors.find(sig->signal_id) == panel.m_signalColors.end())
            baseColor = wxColour(44, 62, 80);

        if (cacheOk) {
        for (size_t k = 0; k < segments.size(); k++)
        {
            auto& seg = segments[k];
            bool isHover = (i == panel.m_hoverSignal && k == panel.m_hoverSegment);
            wxColour segColor = isHover ? wxColour(255, 140, 0) : baseColor;

            if (rowKind == WaveformPanel::WaveTraceKind::BusBits
                || rowKind == WaveformPanel::WaveTraceKind::TextString
                || rowKind == WaveformPanel::WaveTraceKind::TransactionEvent)
            {
                const int segY = seg.y - scrollPx;
                const bool isTx = (rowKind == WaveformPanel::WaveTraceKind::TransactionEvent);
                const bool isText = (rowKind == WaveformPanel::WaveTraceKind::TextString);

                wxColour bgc = isHover ? wxColour(255, 220, 150)
                                       : (isTx ? wxColour(240, 210, 255)
                                               : (isText ? wxColour(235, 225, 200) : wxColour(200, 230, 255)));
                wxColour penc = isHover ? wxColour(255, 140, 0)
                                        : (isTx ? wxColour(140, 60, 180) : wxColour(0, 100, 200));

                std::uint8_t bgr, bgg, bgb, bga;
                UnpackColourA(bgc, bgr, bgg, bgb, bga);
                gl.AddBusRect(seg.x1, segY - 9, seg.x2 - seg.x1, 18, bgr, bgg, bgb, bga);
                continue;
            }

            if (rowKind == WaveformPanel::WaveTraceKind::RealAnalog)
            {
                const int segY = seg.y - scrollPx;
                UnpackColour(segColor, r, g, b);
                if (seg.yEnd >= 0) {
                    const int segY2 = seg.yEnd - scrollPx;
                    gl.AddAnalogLine(seg.x1, segY, seg.x2, segY2, r, g, b);
                } else {
                    gl.AddAnalogLine(seg.x1, segY, seg.x2, segY, r, g, b);
                    if (k > 0) {
                        auto& prev = segments[k - 1];
                        const int prevY = prev.yEnd >= 0 ? prev.yEnd - scrollPx : prev.y - scrollPx;
                        if (prevY != segY)
                            gl.AddAnalogLine(seg.x1, prevY, seg.x1, segY, r, g, b);
                    }
                }
                continue;
            }

            // DigitalScalar
            int yCurr = (seg.value == '0') ? yL : yH;
            UnpackColour(segColor, r, g, b);
            gl.AddDigitalLine(seg.x1, yCurr, seg.x2, yCurr, r, g, b);
            if (k > 0)
            {
                auto& prev = segments[k - 1];
                int yPrev = (prev.value == '0') ? yL : yH;
                if (prev.value != seg.value)
                    gl.AddDigitalTransition(seg.x1, yPrev, yCurr, r, g, b);
            }
        }
        }

        }

        // ---- Hover tooltip 背景 ----
        if (cacheOk && panel.m_hoverSignal >= 0 && panel.m_hoverSegment >= 0
            && panel.m_hoverSignal < (int)panel.m_cachedSegments.size()
            && panel.m_hoverSegment < (int)panel.m_cachedSegments[panel.m_hoverSignal].size())
        {
            auto& seg = panel.m_cachedSegments[panel.m_hoverSignal][panel.m_hoverSegment];
            if (!seg.text.empty())
            {
                wxString info = "Value: " + seg.text;
                int mx, my;
                wxGetMousePosition(&mx, &my);
                panel.ScreenToClient(&mx, &my);
                wxSize sz = measureDC.GetTextExtent(info);
                gl.AddCursorLabelBg(mx + 10, my + 10, sz.x + 10, sz.y + 6,
                    Th().markerLabelBg.Red(), Th().markerLabelBg.Green(), Th().markerLabelBg.Blue());
            }
        }
    }

    // ---- Marker 线 + label 背景 ----
    for (size_t i = 0; i < panel.m_markers.size(); i++)
    {
        auto& mk = panel.m_markers[i];
        int x = panel.TimeToX(mk.timestamp, scale);
        bool isHover = ((int)i == panel.m_hoverMarkerIndex);
        bool isDrag = ((int)i == panel.m_draggingMarkerIndex);

        wxColour lc = isDrag ? wxColour(255, 0, 0)
                     : isHover ? wxColour(255, 140, 0)
                     : wxColour(0, 180, 0);
        UnpackColour(lc, r, g, b);
        gl.AddMarkerLine(x, 0, size.y, r, g, b);

        wxSize sz = measureDC.GetTextExtent(mk.label);
        wxColour bg = isDrag ? wxColour(255, 220, 220)
                     : isHover ? wxColour(255, 240, 200)
                     : wxColour(200, 255, 200);
        UnpackColourA(bg, r, g, b, a);
        gl.AddMarkerLabelBg(x + 3, 5, sz.x + 6, sz.y + 4, r, g, b, a);
    }

    // ---- A/B 测量线 ----
    if (panel.m_hasMarkerA && panel.m_markerA >= 0 && panel.m_markerB >= 0)
    {
        int xA = panel.TimeToX(panel.m_markerA, scale);
        int xB = panel.TimeToX(panel.m_markerB, scale);
        gl.AddMarkerLine(xA, 0, size.y, 255, 0, 0);
        gl.AddMarkerLine(xB, 0, size.y, 255, 0, 0);
    }

    // ---- Selection overlay ----
    if (panel.m_isSelecting)
    {
        int x1 = panel.m_selectStartX, x2 = panel.m_selectEndX;
        if (x1 > x2) std::swap(x1, x2);
        gl.AddSelectionOverlay(x1, 0, x2 - x1, size.y, 100, 150, 255, 60);
    }

    // ---- Measurement bar ----
    DrawMarkerMeasurementBarGL(panel, gl, size, scale);

    // ---- Playhead ----
    if (panel.m_ghostMarkerActive && panel.m_ghostMarkerTimestamp >= 0) {
        const int gx = panel.TimeToX(panel.m_ghostMarkerTimestamp, scale);
        gl.AddMarkerLine(gx, timeAxisY + 8, size.y, 160, 160, 170);
    }
    if (panel.m_hasBaseline && panel.m_baselineTimestamp >= 0) {
        const int bx = panel.TimeToX(panel.m_baselineTimestamp, scale);
        gl.AddMarkerLine(bx, timeAxisY + 8, size.y, 90, 90, 110);
    }
    gl.AddPlayheadLine(cursorX, timeAxisY + 8, size.y, 66, 133, 244);

    // ---- Scrollbar ----
    if (panel.VerticalScrollNeeded()) {
        const wxRect track = panel.SignalScrollTrackRect();
        const int thumbH = panel.SignalScrollThumbHeight();
        const int thumbY = panel.SignalScrollThumbY();
        gl.AddScrollBarTrack(track.x, track.y, track.width, track.height, 228, 232, 240);
        std::uint8_t sr, sg, sb;
        if (panel.m_scrollBarDragging)
            UnpackColour(wxColour(100, 120, 150), sr, sg, sb);
        else
            UnpackColour(wxColour(140, 155, 175), sr, sg, sb);
        gl.AddScrollBarThumb(track.x + 1, thumbY, track.width - 2, thumbH, sr, sg, sb);
    }

    // ---- Minimap ----
    DrawMiniMapGL(panel, gl, size);

    // 3. 几何提交 + 文字纹理合成，单次 SwapBuffers（消除闪烁）
    gl.EndFrame();
    CompositeTextLayerGL(panel, gl, size, viewW, scale, cursorX, scrollPx, firstRow, lastRow);
    panel.SwapBuffers();

    if (panel.m_isEditingMarker && panel.m_editingMarkerIndex >= 0)
    {
        wxTextEntryDialog dlg(&panel, "Edit label:", "Marker Name", panel.m_editingMarkerText);
        if (dlg.ShowModal() == wxID_OK) panel.m_markers[panel.m_editingMarkerIndex].label = dlg.GetValue();
        panel.m_isEditingMarker = false;
        panel.m_editingMarkerIndex = -1;
    }
}

// ============================================================================
// 纯文本叠加层（被 Paint 和 PaintGL 共用，用于不依赖几何的文本绘制）
// ============================================================================

void PaintTextOverlay(WaveformPanel& panel, WaveformTextCanvas& canvas, wxSize& size,
                      int viewW, double scale, int cursorX, int scrollPx,
                      int firstRow, int lastRow)
{
    std::unique_lock<std::mutex> lock(panel.m_cacheMutex);
    int safeCount = std::min((int)panel.m_displayedSignals2.size(), (int)panel.m_cachedSegments.size());
    const bool cacheOk = panel.m_cacheKeyOffset != LLONG_MIN
        && !panel.m_cachedSegments.empty()
        && panel.m_cacheKeyOffset == panel.m_timeOffset
        && panel.m_cacheKeyRange == panel.m_displayTimeRange
        && panel.m_cacheKeyViewW == viewW
        && panel.m_cacheKeyScrollRow == panel.m_signalScrollRow
        && panel.m_cacheKeyTransformEpoch == panel.m_cacheTransformEpoch;

    if (safeCount == 0) {
        canvas.SetTextForeground(wxColour(120, 125, 135));
        canvas.DrawTextAt(wxString::FromUTF8(
            "请展开左侧模块树，在信号列表中双击信号即可显示波形"),
            LEFT_MARGIN + 16, panel.SignalPlotTopY() + 36);
    }

    const int timeAxisY = 25;

    canvas.SetFont(wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT));
    double targetPixel = 80.0;
    double rawStep = targetPixel / scale;
    double step = panel.NiceStep(rawStep);
    double start = floor((double)panel.m_timeOffset / step) * step;
    int lastTextX = -1000;
    int gi = 0;
    for (double ts = start; ts <= (double)panel.m_timeOffset + (double)panel.m_displayTimeRange; ts += step, ++gi)
    {
        const int x = panel.TimeToX((long long)ts, scale);
        const bool major = (gi % 5 == 0);
        if (major && abs(x - lastTextX) > 56)
        {
            canvas.SetTextForeground(wxColour(55, 65, 80));
            canvas.DrawTextAt(WxFormatTimeLabel(ts), std::max(LEFT_MARGIN + 2, x - 14), 4);
            lastTextX = x;
        }
    }

    if (panel.m_hoverPlotX >= LEFT_MARGIN && panel.m_hoverPlotX < size.x - WAVE_PADDING && !panel.m_rulerHoverLabel.empty())
    {
        wxSize tw = canvas.GetTextExtent(panel.m_rulerHoverLabel);
        int tx = std::min(panel.m_hoverPlotX + 6, size.x - (int)tw.x - WAVE_PADDING - 6);
        tx = std::max(LEFT_MARGIN + 2, tx);
        canvas.SetTextForeground(wxColour(75, 85, 105));
        canvas.DrawTextAt(panel.m_rulerHoverLabel, tx, 4);
    }

    for (int i = firstRow; i < lastRow; i++)
    {
        signal_t* sig = panel.m_displayedSignals2[i];
        const auto commentIt = panel.m_rowComments.find(i);
        const bool isCommentRow = (commentIt != panel.m_rowComments.end());
        int yBase = panel.RowYBase(i);

        if (isCommentRow) {
            wxFont f = canvas.GetFont();
            wxFont italic = f.Italic();
            canvas.SetFont(italic);
            canvas.SetTextForeground(wxColour(100, 75, 10));
            canvas.DrawTextAt("// " + commentIt->second, 10, yBase - 8);
            canvas.SetFont(f);
            continue;
        }

        if (!sig) continue;

        bool isMatch = !panel.m_searchKeyword.empty() && panel.m_searchMatchedSignals.count(sig->signal_id);

        wxString displayName = panel.TraceRowDisplayName(sig);

        canvas.SetTextForeground(isMatch ? wxColour(255, 60, 60) : Th().signalNameText);
        canvas.DrawTextAt(displayName, 10, yBase - 8);

        if (cacheOk && i < (int)panel.m_cachedSegments.size()) {
            auto& segments = panel.m_cachedSegments[i];
            const WaveformPanel::WaveTraceKind rowKind = segments.empty()
                ? WaveformPanel::ClassifyTraceKind(sig) : segments[0].traceKind;
            if (rowKind == WaveformPanel::WaveTraceKind::BusBits
                || rowKind == WaveformPanel::WaveTraceKind::TextString
                || rowKind == WaveformPanel::WaveTraceKind::TransactionEvent)
            {
                for (size_t k = 0; k < segments.size(); k++)
                {
                    auto& seg = segments[k];
                    if (!seg.text.empty()) {
                        int segY = seg.y - scrollPx;
                        int rectW = seg.x2 - seg.x1;
                        if (rectW > 25) {
                            lock.unlock();
                            const std::string label = panel.BusSegmentDisplayText(sig, seg, scale);
                            lock.lock();
                            const bool isTx = (rowKind == WaveformPanel::WaveTraceKind::TransactionEvent);
                            const wxRect rect(seg.x1, segY - 9, rectW, 18);
                            DrawBusSegmentLabel(canvas, rect, label,
                                isTx ? wxColour(90, 20, 120) : wxColour(0, 80, 0));
                        }
                    }
                }
            }
        }

        if (panel.m_showCursorValue || panel.m_measureMode != WaveformPanel::MEASURE_NONE) {
            lock.unlock();
            if (panel.m_showCursorValue) {
                const std::string val = panel.GetValueAt(sig, panel.m_currentTimestamp);
                if (!val.empty()) {
                    wxString text = wxString::Format("%s = %s", sig->full_name, val.c_str());
                    int tx = cursorX + 5;
                    int ty = yBase - 8;
                    if (tx > size.x - 150) tx = cursorX - 140;
                    wxSize sz = canvas.GetTextExtent(text);
                    canvas.SetFillColour(Th().cursorValueBg);
                    canvas.SetStrokeColour(wxColour(200, 200, 100));
                    canvas.DrawRectangle(tx - 2, ty - 1, sz.x + 4, sz.y + 2);
                    canvas.SetTextForeground(*wxBLACK);
                    canvas.DrawTextAt(text, tx, ty);
                }
            }
            if (panel.m_measureMode != WaveformPanel::MEASURE_NONE) {
                double val = 0;
                wxString text;
                if (panel.m_measureMode == WaveformPanel::MEASURE_FREQ) {
                    val = panel.ComputeFrequency(sig);
                    text = wxString::Format("F=%.3f", val);
                } else if (panel.m_measureMode == WaveformPanel::MEASURE_DUTY) {
                    val = panel.ComputeDuty(sig);
                    text = wxString::Format("D=%.2f%%", val * 100);
                }
                canvas.SetTextForeground(wxColour(120, 0, 120));
                canvas.DrawTextAt(text, size.x - 120, yBase - 10);
            }
            lock.lock();
        }
    }

    if (cacheOk && panel.m_hoverSignal >= 0 && panel.m_hoverSegment >= 0
        && panel.m_hoverSignal < (int)panel.m_cachedSegments.size()
        && panel.m_hoverSegment < (int)panel.m_cachedSegments[panel.m_hoverSignal].size())
    {
        auto& seg = panel.m_cachedSegments[panel.m_hoverSignal][panel.m_hoverSegment];
        if (!seg.text.empty())
        {
            wxString info = "Value: " + seg.text;
            int mx, my;
            wxGetMousePosition(&mx, &my);
            panel.ScreenToClient(&mx, &my);
            canvas.SetTextForeground(*wxBLACK);
            canvas.DrawTextAt(info, mx + 15, my + 13);
        }
    }

    for (size_t i = 0; i < panel.m_markers.size(); i++)
    {
        auto& mk = panel.m_markers[i];
        int x = panel.TimeToX(mk.timestamp, scale);
        bool isDrag = ((int)i == panel.m_draggingMarkerIndex);
        canvas.SetTextForeground(*wxBLACK);
        canvas.DrawTextAt(mk.label, x + 6, 7);
        if (isDrag)
            canvas.DrawTextAt(wxString::Format("T=%lld", mk.timestamp), x + 5, canvas.GetTextExtent(mk.label).y + 15);
    }

    if (panel.m_hasMarkerA && panel.m_markerA >= 0 && panel.m_markerB >= 0)
    {
        int xA = panel.TimeToX(panel.m_markerA, scale);
        int xB = panel.TimeToX(panel.m_markerB, scale);
        wxString text = wxString::Format("ΔT = %lld", (long long)(panel.m_markerB - panel.m_markerA));
        canvas.SetTextForeground(wxColour(200, 0, 0));
        canvas.DrawTextAt(text, (xA + xB) / 2, 10);
    }

    DrawMarkerMeasurementBar(panel, canvas, size, scale);
}

#endif // BEAR2WAVE_RENDER_OPENGL

} // namespace WaveformPainter
