#include "panels/WaveformRenderer.h"
#include "panels/WaveformPanel.hpp"
#include "waveform_constants.h"
#include "core/trace_display.h"
#include "core/trace_translate_debug.h"
#include "core/ghw_state.h"
#include "core/waveform_perf.h"
#include "core/trace_vc.h"
#include "core/WaveformRadix.h"

#include <wx/wx.h>

#include <algorithm>
#include <cmath>
#include <climits>
#include <cctype>
#include <cstring>
#include <limits>
#include <map>
#include <mutex>
#include <thread>
#include <vector>

namespace WaveformRenderer {

bool ParseTraceDouble(const char* s, double* out)
{
    return bear2wave_parse_trace_double(s, out);
}

WaveTraceKind ClassifyTraceKind(const signal_t* sig)
{
    return static_cast<WaveTraceKind>(bear2wave_classify_trace_kind(sig));
}

char ParseVcdValue(const char* v)
{
    if (!v || !v[0]) return '0';
    return bear2wave_nine_state_waveform_char(v[0]);
}

std::string FormatTimeLabel(double t)
{
    char buf[64];
    if (t < 1e3)
        snprintf(buf, sizeof(buf), "%.0f ns", t);
    else if (t < 1e6)
        snprintf(buf, sizeof(buf), "%.2f us", t / 1e3);
    else if (t < 1e9)
        snprintf(buf, sizeof(buf), "%.2f ms", t / 1e6);
    else
        snprintf(buf, sizeof(buf), "%.2f s", t / 1e9);
    return buf;
}

static const char* TraceVcValueAt(signal_t* sig, size_t index)
{
    static thread_local char s_buf[VCD_SIGNAL_SIZE];
    if (!sig || index >= trace_vc_count(sig))
        return "";
    trace_vc_format_value(sig, index, s_buf, sizeof(s_buf));
    return s_buf;
}

void BuildCacheAsync(WaveformPanel& panel)
    {
        panel.JoinCacheWorkerIfJoined();
        if (panel.m_allSignals.empty()) {
            std::lock_guard<std::mutex> lock(panel.m_cacheMutex);
            panel.m_cachedSegments.clear();
            return;
        }
        if (panel.m_displayedSignals2.empty()) {
            std::lock_guard<std::mutex> lock(panel.m_cacheMutex);
            panel.m_cachedSegments.clear();
            return;
        }

        // 强制获取最新、有效的面板尺寸
        wxSize size = panel.GetClientSize();
        int viewW = size.GetWidth() - LEFT_MARGIN - WAVE_PADDING;
        // 确保viewW至少为100，避免缓存为空
        viewW = std::max(viewW, 100);
        long long range = panel.m_displayTimeRange;
        if (range < 1)
            range = 1;
        const double scale = (double)viewW / (double)range;
        const long long offset = panel.m_timeOffset;
        auto signals = panel.m_displayedSignals2;
        const int scrollRow = panel.m_signalScrollRow;
        const int visRows = panel.VisibleSignalRowCount();
        const int rowPad = WaveformPerf::CacheVisibleRowPad();
        size_t cacheRowFirst = 0;
        size_t cacheRowLast = signals.size();
        if (WaveformPerf::CacheVisibleRowsOnly() && !signals.empty()) {
            cacheRowFirst = (size_t)std::max(0, scrollRow - rowPad);
            cacheRowLast = std::min(
                signals.size(), (size_t)std::max(scrollRow + visRows + rowPad, scrollRow + 1));
        }

        std::map<signal_t*, WaveformPanel::DataFormat> signalDataFormats = panel.m_signalDataFormats;
        const std::map<signal_t*, int> signalTransforms = panel.m_signalTransforms;
        const std::map<signal_t*, long long> signalTimeShifts = panel.m_signalTimeShift;
        const uint64_t transformEpoch = panel.m_cacheTransformEpoch;

        const int budgetRows = WaveformPerf::CacheVisibleRowsOnly()
            ? std::max(1, (int)(cacheRowLast > cacheRowFirst ? cacheRowLast - cacheRowFirst : 1))
            : std::max(1, (int)signals.size());

        const uint64_t buildEpoch = ++panel.m_cacheBuildEpoch;
        const long long cacheKeyOffset = offset;
        const long long cacheKeyRange = range;
        const int cacheKeyViewW = viewW;
        panel.m_cacheBuildThread = std::thread([&panel, buildEpoch, signals, signalDataFormats, signalTransforms, signalTimeShifts, viewW, range, offset, scale,
                                          budgetRows, cacheKeyOffset, cacheKeyRange, cacheKeyViewW, scrollRow,
                                          cacheRowFirst, cacheRowLast, transformEpoch]() {
            const long long visibleStart = offset;
            const long long visibleEnd = offset + range;
            trace_translate_error_log("cache_build begin rows=%zu transform_epoch=%llu snapshot_tf=%zu",
                signals.size(), (unsigned long long)transformEpoch, signalTransforms.size());

            std::vector<std::vector<WaveformPanel::DrawSegment>> newCache;
            newCache.resize(signals.size());

            const size_t maxSeg = (size_t)WaveformPerf::MaxDrawSegments();

            const auto buildRow = [&](size_t i) {
                signal_t* sig = signals[i];
                std::vector<WaveformPanel::DrawSegment> segments;
                const size_t count = trace_vc_count(sig);
                if (!sig || count == 0)
                {
                    newCache[i] = {};
                    return;
                }
                vcd_ensure_signal_sorted(sig);

                static thread_local char s_cacheVcBuf[VCD_SIGNAL_SIZE];
                const auto ts_at = [sig](size_t idx) -> long long {
                    return (long long)trace_vc_timestamp(sig, idx);
                };
                const auto val_at = [sig](size_t idx) -> const char* {
                    trace_vc_format_value(sig, idx, s_cacheVcBuf, sizeof(s_cacheVcBuf));
                    return s_cacheVcBuf;
                };

                int yBase = 25 + (int)i * SIGNAL_ROW_HEIGHT + 30;
                int yH = yBase - 15, yL = yBase + 15;

                WaveformPanel::WaveTraceKind tk = WaveformPanel::ClassifyTraceKind(sig);
                if (tk == WaveformPanel::WaveTraceKind::RealAnalog) {
                    bool any = false;
                    for (size_t j = 0; j < count; j++) {
                        double tmp;
                        if (WaveformRenderer::ParseTraceDouble(val_at(j), &tmp)) { any = true; break; }
                    }
                    if (!any) tk = WaveformPanel::WaveTraceKind::DigitalScalar;
                }

                WaveformPanel::DataFormat format = WaveformPanel::FORMAT_BINARY;
                auto formatIt = signalDataFormats.find(sig);
                if (formatIt != signalDataFormats.end())
                    format = formatIt->second;

                const auto ts_lower = [&](long long t) -> size_t {
                    size_t lo = 0, hi = count;
                    while (lo < hi) {
                        const size_t mid = lo + (hi - lo) / 2;
                        if (ts_at(mid) < t)
                            lo = mid + 1;
                        else
                            hi = mid;
                    }
                    return lo;
                };
                const auto ts_upper = [&](long long t) -> size_t {
                    if (count == 0)
                        return 0;
                    size_t lo = 0, hi = count;
                    while (lo < hi) {
                        const size_t mid = lo + (hi - lo) / 2;
                        if (ts_at(mid) <= t)
                            lo = mid + 1;
                        else
                            hi = mid;
                    }
                    return lo > 0 ? lo - 1 : 0;
                };

                long long rowShift = 0;
                {
                    auto itSh = signalTimeShifts.find(sig);
                    if (itSh != signalTimeShifts.end())
                        rowShift = itSh->second;
                }
                const auto trace_ts_lower = ts_lower;
                const auto trace_ts_upper = ts_upper;
                const auto visible_ts_lower = [&](long long displayStart) {
                    return trace_ts_lower(displayStart - rowShift);
                };
                const auto value_index_at_display = [&](long long displayT) {
                    return trace_ts_upper(displayT - rowShift);
                };
                const auto display_ts = [&](size_t idx) -> long long {
                    return ts_at(idx) + rowShift;
                };

                const size_t c = visible_ts_lower(visibleStart);

                const bool isBusEarly = (tk == WaveformPanel::WaveTraceKind::BusBits
                    || tk == WaveformPanel::WaveTraceKind::TextString
                    || tk == WaveformPanel::WaveTraceKind::TransactionEvent);
                WaveformPerf::LodTraceKind lodKind = WaveformPerf::LodTraceKind::Scalar;
                if (tk == WaveformPanel::WaveTraceKind::RealAnalog)
                    lodKind = WaveformPerf::LodTraceKind::Analog;
                else if (isBusEarly)
                    lodKind = WaveformPerf::LodTraceKind::Bus;
                const size_t rowBudget = WaveformPerf::RowSegmentBudget(
                    viewW, budgetRows, maxSeg, lodKind);
                const size_t pixelBudget = std::max<size_t>(
                    256, std::min((size_t)std::max(256, viewW * 4), rowBudget));
                const size_t step = std::max<size_t>(1, count / pixelBudget);

                if (tk == WaveformPanel::WaveTraceKind::RealAnalog) {
                    double vmin = std::numeric_limits<double>::infinity();
                    double vmax = -std::numeric_limits<double>::infinity();
                    auto consider = [&](const char* v) {
                        double d;
                        if (WaveformRenderer::ParseTraceDouble(v, &d)) {
                            vmin = std::min(vmin, d);
                            vmax = std::max(vmax, d);
                        }
                    };
                    if (c > 0) consider(val_at(c - 1));
                    for (size_t j = c; j < count; j++) {
                        const long long dts = display_ts(j);
                        if (dts > visibleEnd) break;
                        if (dts >= visibleStart) consider(val_at(j));
                    }
                    if (!std::isfinite(vmin) || !std::isfinite(vmax) || vmax < vmin) {
                        vmin = 0.0;
                        vmax = 1.0;
                    }
                    double span = vmax - vmin;
                    if (span < 1e-300) {
                        vmin -= 0.5;
                        vmax += 0.5;
                        span = 1.0;
                    }

                    auto mapY = [&](double d) {
                        int yy = (int)(yBase + 15.0 - (d - vmin) / span * 30.0);
                        const int top = yBase - 15, bot = yBase + 15;
                        if (yy < top) yy = top;
                        if (yy > bot) yy = bot;
                        return yy;
                    };

                    double lastD = 0.0;
                    std::string lastText = "0";
                    bool haveD = false;
                    for (size_t k = 0; k < c; k++) {
                        double d;
                        if (WaveformRenderer::ParseTraceDouble(val_at(k), &d)) {
                            lastD = d;
                            lastText = val_at(k);
                            haveD = true;
                        }
                    }
                    if (!haveD && count > 0) {
                        double d;
                        if (WaveformRenderer::ParseTraceDouble(val_at(0), &d)) {
                            lastD = d;
                            lastText = val_at(0);
                        }
                    }
                    int lastY = mapY(lastD);

                    const bool analogLinear =
                        panel.m_analogRenderStyle == WaveformPanel::AnalogRenderStyle::Linear;

                    int lastDrawX = LEFT_MARGIN;
                    int lastPixelX = -1;
                    for (size_t scan = c; scan < count; scan += step) {
                        if (segments.size() >= rowBudget)
                            break;
                        const long long dts = display_ts(scan);
                        if (dts > visibleEnd) break;

                        int currX = LEFT_MARGIN + (int)((double)(dts - offset) * scale);
                        if (currX == lastPixelX) continue;
                        lastPixelX = currX;
                        if (currX < lastDrawX) continue;

                        int currY = lastY;
                        if (WaveformRenderer::ParseTraceDouble(val_at(scan), &lastD)) {
                            lastText = val_at(scan);
                            currY = mapY(lastD);
                        }

                        WaveformPanel::DrawSegment seg;
                        seg.x1 = lastDrawX;
                        seg.x2 = currX;
                        seg.traceKind = WaveformPanel::WaveTraceKind::RealAnalog;
                        seg.y = lastY;
                        seg.yEnd = analogLinear ? currY : -1;
                        seg.value = 'r';
                        seg.text = lastText;
                        segments.push_back(seg);

                        lastDrawX = currX;
                        lastY = currY;
                    }

                    int endX = LEFT_MARGIN + (int)((double)(visibleEnd - offset) * scale);
                    if (endX > lastDrawX) {
                        WaveformPanel::DrawSegment seg;
                        seg.x1 = lastDrawX;
                        seg.x2 = endX;
                        seg.traceKind = WaveformPanel::WaveTraceKind::RealAnalog;
                        seg.y = lastY;
                        seg.yEnd = -1;
                        seg.value = 'r';
                        seg.text = lastText;
                        segments.push_back(seg);
                    }

                    newCache[i] = std::move(segments);
                    return;
                }

                const bool isBus = (tk == WaveformPanel::WaveTraceKind::BusBits
                    || tk == WaveformPanel::WaveTraceKind::TextString
                    || tk == WaveformPanel::WaveTraceKind::TransactionEvent);

                char lastVal = '0';
                std::string lastRaw;
                std::string lastText;
                int lastDrawX = LEFT_MARGIN;

                const int sigW = (int)sig->size;

                const auto pushLevelSeg = [&](int x1, int x2, char val, const std::string& text) {
                    if (x2 <= x1 || segments.size() >= rowBudget)
                        return;
                    WaveformPanel::DrawSegment seg;
                    seg.x1 = x1;
                    seg.x2 = x2;
                    seg.traceKind = isBus ? tk : WaveformPanel::WaveTraceKind::DigitalScalar;
                    seg.value = isBus ? 'b' : val;
                    seg.y = isBus ? yBase : (val == '0' ? yL : yH);
                    seg.text = text;
                    segments.push_back(std::move(seg));
                };

                const auto valueSame = [&](const char* a, const char* b) -> bool {
                    if (isBus)
                        return std::strcmp(a ? a : "", b ? b : "") == 0;
                    return WaveformRenderer::ParseVcdValue(a) == WaveformRenderer::ParseVcdValue(b);
                };

                // O(viewW * log count): one sample per pixel column (scalar: 0/1; bus: full value)
                segments.reserve(std::min(rowBudget, (size_t)viewW * 2 + 4));
                {
                    const size_t idx0 = value_index_at_display(visibleStart);
                    const char* raw0 = val_at(idx0);
                    lastVal = WaveformRenderer::ParseVcdValue(raw0);
                    lastRaw = raw0 ? raw0 : "";
                    lastText = panel.FormatValueByDataFormat(
                        raw0, format, sigW, sig, ts_at(idx0), &signalTransforms);

                    for (int px = 1; px <= viewW && segments.size() < rowBudget; ++px) {
                        const int currX = LEFT_MARGIN + std::min(px, viewW);
                        const long long tAtX = offset + (long long)px * range / viewW;
                        if (tAtX > visibleEnd)
                            break;

                        const size_t idx = value_index_at_display(tAtX);
                        const char* newRaw = val_at(idx);
                        if (valueSame(lastRaw.c_str(), newRaw))
                            continue;

                        if (currX > lastDrawX)
                            pushLevelSeg(lastDrawX, currX, lastVal, lastText);

                        lastDrawX = currX;
                        lastVal = WaveformRenderer::ParseVcdValue(newRaw);
                        lastRaw = newRaw ? newRaw : "";
                        lastText = panel.FormatValueByDataFormat(
                            newRaw, format, sigW, sig, ts_at(idx), &signalTransforms);
                    }
                }

                int endX = LEFT_MARGIN + (int)((double)(visibleEnd - offset) * scale);
                if (endX > lastDrawX) {
                    WaveformPanel::DrawSegment seg;
                    seg.x1 = lastDrawX;
                    seg.x2 = endX;
                    seg.traceKind = isBus ? tk : WaveformPanel::WaveTraceKind::DigitalScalar;
                    seg.value = isBus ? 'b' : lastVal;
                    seg.y = isBus ? yBase : (lastVal == '0' ? yL : yH);
                    seg.text = lastText;
                    segments.push_back(seg);
                }

                newCache[i] = std::move(segments);
                {
                    auto itf = signalTransforms.find(sig);
                    if (itf != signalTransforms.end() && (itf->second & WaveformRenderer::TR_TRANSLATE_PROC)) {
                        static std::mutex s_rowLogMu;
                        static int s_rowLog = 0;
                        std::lock_guard<std::mutex> lk(s_rowLogMu);
                        if (s_rowLog < 20) {
                            ++s_rowLog;
                            const auto& segs = newCache[i];
                            const char* t0 = segs.empty() ? "" : segs[0].text.c_str();
                            trace_translate_error_log("cache_row sig=%s ptr=%p segs=%zu text0=\"%s\"",
                                sig->full_name, (void*)sig, segs.size(), t0);
                        }
                    }
                }
            };

            const unsigned nThreads = WaveformPerf::CacheBuildThreads();
            const size_t buildCount = cacheRowLast > cacheRowFirst ? (cacheRowLast - cacheRowFirst) : 0;
            if (nThreads <= 1 || buildCount < 2) {
                for (size_t i = cacheRowFirst; i < cacheRowLast; ++i)
                    buildRow(i);
            } else {
                std::vector<std::thread> workers;
                const size_t chunk = (buildCount + nThreads - 1) / nThreads;
                workers.reserve(nThreads);
                for (unsigned t = 0; t < nThreads; ++t) {
                    const size_t i0 = cacheRowFirst + (size_t)t * chunk;
                    const size_t i1 = std::min(cacheRowLast, i0 + chunk);
                    if (i0 >= i1)
                        continue;
                    workers.emplace_back([&, i0, i1]() {
                        for (size_t i = i0; i < i1; ++i)
                            buildRow(i);
                    });
                }
                for (auto& w : workers)
                    w.join();
            }

            std::lock_guard<std::mutex> lock2(panel.m_cacheMutex);
            if (buildEpoch != panel.m_cacheBuildEpoch)
                return;
            panel.m_cachedSegments = std::move(newCache);
            panel.m_cacheKeyOffset = cacheKeyOffset;
            panel.m_cacheKeyRange = cacheKeyRange;
            panel.m_cacheKeyViewW = cacheKeyViewW;
            panel.m_cacheKeyScrollRow = scrollRow;
            panel.m_cacheKeyTransformEpoch = transformEpoch;
            panel.CallAfter([&panel]() { panel.Refresh(); });
        });
    }

} // namespace WaveformRenderer

