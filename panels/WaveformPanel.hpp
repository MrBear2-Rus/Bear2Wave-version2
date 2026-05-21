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
#include <wx/textfile.h>
#include <wx/button.h>
#include <wx/stattext.h>
#include <wx/textctrl.h>
#include <wx/checkbox.h>
#include <wx/log.h>
#include <wx/generic/logg.h>
#include <wx/glcanvas.h>
#include <wx/filefn.h>
#include <wx/progdlg.h>
#include <wx/settings.h>

/* wx 在 MSVC 上会间接包含 windows.h（定义 GetCurrentTime 等宏） */
#ifdef GetCurrentTime
#undef GetCurrentTime
#endif
#ifdef SetCurrentTime
#undef SetCurrentTime
#endif

#include <gl/GL.h>
#include <gl/GLU.h>

#include <fstapi.h>
#include <algorithm>
#include <climits>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <random>
#include <sstream>
#include <string>
#include <thread>
#include <unordered_set>
#include <vector>

#include "waveform_constants.h"
#include "core/waveform_perf.h"
#include "core/WaveformRadix.h"
#include "panels/SignalTraceContextMenu.hpp"
#include "vcd.h"
#include "trace_loader.h"
#include "core/trace_var_types.h"

class WaveformPanel : public wxGLCanvas
{
public:
    enum class WaveTraceKind {
        DigitalScalar,
        BusBits,
        RealAnalog,
        TextString
    };

    struct DrawSegment
    {
        int x1, x2;
        int y;
        char value;
        std::string text;
        WaveTraceKind traceKind = WaveTraceKind::DigitalScalar;
    };

    static bool ParseTraceDouble(const char* s, double* out)
    {
        if (!s || !s[0] || !out) return false;
        char* endp = nullptr;
        double d = strtod(s, &endp);
        if (endp == s || !std::isfinite(d)) return false;
        *out = d;
        return true;
    }

    static WaveTraceKind ClassifyTraceKind(const signal_t* sig)
    {
        if (!sig) return WaveTraceKind::DigitalScalar;
        const int32_t t = sig->fst_var_type;

        if (t == BEAR2WAVE_VT_STRING || t == BEAR2WAVE_VT_TIME || t == BEAR2WAVE_VT_TRANSACTION)
            return WaveTraceKind::TextString;
        if (t == BEAR2WAVE_VT_REAL || t == BEAR2WAVE_VT_ANALOG)
            return WaveTraceKind::RealAnalog;

        if (t >= 0) {
            if (t == (int32_t)FST_VT_GEN_STRING)
                return WaveTraceKind::TextString;
#ifdef FST_VT_VCD_EVENT
            if (t == (int32_t)FST_VT_VCD_EVENT)
                return WaveTraceKind::TextString;
#endif
            if (t == (int32_t)FST_VT_VCD_REAL || t == (int32_t)FST_VT_VCD_REAL_PARAMETER
                || t == (int32_t)FST_VT_VCD_REALTIME || t == (int32_t)FST_VT_SV_SHORTREAL)
                return WaveTraceKind::RealAnalog;
            if (sig->size > 1u)
                return WaveTraceKind::BusBits;
            return WaveTraceKind::DigitalScalar;
        }
        if (sig->size > 1u)
            return WaveTraceKind::BusBits;
        return WaveTraceKind::DigitalScalar;
    }

    enum MeasureMode
    {
        MEASURE_NONE,
        MEASURE_FREQ,
        MEASURE_DUTY
    };

    MeasureMode m_measureMode = MEASURE_NONE;

    std::vector<int> m_cursorPositions;
    double m_dragRemainder = 0.0;
    long long m_timeOffset = 0;
    bool m_showCursorValue = false;
    bool m_isDragging = false;
    /** Left-drag on plot: move playhead / cursor time (syncs bottom slider). */
    bool m_scrubbingPlayhead = false;
    int m_lastMouseX = 0;
    bool m_isSelecting = false;
    int m_selectStartX = 0;
    int m_selectEndX = 0;
    int m_hoverSignal = -1;
    int m_hoverSegment = -1;
    int m_draggingMarkerIndex = -1;
    int m_hoverMarkerIndex = -1;
    int m_draggingMarkerStartX = 0;

    long long m_markerA = -1;
    long long m_markerB = -1;
    bool m_hasMarkerA = false;
    bool m_isMeasuring = false;
    /** Ctrl+左键拖动：红色 A/B 测量条，不移动播放头 */
    bool m_measuringDrag = false;

    bool m_isEditingMarker = false;
    int m_editingMarkerIndex = -1;

    wxRect m_minimapRect;
    bool m_draggingMinimap = false;

    long long m_limitStart = 0;
    long long m_limitEnd = -1;
    bool m_hasLimit = false;

    std::unordered_set<std::string> m_searchMatchedSignals;
    std::string m_searchKeyword;

    wxString m_editingMarkerText;
    std::unordered_set<std::string> m_visibleSignals;

    std::mutex m_cacheMutex;
    std::vector<std::vector<DrawSegment>> m_cachedSegments;
    long long m_cacheKeyOffset = LLONG_MIN;
    long long m_cacheKeyRange = -1;
    int m_cacheKeyViewW = -1;

    struct Marker
    {
        long long timestamp;
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
    
    /** Row index in m_displayedSignals2 -> comment text (GTKWave-style comment row). */
    std::map<int, wxString> m_rowComments;
    
    // VCD数据
    vcd_t* m_vcdData;
    long long m_currentTimestamp;
    long long m_displayTimeRange;
    long long m_maxTimestamp;
    SignalGroup* m_signalTreeRoot = nullptr;
    std::vector<signal_t*> m_allSignals;
    std::map<std::string, wxColour> m_signalColors;

    /** Reserved for future GL waveform path; painting currently uses wxAutoBufferedPaintDC. */
    wxGLContext* m_glContext = nullptr;
    
    // 随机数生成器
    std::mt19937 m_rng;
    
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
        FORMAT_REAL,
        FORMAT_TIME,
        FORMAT_ENUM
    };
    std::map<signal_t*, DataFormat> m_signalDataFormats;

    static const int TR_INVERT = 1;
    static const int TR_REVERSE_BITS = 2;
    static const int TR_RIGHT_JUSTIFY = 4;
    static const int TR_POPCNT = 8;
    static const int TR_RANGE_FILL = 16;
    std::map<signal_t*, int> m_signalTransforms;
    std::map<signal_t*, int> m_signalGrayLevel;
    std::map<signal_t*, int> m_fixedPointShift;
    std::vector<std::pair<std::string, std::string>> m_translateRules;

    std::unordered_set<std::string> m_manualColorSignalIds;
    int m_cycleColorIndex = 0;

    DataFormat DataFormatForSignal(signal_t* sig) const
    {
        auto it = m_signalDataFormats.find(sig);
        return it != m_signalDataFormats.end() ? it->second : FORMAT_BINARY;
    }

    static WaveformRadix::Radix ToWaveformRadix(DataFormat f)
    {
        switch (f) {
        case FORMAT_BINARY: return WaveformRadix::Radix::Binary;
        case FORMAT_OCTAL: return WaveformRadix::Radix::Octal;
        case FORMAT_DECIMAL: return WaveformRadix::Radix::Decimal;
        case FORMAT_HEXADECIMAL: return WaveformRadix::Radix::Hex;
        case FORMAT_ASCII: return WaveformRadix::Radix::Ascii;
        case FORMAT_SIGNED_DECIMAL: return WaveformRadix::Radix::Signed;
        case FORMAT_REAL: return WaveformRadix::Radix::Real;
        case FORMAT_TIME:
        case FORMAT_ENUM:
            return WaveformRadix::Radix::Binary;
        }
        return WaveformRadix::Radix::Binary;
    }

    static DataFormat FromWaveformRadix(WaveformRadix::Radix r)
    {
        switch (r) {
        case WaveformRadix::Radix::Binary: return FORMAT_BINARY;
        case WaveformRadix::Radix::Octal: return FORMAT_OCTAL;
        case WaveformRadix::Radix::Decimal: return FORMAT_DECIMAL;
        case WaveformRadix::Radix::Hex: return FORMAT_HEXADECIMAL;
        case WaveformRadix::Radix::Ascii: return FORMAT_ASCII;
        case WaveformRadix::Radix::Signed: return FORMAT_SIGNED_DECIMAL;
        case WaveformRadix::Radix::Real: return FORMAT_REAL;
        }
        return FORMAT_BINARY;
    }

    void SetSignalDataFormat(signal_t* sig, DataFormat fmt)
    {
        if (!sig)
            return;
        m_signalDataFormats[sig] = fmt;
        RequestDrawCacheRebuild();
        Refresh();
    }

    void SetSignalDataFormatByName(const std::string& name, DataFormat fmt)
    {
        if (name.empty() || !m_vcdData)
            return;
        if (signal_t* sig = vcd_get_signal_by_name(m_vcdData, name.c_str())) {
            SetSignalDataFormat(sig, fmt);
            return;
        }
        for (signal_t* sig : m_allSignals) {
            if (!sig)
                continue;
            if (name == sig->full_name || name == sig->name) {
                SetSignalDataFormat(sig, fmt);
                return;
            }
        }
    }

    int GtkwaveRadixCodeForSignal(signal_t* sig) const
    {
        return WaveformRadix::ToGtkwaveCode(ToWaveformRadix(DataFormatForSignal(sig)));
    }

    /** True iff timestamps are non-decreasing (required for binary probe). */
    static bool ValueChangesNonDecreasing(const value_change_t* vec, size_t count)
    {
        for (size_t c = 1; c < count; ++c) {
            if (vec[c - 1].timestamp > vec[c].timestamp)
                return false;
        }
        return true;
    }

    /** Last raw VCD/FST value string at or strictly before ts. */
    const char* RawValueAtOrBefore(signal_t* sig, long long ts) const
    {
        if (!sig || sig->changes_count == 0 || !sig->value_changes) return nullptr;
        vcd_ensure_signal_sorted(sig);
        const value_change_t* vec = sig->value_changes;
        const size_t count = sig->changes_count;
        const timestamp_t tsc = (ts < 0) ? 0 : (timestamp_t)ts;
        timestamp_t bestTs = 0;
        const char* bestVal = nullptr;
        bool have = false;

        if (sig->changes_sorted) {
            size_t lo = 0, hi = count;
            while (lo < hi) {
                const size_t mid = lo + (hi - lo) / 2;
                if (vec[mid].timestamp <= tsc)
                    lo = mid + 1;
                else
                    hi = mid;
            }
            if (lo > 0) {
                size_t i = lo - 1;
                while (i + 1 < count && vec[i + 1].timestamp == vec[i].timestamp)
                    ++i;
                bestTs = vec[i].timestamp;
                bestVal = vec[i].value;
                have = true;
            }
        } else {
            for (size_t c = 0; c < count; c++) {
                const timestamp_t tv = vec[c].timestamp;
                if (tv > tsc) continue;
                if (!have || tv > bestTs) {
                    bestTs = tv;
                    bestVal = vec[c].value;
                    have = true;
                } else if (tv == bestTs) {
                    bestVal = vec[c].value;
                }
            }
        }

        if (have) {
            const bool row0hit = !m_displayedSignals2.empty() && sig == m_displayedSignals2[0];
            if (row0hit) {
                static long long s_rvatHitRow0Ts = LLONG_MIN;
                if (ts != s_rvatHitRow0Ts) {
                    s_rvatHitRow0Ts = ts;
                    wxLogDebug(
                        "[Bear2Wave][RVAT] HIT row0 req_ts=%lld pick_ts=%llu raw=\"%s\"",
                        (long long)ts,
                        (unsigned long long)bestTs,
                        bestVal ? bestVal : "");
                }
            }
            return bestVal;
        }
        {
            timestamp_t tmin = vec[0].timestamp;
            timestamp_t tmax = vec[0].timestamp;
            for (size_t c = 1; c < count; ++c) {
                if (vec[c].timestamp < tmin) tmin = vec[c].timestamp;
                if (vec[c].timestamp > tmax) tmax = vec[c].timestamp;
            }
            const bool row0 = !m_displayedSignals2.empty() && sig == m_displayedSignals2[0];
            if (WaveDebugVerbose()) {
                wxLogDebug(
                    "[Bear2Wave][RVAT] MISS sig=%s req_ts=%lld tsc=%llu nchg=%zu tmin=%llu tmax=%llu",
                    sig->full_name,
                    (long long)ts,
                    (unsigned long long)tsc,
                    count,
                    (unsigned long long)tmin,
                    (unsigned long long)tmax);
            } else if (row0) {
                static long long s_rvatMissRow0Ts = LLONG_MIN;
                if (ts != s_rvatMissRow0Ts) {
                    s_rvatMissRow0Ts = ts;
                    wxLogDebug(
                        "[Bear2Wave][RVAT] MISS row0 sig=%s req_ts=%lld tsc=%llu nchg=%zu tmin=%llu tmax=%llu",
                        sig->full_name,
                        (long long)ts,
                        (unsigned long long)tsc,
                        count,
                        (unsigned long long)tmin,
                        (unsigned long long)tmax);
                }
            }
        }
        return nullptr;
    }

    // 获取信号在指定时间的值（与 BuildDrawCacheAsync 中 seg.text 一致：同一套 FormatValueByDataFormat）
    std::string GetValueAt(signal_t* sig, long long ts)
    {
        if (!sig) return "";
        EnsureTraceSignalLoadedSync(sig);
        if (sig->changes_count == 0 || !sig->value_changes) return "";
        long long tq = ts;
        if (m_maxTimestamp > 0)
            tq = std::max(0LL, std::min(tq, m_maxTimestamp));
        const WaveTraceKind tk = ClassifyTraceKind(sig);
        const char* lastStr = RawValueAtOrBefore(sig, tq);
        if (!lastStr || !lastStr[0]) {
            if (!m_displayedSignals2.empty() && sig == m_displayedSignals2[0]) {
                static long long s_dbgEmptyTs = LLONG_MIN;
                if (ts != s_dbgEmptyTs) {
                    s_dbgEmptyTs = ts;
                    wxLogDebug(
                        "[Bear2Wave][GVA] EMPTY row0 sig=%s ts=%lld (RawValueAtOrBefore null/empty)",
                        sig->full_name,
                        (long long)tq);
                }
            }
            return "";
        }

        /* 立刻拷贝：避免后续 Format 过程中别处改写同一块 value_changes 导致野串 */
        const std::string rawHold(lastStr);

        std::string out;
        if (tk == WaveTraceKind::RealAnalog)
            out = rawHold;
        else
            out = FormatValueByDataFormat(
                rawHold.c_str(), DataFormatForSignal(sig), (int)sig->size, sig, ts);

        if (WaveDebugVerbose() && !m_displayedSignals2.empty() && sig == m_displayedSignals2[0]) {
            static long long s_dbgGvaTs = LLONG_MIN;
            if (tq != s_dbgGvaTs) {
                s_dbgGvaTs = tq;
                wxLogDebug(
                    "[Bear2Wave][GVA] row0 ts=%lld raw=\"%s\" fmt=\"%s\" kind=%d chg=%zu",
                    (long long)tq,
                    rawHold.c_str(),
                    out.c_str(),
                    (int)tk,
                    (size_t)sig->changes_count);
            }
        }
        return out;
    }
    
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

    /** Mouse X position over plot (for ruler); -1 if not over plot. */
    int m_hoverPlotX = -1;
    /** Sim time under m_hoverPlotX (same formula as ruler); valid when m_hoverPlotX is over plot. */
    long long m_hoverPlotSimTime = 0;
    wxString m_rulerHoverLabel;

    // 添加信号（每次调用就加一次，允许重复）
    void AddDisplaySignal(signal_t* sig)
    {
        if (!sig) return;
        m_displayedSignals2.push_back(sig);
        QueueTraceLoad();
        AssignSignalColors();
        RequestDrawCacheRebuild(true);
        Refresh();
    }

    /** Debounced rebuild; set immediate=true after open file or add signal. */
    void RequestDrawCacheRebuild(bool immediate = false)
    {
        m_hoverSignal = -1;
        m_hoverSegment = -1;
        if (immediate) {
            if (m_cacheDebounceTimer)
                m_cacheDebounceTimer->Stop();
            BuildDrawCacheAsync();
            return;
        }
        if (!m_cacheDebounceTimer)
            return;
        m_cacheDebounceTimer->Start(WaveformPerf::CacheDebounceMs(), wxTIMER_ONE_SHOT);
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

    void SetTraceColor(signal_t* sig, const wxColour& color)
    {
        if (!sig)
            return;
        m_signalColors[sig->signal_id] = color;
        m_manualColorSignalIds.insert(sig->signal_id);
        Refresh();
    }

    static wxColour CyclePaletteColor(int index)
    {
        static const wxColour palette[] = {
            wxColour(220, 50, 50),
            wxColour(230, 120, 40),
            wxColour(220, 200, 50),
            wxColour(60, 170, 70),
            wxColour(40, 160, 200),
            wxColour(50, 90, 220),
            wxColour(180, 60, 200),
            wxColour(120, 80, 200),
        };
        return palette[index % (int)(sizeof(palette) / sizeof(palette[0]))];
    }

    void AssignSignalColors()
    {
        std::unordered_map<std::string, wxColour> preserved;
        for (const std::string& id : m_manualColorSignalIds) {
            auto it = m_signalColors.find(id);
            if (it != m_signalColors.end())
                preserved[id] = it->second;
        }

        m_signalColors.clear();
        std::uniform_int_distribution<int> dist(0, 180);
        
        for (auto sig : m_allSignals)
        {
            auto kept = preserved.find(sig->signal_id);
            if (kept != preserved.end()) {
                m_signalColors[sig->signal_id] = kept->second;
                continue;
            }

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

        long long limitRange = m_limitEnd - m_limitStart;

        if (m_displayTimeRange > limitRange)
            m_displayTimeRange = limitRange;

        if (m_timeOffset < m_limitStart)
            m_timeOffset = m_limitStart;

        if (m_timeOffset + m_displayTimeRange > m_limitEnd)
            m_timeOffset = m_limitEnd - m_displayTimeRange;

        if (m_timeOffset < m_limitStart)
            m_timeOffset = m_limitStart;
    }

    /** 钳位可见时间窗 [offset, offset+range] 到 [0, maxTimestamp] 与 limit 区域。 */
    void ClampTimeView()
    {
        if (m_displayTimeRange < 1)
            m_displayTimeRange = 1;
        if (m_timeOffset < 0)
            m_timeOffset = 0;
        if (m_maxTimestamp > 0) {
            if (m_displayTimeRange > m_maxTimestamp)
                m_displayTimeRange = m_maxTimestamp;
            if (m_timeOffset + m_displayTimeRange > m_maxTimestamp)
                m_timeOffset = m_maxTimestamp - m_displayTimeRange;
            if (m_timeOffset < 0)
                m_timeOffset = 0;
        }
        ClampViewToLimit();
    }
    
    // 获取信号在指定时间的值
    const char* GetSignalValueAt(signal_t* sig, long long timestamp)
    {
        if (!sig || sig->changes_count == 0)
            return "";
        const timestamp_t ts = (timestamp < 0) ? 0 : (timestamp_t)timestamp;
        return vcd_signal_get_value_at_timestamp(sig, ts);
    }

    long long FindNextEdge(long long t)
    {
        long long best = m_maxTimestamp;
        const timestamp_t tt = (t < 0) ? 0 : (timestamp_t)t;
        for (auto sig : m_displayedSignals2) {
            if (!sig || !sig->value_changes || sig->changes_count == 0) continue;
            for (size_t i = 0; i < sig->changes_count; i++) {
                const long long ts = (long long)sig->value_changes[i].timestamp;
                if (sig->value_changes[i].timestamp > tt && ts < best) best = ts;
            }
        }
        return best;
    }

    long long FindPrevEdge(long long t)
    {
        long long best = 0;
        const timestamp_t tt = (t < 0) ? 0 : (timestamp_t)t;
        for (auto sig : m_displayedSignals2) {
            if (!sig || !sig->value_changes || sig->changes_count == 0) continue;
            for (size_t i = 0; i < sig->changes_count; i++) {
                const long long ts = (long long)sig->value_changes[i].timestamp;
                if (sig->value_changes[i].timestamp < tt && ts > best) best = ts;
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
        if (!sig || !sig->value_changes || sig->changes_count < 2) return 0;
        std::vector<long long> risingEdges;
        for (size_t i = 1; i < sig->changes_count; i++)
        {
            char prev = ParseVcdValue(sig->value_changes[i - 1].value);
            char curr = ParseVcdValue(sig->value_changes[i].value);
            if (prev == '0' && curr == '1') risingEdges.push_back((long long)sig->value_changes[i].timestamp);
        }
        if (risingEdges.size() < 2) return 0;
        double period = (double)(risingEdges.back() - risingEdges.front()) / (double)(risingEdges.size() - 1);
        return period <= 0 ? 0 : 1.0 / period;
    }

    double ComputeDuty(signal_t* sig)
    {
        if (!sig || !sig->value_changes || sig->changes_count < 2) return 0;
        double highTime = 0, totalTime = 0;
        for (size_t i = 1; i < sig->changes_count; i++)
        {
            long long t1 = (long long)sig->value_changes[i - 1].timestamp;
            long long t2 = (long long)sig->value_changes[i].timestamp;
            char val = ParseVcdValue(sig->value_changes[i - 1].value);
            if (val == '1') highTime += (double)(t2 - t1);
            totalTime += (double)(t2 - t1);
        }
        return totalTime <= 0 ? 0 : highTime / totalTime;
    }

    long long FindNearestEdge(long long targetTime)
    {
        if (!m_vcdData || m_allSignals.empty()) return targetTime;
        long long best = targetTime;
        long long minDist = LLONG_MAX;
        for (signal_t* sig : m_allSignals)
        {
            if (!sig || sig->changes_count == 0 || !sig->value_changes) continue;
            for (size_t c = 0; c < sig->changes_count; c++)
            {
                long long t = (long long)sig->value_changes[c].timestamp;
                long long d = t > targetTime ? (t - targetTime) : (targetTime - t);
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

        long long lastT = m_markers[0].timestamp;
        for (size_t i = 1; i < m_markers.size(); i++)
        {
            long long t = m_markers[i].timestamp;
            long long delta = t - lastT;
            int x1 = TimeToX(lastT, scale);
            int x2 = TimeToX(t, scale);
            wxString s = wxString::Format("ΔT = %lld", (long long)delta);
            wxSize sz = dc.GetTextExtent(s);
            int tx = (x1 + x2 - sz.x) / 2;
            dc.DrawText(s, tx, yBar - sz.y / 2);
            lastT = t;
        }
    }

    int TimeToX(long long t, double scale) const
    {
        return LEFT_MARGIN + (int)((double)(t - m_timeOffset) * scale);
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
        m_displayedSignals2.clear();
        InitSignalTree();
        RequestDrawCacheRebuild();
        Refresh();
    }

    void ToggleCursorValueDisplay()
    {
        m_showCursorValue = !m_showCursorValue;
        wxLogDebug("[Bear2Wave][Wave] ToggleCursorValueDisplay -> %s playhead=%lld",
            m_showCursorValue ? "ON" : "OFF",
            m_currentTimestamp);
        Refresh();
    }

    char ParseVcdValue(const char* v) const
    {
        if (!v || !v[0]) return '0';
        char c = (char)tolower((unsigned char)v[0]);
        if (c == '0' || c == '1' || c == 'x' || c == 'z') return c;
        if (c == 'l') return '0';
        if (c == 'h') return '1';
        if (c == 'u' || c == 'w' || c == 'd') return 'x';
        return '0';
    }

    std::string ParseBusValue(const char* v) const
    {
        if (!v) return "";
        const unsigned char c0 = (unsigned char)v[0];
        if (c0 == 'b' || c0 == 'B' || c0 == 'r' || c0 == 'R')
            return std::string(v + 1);
        return std::string(v);
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
        RequestDrawCacheRebuild();
        Refresh();
    }

    WaveformPanel(wxWindow* parent)
        : wxGLCanvas(parent), m_vcdData(nullptr), m_currentTimestamp(0),
        m_displayTimeRange(1000), m_maxTimestamp(1000)
    {
        SetBackgroundStyle(wxBG_STYLE_PAINT);
        SetDoubleBuffered(true);
        SetBackgroundColour(wxColour(252, 253, 255));
        /* wxGLCanvas 无 SetFocusable；用 wxWANTS_CHARS 才能收到 PgUp/PgDn 等按键 */
        SetWindowStyleFlag(GetWindowStyleFlag() | wxWANTS_CHARS);
        m_rng.seed(std::random_device{}());

        Bind(wxEVT_PAINT, &WaveformPanel::OnPaint, this);
        Bind(wxEVT_KEY_DOWN, &WaveformPanel::OnKeyDown, this);
        Bind(wxEVT_SIZE, &WaveformPanel::OnResize, this);
        Bind(wxEVT_MOTION, &WaveformPanel::OnMouseMove, this);
        Bind(wxEVT_MOUSEWHEEL, &WaveformPanel::OnMouseWheel, this);
        Bind(wxEVT_LEFT_DOWN, &WaveformPanel::OnMouseDown, this);
        Bind(wxEVT_LEFT_UP, &WaveformPanel::OnMouseUp, this);
        Bind(wxEVT_RIGHT_DOWN, &WaveformPanel::OnRightDown, this);
        Bind(wxEVT_RIGHT_UP, &WaveformPanel::OnRightUp, this);
        Bind(wxEVT_MOUSE_CAPTURE_LOST, &WaveformPanel::OnMouseCaptureLost, this);
        Bind(wxEVT_MENU, &WaveformPanel::OnTraceContextMenu, this);
        m_cacheDebounceTimer = std::make_unique<wxTimer>(this);
        Bind(wxEVT_TIMER, &WaveformPanel::OnCacheDebounceTimer, this, m_cacheDebounceTimer->GetId());
        m_traceLoadDebounceTimer = std::make_unique<wxTimer>(this);
        Bind(wxEVT_TIMER, &WaveformPanel::OnTraceLoadDebounceTimer, this, m_traceLoadDebounceTimer->GetId());
    }

    int HitTestDisplayedSignalRow(int x, int y) const
    {
        if (x < 0 || x >= LEFT_MARGIN || m_displayedSignals2.empty())
            return -1;
        const int timeAxisY = 25;
        const int yOffset = 30;
        const int safeCount = (int)m_displayedSignals2.size();
        for (int i = 0; i < safeCount; ++i) {
            const int yBase = timeAxisY + i * SIGNAL_ROW_HEIGHT + yOffset;
            if (y >= yBase - 20 && y <= yBase + 20)
                return i;
        }
        return -1;
    }

    signal_t* SelectedTraceSignal() const
    {
        if (m_selectedSignalIndex < 0 || m_selectedSignalIndex >= (int)m_displayedSignals2.size())
            return nullptr;
        return m_displayedSignals2[m_selectedSignalIndex];
    }

    void ApplyRadixToContextTargets(DataFormat fmt)
    {
        signal_t* sel = SelectedTraceSignal();
        if (!sel)
            return;
        m_signalDataFormats[sel] = fmt;
        RequestDrawCacheRebuild();
        Refresh();
    }

    void SetTransformFlag(signal_t* sig, int flag, bool on)
    {
        if (!sig)
            return;
        int& f = m_signalTransforms[sig];
        if (on)
            f |= flag;
        else
            f &= ~flag;
        RequestDrawCacheRebuild();
        Refresh();
    }

    void SetGrayLevel(signal_t* sig, int level)
    {
        if (!sig)
            return;
        if (level <= 0)
            m_signalGrayLevel.erase(sig);
        else
            m_signalGrayLevel[sig] = level;
        Refresh();
    }

    static wxColour GrayAdjustedColour(wxColour c, int level)
    {
        if (level <= 0)
            return c;
        const double k = (level == 1) ? 0.35 : (level == 2) ? 0.55 : 0.75;
        const int g = (int)(c.Red() * (1.0 - k) + 128 * k);
        return wxColour(g, g, g);
    }

    wxColour TraceColourForSignal(signal_t* sig, bool searchMatch) const
    {
        if (!sig)
            return ThemeTraceDefault();
        wxColour color;
        if (!m_searchKeyword.empty())
            color = searchMatch ? wxColour(255, 60, 60) : wxColour(180, 180, 180);
        else {
            auto it = m_signalColors.find(sig->signal_id);
            color = (it != m_signalColors.end()) ? it->second : ThemeTraceDefault();
        }
        auto git = m_signalGrayLevel.find(sig);
        if (git != m_signalGrayLevel.end())
            color = GrayAdjustedColour(color, git->second);
        return color;
    }

    std::string ApplyTranslateRules(const std::string& text) const
    {
        if (m_translateRules.empty())
            return text;
        wxString s(text);
        for (const auto& rule : m_translateRules) {
            wxRegEx re(wxString::FromUTF8(rule.first), wxRE_ADVANCED);
            if (re.IsValid())
                re.Replace(&s, wxString::FromUTF8(rule.second), true);
        }
        return s.ToStdString();
    }

    void LoadTranslateFilterFile()
    {
        wxFileDialog dlg(this, "Open translate filter", "", "",
            "Filter files (*.lst;*.txt)|*.lst;*.txt|All (*.*)|*.*",
            wxFD_OPEN | wxFD_FILE_MUST_EXIST);
        if (dlg.ShowModal() != wxID_OK)
            return;
        wxTextFile tf(dlg.GetPath());
        if (!tf.Exists() || !tf.Open())
            return;
        m_translateRules.clear();
        for (size_t li = 0; li < tf.GetLineCount(); ++li) {
            wxString line = tf.GetLine(li);
            line.Trim();
            if (line.empty() || line.StartsWith("#"))
                continue;
            const int eq = line.Find('=');
            const int arrow = line.Find("=>");
            int sep = -1;
            if (eq != wxNOT_FOUND && (arrow == wxNOT_FOUND || eq < arrow))
                sep = eq;
            else if (arrow != wxNOT_FOUND)
                sep = arrow;
            if (sep == wxNOT_FOUND)
                continue;
            wxString from = line.Left(sep).Trim();
            wxString to = line.Mid(sep + (line[sep] == '=' ? 1 : 2)).Trim();
            if (!from.empty())
                m_translateRules.emplace_back(from.ToStdString(), to.ToStdString());
        }
        wxLogMessage("Loaded %zu translate rule(s).", m_translateRules.size());
        RequestDrawCacheRebuild();
        Refresh();
    }

    void ShiftRowCommentsOnInsert(int pos)
    {
        std::map<int, wxString> next;
        for (const auto& kv : m_rowComments) {
            const int k = kv.first >= pos ? kv.first + 1 : kv.first;
            next[k] = kv.second;
        }
        m_rowComments = std::move(next);
    }

    void ShiftRowCommentsOnDelete(int pos)
    {
        std::map<int, wxString> next;
        for (const auto& kv : m_rowComments) {
            if (kv.first == pos)
                continue;
            const int k = kv.first > pos ? kv.first - 1 : kv.first;
            next[k] = kv.second;
        }
        m_rowComments = std::move(next);
    }

    void ShowTraceContextMenu(const wxPoint& clientPos)
    {
        wxMenu menu;
        SignalTraceMenu::Build(menu);
        PopupMenu(&menu, clientPos);
    }

    void OnTraceContextMenu(wxCommandEvent& ev)
    {
        const int id = ev.GetId();
        if (id < SignalTraceMenu::ID_HEX || id > SignalTraceMenu::ID_OPEN_SCOPE) {
            ev.Skip();
            return;
        }
        signal_t* sig = SelectedTraceSignal();

        switch (id) {
        case SignalTraceMenu::ID_HEX:
            ApplyRadixToContextTargets(FORMAT_HEXADECIMAL);
            return;
        case SignalTraceMenu::ID_DECIMAL:
            ApplyRadixToContextTargets(FORMAT_DECIMAL);
            return;
        case SignalTraceMenu::ID_SIGNED_DECIMAL:
            ApplyRadixToContextTargets(FORMAT_SIGNED_DECIMAL);
            return;
        case SignalTraceMenu::ID_BINARY:
            ApplyRadixToContextTargets(FORMAT_BINARY);
            return;
        case SignalTraceMenu::ID_OCTAL:
            ApplyRadixToContextTargets(FORMAT_OCTAL);
            return;
        case SignalTraceMenu::ID_ASCII:
            ApplyRadixToContextTargets(FORMAT_ASCII);
            return;
        case SignalTraceMenu::ID_TIME:
            ApplyRadixToContextTargets(FORMAT_TIME);
            return;
        case SignalTraceMenu::ID_ENUM:
            ApplyRadixToContextTargets(FORMAT_ENUM);
            return;
        case SignalTraceMenu::ID_BITS_TO_REAL:
            ApplyRadixToContextTargets(FORMAT_REAL);
            return;
        case SignalTraceMenu::ID_REAL_TO_BITS:
            ApplyRadixToContextTargets(FORMAT_BINARY);
            return;
        case SignalTraceMenu::ID_RJ_ON:
            SetTransformFlag(sig, TR_RIGHT_JUSTIFY, true);
            return;
        case SignalTraceMenu::ID_RJ_OFF:
            SetTransformFlag(sig, TR_RIGHT_JUSTIFY, false);
            return;
        case SignalTraceMenu::ID_INVERT_ON:
            SetTransformFlag(sig, TR_INVERT, true);
            return;
        case SignalTraceMenu::ID_INVERT_OFF:
            SetTransformFlag(sig, TR_INVERT, false);
            return;
        case SignalTraceMenu::ID_REVERSE_ON:
            SetTransformFlag(sig, TR_REVERSE_BITS, true);
            return;
        case SignalTraceMenu::ID_REVERSE_OFF:
            SetTransformFlag(sig, TR_REVERSE_BITS, false);
            return;
        case SignalTraceMenu::ID_POPCNT:
            SetTransformFlag(sig, TR_POPCNT, true);
            return;
        case SignalTraceMenu::ID_COLOR_NORMAL:
            if (sig) {
                m_manualColorSignalIds.erase(sig->signal_id);
                AssignSignalColors();
            }
            return;
        case SignalTraceMenu::ID_COLOR_RED:
            SetTraceColor(sig, wxColour(220, 50, 50));
            return;
        case SignalTraceMenu::ID_COLOR_ORANGE:
            SetTraceColor(sig, wxColour(230, 120, 40));
            return;
        case SignalTraceMenu::ID_COLOR_YELLOW:
            SetTraceColor(sig, wxColour(220, 200, 50));
            return;
        case SignalTraceMenu::ID_COLOR_GREEN:
            SetTraceColor(sig, wxColour(60, 170, 70));
            return;
        case SignalTraceMenu::ID_COLOR_CYAN:
            SetTraceColor(sig, wxColour(40, 160, 200));
            return;
        case SignalTraceMenu::ID_COLOR_BLUE:
            SetTraceColor(sig, wxColour(50, 90, 220));
            return;
        case SignalTraceMenu::ID_COLOR_MAGENTA:
            SetTraceColor(sig, wxColour(180, 60, 200));
            return;
        case SignalTraceMenu::ID_COLOR_VIOLET:
            SetTraceColor(sig, wxColour(120, 80, 200));
            return;
        case SignalTraceMenu::ID_COLOR_GRAY:
            SetTraceColor(sig, wxColour(140, 140, 140));
            return;
        case SignalTraceMenu::ID_COLOR_WHITE:
            SetTraceColor(sig, wxColour(250, 250, 250));
            return;
        case SignalTraceMenu::ID_COLOR_BLACK:
            SetTraceColor(sig, wxColour(30, 30, 30));
            return;
        case SignalTraceMenu::ID_COLOR_CYCLE:
            if (sig)
                SetTraceColor(sig, CyclePaletteColor(m_cycleColorIndex++));
            return;
        case SignalTraceMenu::ID_INSERT_ANALOG_EXT: {
            wxString heightStr = wxGetTextFromUser(
                "Analog height extension (0-100):",
                "Insert Analog Height Extension",
                wxString::Format("%d", m_analogHeightExtension),
                this);
            long height = 0;
            if (!heightStr.IsEmpty() && heightStr.ToLong(&height) && height >= 0 && height <= 100) {
                m_analogHeightExtension = (int)height;
                Refresh();
            }
            return;
        }
        case SignalTraceMenu::ID_INSERT_BLANK: {
            int pos = m_selectedSignalIndex >= 0 ? m_selectedSignalIndex : (int)m_displayedSignals2.size();
            m_displayedSignals2.insert(m_displayedSignals2.begin() + pos, nullptr);
            RequestDrawCacheRebuild();
            Refresh();
            return;
        }
        case SignalTraceMenu::ID_INSERT_COMMENT: {
            wxString text = wxGetTextFromUser("Comment text:", "Insert Comment", "", this);
            if (!text.IsEmpty()) {
                const int pos = m_selectedSignalIndex >= 0 ? m_selectedSignalIndex : (int)m_displayedSignals2.size();
                ShiftRowCommentsOnInsert(pos);
                m_displayedSignals2.insert(m_displayedSignals2.begin() + pos, nullptr);
                m_rowComments[pos] = text;
                RequestDrawCacheRebuild();
                Refresh();
            }
            return;
        }
        case SignalTraceMenu::ID_ALIAS_TRACE:
            if (sig) {
                wxString cur;
                auto it = m_signalAliases.find(sig);
                if (it != m_signalAliases.end())
                    cur = it->second;
                wxString alias = wxGetTextFromUser("Alias:", "Alias Highlighted Trace", cur, this);
                if (!alias.IsEmpty()) {
                    m_signalAliases[sig] = alias;
                    Refresh();
                }
            }
            return;
        case SignalTraceMenu::ID_REMOVE_ALIASES:
            if (sig)
                m_signalAliases.erase(sig);
            Refresh();
            return;
        case SignalTraceMenu::ID_CUT:
            if (m_selectedSignalIndex >= 0 && m_selectedSignalIndex < (int)m_displayedSignals2.size()) {
                if (sig) {
                    m_clipboard.clear();
                    m_clipboard.push_back(sig);
                }
                ShiftRowCommentsOnDelete(m_selectedSignalIndex);
                m_rowComments.erase(m_selectedSignalIndex);
                m_displayedSignals2.erase(m_displayedSignals2.begin() + m_selectedSignalIndex);
                m_selectedSignalIndex = -1;
                RequestDrawCacheRebuild();
                Refresh();
            }
            return;
        case SignalTraceMenu::ID_COPY:
            if (sig) {
                m_clipboard.clear();
                m_clipboard.push_back(sig);
            }
            return;
        case SignalTraceMenu::ID_PASTE:
            if (!m_clipboard.empty()) {
                int pos = m_selectedSignalIndex >= 0 ? m_selectedSignalIndex + 1 : (int)m_displayedSignals2.size();
                for (signal_t* s : m_clipboard) {
                    if (s)
                        m_displayedSignals2.insert(m_displayedSignals2.begin() + pos++, s);
                }
                RequestDrawCacheRebuild();
                Refresh();
            }
            return;
        case SignalTraceMenu::ID_DELETE:
            if (m_selectedSignalIndex >= 0 && m_selectedSignalIndex < (int)m_displayedSignals2.size()) {
                ShiftRowCommentsOnDelete(m_selectedSignalIndex);
                m_rowComments.erase(m_selectedSignalIndex);
                m_displayedSignals2.erase(m_displayedSignals2.begin() + m_selectedSignalIndex);
                m_selectedSignalIndex = -1;
                RequestDrawCacheRebuild();
                Refresh();
            }
            return;
        case SignalTraceMenu::ID_OPEN_SCOPE:
            if (sig && m_onOpenScope)
                m_onOpenScope(sig);
            return;
        case SignalTraceMenu::ID_TRANSLATE_FILE:
            LoadTranslateFilterFile();
            return;
        case SignalTraceMenu::ID_TRANSLATE_PROC: {
            wxString rule = wxGetTextFromUser(
                "Enter pattern=>replacement (one rule):", "Translate Filter Process", "", this);
            if (rule.empty())
                return;
            const int arrow = rule.Find("=>");
            const int eq = rule.Find('=');
            int sep = -1;
            if (arrow != wxNOT_FOUND)
                sep = arrow;
            else if (eq != wxNOT_FOUND)
                sep = eq;
            if (sep == wxNOT_FOUND) {
                wxLogWarning("Use format: pattern=>replacement");
                return;
            }
            wxString from = rule.Left(sep).Trim();
            wxString to = rule.Mid(sep + (rule[sep] == '=' ? 1 : 2)).Trim();
            if (!from.empty())
                m_translateRules.emplace_back(from.ToStdString(), to.ToStdString());
            RequestDrawCacheRebuild();
            Refresh();
            return;
        }
        case SignalTraceMenu::ID_TRANSACTION_PROC:
            ApplyRadixToContextTargets(FORMAT_ASCII);
            if (sig && ClassifyTraceKind(sig) == WaveTraceKind::TextString)
                wxLogMessage("Transaction filter: showing as text/trace events.");
            else
                wxLogMessage("Transaction filter: applied ASCII format (best for string/event traces).");
            return;
        case SignalTraceMenu::ID_ANALOG_FMT:
            ApplyRadixToContextTargets(FORMAT_REAL);
            return;
        case SignalTraceMenu::ID_RANGE_FILL_ON:
            SetTransformFlag(sig, TR_RANGE_FILL, true);
            return;
        case SignalTraceMenu::ID_RANGE_FILL_OFF:
            SetTransformFlag(sig, TR_RANGE_FILL, false);
            return;
        case SignalTraceMenu::ID_GRAY_OFF:
            SetGrayLevel(sig, 0);
            return;
        case SignalTraceMenu::ID_GRAY_LIGHT:
            SetGrayLevel(sig, 1);
            return;
        case SignalTraceMenu::ID_GRAY_MEDIUM:
            SetGrayLevel(sig, 2);
            return;
        case SignalTraceMenu::ID_GRAY_STRONG:
            SetGrayLevel(sig, 3);
            return;
        case SignalTraceMenu::ID_FIXED_POINT: {
            int cur = 0;
            auto it = m_fixedPointShift.find(sig);
            if (it != m_fixedPointShift.end())
                cur = it->second;
            wxString s = wxGetTextFromUser(
                "Arithmetic right-shift for display (negative = left):",
                "Fixed Point Shift",
                wxString::Format("%d", cur),
                this);
            long v = 0;
            if (!s.empty() && s.ToLong(&v)) {
                if (v == 0)
                    m_fixedPointShift.erase(sig);
                else
                    m_fixedPointShift[sig] = (int)v;
                RequestDrawCacheRebuild();
                Refresh();
            }
            return;
        }
        default:
            break;
        }
    }

    ~WaveformPanel()
    {
        EndMouseInteraction(false);
        CancelTraceLoad();
        JoinTraceWorkerIfJoined();
        JoinCacheWorkerIfJoined();
        if (m_vcdData) {
            vcd_free(m_vcdData);
            m_vcdData = nullptr;
        }
        if (m_signalTreeRoot) FreeSignalTree(m_signalTreeRoot);
    }

    void OnRightDown(wxMouseEvent& event)
    {
        const int x = event.GetX();
        const int y = event.GetY();
        if (x < LEFT_MARGIN) {
            const int row = HitTestDisplayedSignalRow(x, y);
            if (row >= 0) {
                m_selectedSignalIndex = row;
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

    void OnRightUp(wxMouseEvent& event)
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
        ClampViewToLimit();
        if (m_displayTimeRange < 10) m_displayTimeRange = 10;
        RequestDrawCacheRebuild();
        Refresh();
    }
    
    // 添加标记
    void AddMarker(long long timestamp, const wxString& label)
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
        long long currentTime = GetCursorSimTime();
        long long newTime = currentTime;
        
        // 实现真正的边沿检测
        if (!m_displayedSignals2.empty()) {
            // 选择第一个信号来检测边沿
            signal_t* sig = m_displayedSignals2[0];
            if (sig && sig->changes_count > 0) {
                // 向前搜索边沿
                for (int i = (int)sig->changes_count - 1; i >= 0; i--) {
                    if ((long long)sig->value_changes[i].timestamp < currentTime) {
                        newTime = (long long)sig->value_changes[i].timestamp;
                        break;
                    }
                }
            }
        }
        
        // 确保时间在有效范围内
        newTime = std::max(0LL, newTime);
        
        SetCursorSimTime(newTime);

        wxMessageBox(wxString::Format("Found previous edge at time %lld.", newTime));
    }
    
    // 查找下一个边沿
    void FindNextEdge()
    {
        // 查找下一个边沿
        long long currentTime = GetCursorSimTime();
        long long newTime = currentTime;
        
        // 实现真正的边沿检测
        if (!m_displayedSignals2.empty()) {
            // 选择第一个信号来检测边沿
            signal_t* sig = m_displayedSignals2[0];
            if (sig && sig->changes_count > 0) {
                // 向后搜索边沿
                for (int i = 0; i < (int)sig->changes_count; i++) {
                    if ((long long)sig->value_changes[i].timestamp > currentTime) {
                        newTime = (long long)sig->value_changes[i].timestamp;
                        break;
                    }
                }
            }
        }
        
        // 确保时间在有效范围内
        if (m_maxTimestamp > 0) {
            newTime = std::min(m_maxTimestamp, newTime);
        }
        
        SetCursorSimTime(newTime);

        wxMessageBox(wxString::Format("Found next edge at time %lld.", newTime));
    }
    
    long long GetCursorSimTime() const { return m_currentTimestamp; }
    
    // 设置当前时间
    void SetCursorSimTime(long long timestamp) { SetCurrentTimestamp(timestamp, true); }

    void OnMouseDown(wxMouseEvent& event)
    {
        if (m_minimapRect.Contains(event.GetPosition()))
        {
            m_hoverSignal = -1;
            m_hoverSegment = -1;
            int mx = event.GetX();
            const int mmW = m_minimapRect.width;
            if (mmW <= 0)
                return;
            double ratio = (double)(mx - m_minimapRect.x) / (double)mmW;
            long long newCenterTime = (long long)(ratio * (double)m_maxTimestamp);
            m_timeOffset = newCenterTime - m_displayTimeRange / 2;
            ClampViewToLimit();
            if (m_timeOffset < 0) m_timeOffset = 0;
            if (m_timeOffset > m_maxTimestamp - m_displayTimeRange)
                m_timeOffset = m_maxTimestamp - m_displayTimeRange;
            RequestDrawCacheRebuild();
            m_draggingMinimap = true;
            CaptureMouse();
            Refresh();
            return;
        }

        int x = event.GetX();
        int y = event.GetY();
        
        if (x < LEFT_MARGIN) {
            const int row = HitTestDisplayedSignalRow(x, y);
            if (row >= 0) {
                m_selectedSignalIndex = row;
                Refresh();
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
            long long timeAtCursor = m_timeOffset + (long long)((x - LEFT_MARGIN) / scale);
            timeAtCursor = FindNearestEdge(timeAtCursor);
            Marker mk;
            mk.timestamp = timeAtCursor;
            mk.label = wxString::Format("M%d", (int)m_markers.size());
            m_markers.push_back(mk);
            Refresh();
            return;
        }

        if (!event.LeftDown())
            return;

        int plotRight = GetClientSize().GetWidth() - WAVE_PADDING;
        if (x < LEFT_MARGIN || x >= plotRight || m_displayTimeRange <= 0)
            return;

        double scale = (double)(GetSize().x - LEFT_MARGIN - WAVE_PADDING) / (double)m_displayTimeRange;
        long long t = m_timeOffset + (long long)((x - LEFT_MARGIN) / scale);
        if (m_maxTimestamp > 0)
            t = std::max(0LL, std::min(t, m_maxTimestamp));

        /* Ctrl+左键：红色测量条（A/B），不拖动播放头 */
        if (event.ControlDown()) {
            m_measuringDrag = true;
            m_scrubbingPlayhead = false;
            if (!m_hasMarkerA) {
                m_markerA = t;
                m_markerB = t;
                m_hasMarkerA = true;
            } else {
                m_markerB = t;
            }
            m_isMeasuring = true;
            CaptureMouse();
            Refresh();
            return;
        }

        /* 普通左键：拖动播放头（光标值标签 + 底栏滑块） */
        m_scrubbingPlayhead = true;
        m_measuringDrag = false;
        m_lastMouseX = x;
        SetFocus();
        SetCurrentTimestamp(t, true);
        CaptureMouse();
    }

    void OnKeyDown(wxKeyEvent& event)
    {
        switch (event.GetKeyCode()) {
        case WXK_PAGEUP:
            PageLeft();
            return;
        case WXK_PAGEDOWN:
            PageRight();
            return;
        default:
            break;
        }
        event.Skip();
    }

    /** 结束拖动/框选等；captureLost 为 true 时勿再调用 ReleaseMouse()（wx 已收回捕获）。 */
    void EndMouseInteraction(bool releaseCapture)
    {
        const bool wasPanning = m_isDragging;
        const bool wasMinimap = m_draggingMinimap;

        m_hoverSignal = -1;
        m_hoverSegment = -1;
        m_isDragging = false;
        m_scrubbingPlayhead = false;
        m_measuringDrag = false;
        m_draggingMarkerIndex = -1;
        m_draggingMinimap = false;
        m_isSelecting = false;

        if (releaseCapture && HasCapture())
            ReleaseMouse();

        if (wasPanning || wasMinimap)
            EmitTimeViewChanged();

        Refresh(false);
    }

    void OnMouseCaptureLost(wxMouseCaptureLostEvent& event)
    {
        (void)event;
        EndMouseInteraction(false);
    }

    void OnMouseUp(wxMouseEvent&)
    {
        EndMouseInteraction(true);
    }

    void OnMouseWheel(wxMouseEvent& event)
    {
        if (m_alternateWheelMode) {
            // 控制时间
            long long currentTime = GetCursorSimTime();
            long long delta = event.GetWheelRotation() > 0 ? -10 : 10;
            long long newTime = currentTime + delta;
            newTime = std::max(0LL, newTime);
            if (m_maxTimestamp > 0) {
                newTime = std::min(m_maxTimestamp, newTime);
            }
            SetCurrentTimestamp(newTime, true);
        } else {
            // 控制缩放
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

            RequestDrawCacheRebuild();
            Refresh();
            EmitTimeViewChanged();
        }
    }

    void OnMouseMove(wxMouseEvent& event)
    {
        if (m_draggingMinimap)
        {
            m_hoverSignal = -1;
            m_hoverSegment = -1;
            int mx = event.GetX();
            const int mmW = m_minimapRect.width;
            if (mmW <= 0)
                return;
            double ratio = (double)(mx - m_minimapRect.x) / (double)mmW;
            long long newCenterTime = (long long)(ratio * (double)m_maxTimestamp);
            m_timeOffset = newCenterTime - m_displayTimeRange / 2;
            if (m_timeOffset < 0) m_timeOffset = 0;
            if (m_timeOffset > m_maxTimestamp - m_displayTimeRange)
                m_timeOffset = m_maxTimestamp - m_displayTimeRange;
            RequestDrawCacheRebuild();
            Refresh();
            return;
        }

        int mx = event.GetX(), my = event.GetY();
        if (m_measuringDrag && event.LeftIsDown() && m_displayTimeRange > 0) {
            int viewW = GetSize().x - LEFT_MARGIN - WAVE_PADDING;
            if (viewW < 1) viewW = 1;
            double scale = (double)viewW / (double)m_displayTimeRange;
            int plotRight = GetClientSize().GetWidth() - WAVE_PADDING;
            int clampedX = std::max(LEFT_MARGIN, std::min(mx, plotRight - 1));
            long long t = m_timeOffset + (long long)((clampedX - LEFT_MARGIN) / scale);
            if (m_maxTimestamp > 0)
                t = std::max(0LL, std::min(t, m_maxTimestamp));
            m_markerB = t;
            m_isMeasuring = true;
            Refresh();
            return;
        }
        if (m_scrubbingPlayhead && event.LeftIsDown() && m_displayTimeRange > 0) {
            int viewW = GetSize().x - LEFT_MARGIN - WAVE_PADDING;
            if (viewW < 1) viewW = 1;
            double scale = (double)viewW / (double)m_displayTimeRange;
            int plotRight = GetClientSize().GetWidth() - WAVE_PADDING;
            int clampedX = std::max(LEFT_MARGIN, std::min(mx, plotRight - 1));
            long long t = m_timeOffset + (long long)((clampedX - LEFT_MARGIN) / scale);
            if (WaveDebugVerbose()) {
                static unsigned s_scrubN = 0;
                if ((++s_scrubN % 8u) == 0u) {
                    wxLogDebug(
                        "[Bear2Wave][Scrub] mx=%d xCl=%d t=%lld off=%lld range=%lld maxTs=%lld scale=%.8f",
                        mx,
                        clampedX,
                        t,
                        m_timeOffset,
                        m_displayTimeRange,
                        m_maxTimestamp,
                        scale);
                }
            }
            SetCurrentTimestamp(t, true);
            return;
        }
        int viewW = GetSize().x - LEFT_MARGIN - WAVE_PADDING;
        if (viewW < 1) viewW = 1;
        double scale = (double)viewW / m_displayTimeRange;

        if (m_draggingMarkerIndex >= 0 && event.ShiftDown() && !m_markersLocked)
        {
            long long newTime = m_timeOffset + (long long)((mx - LEFT_MARGIN) / scale);
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

        /* 无按键拖动时：Ctrl 悬停可预览 B 点；松开 Ctrl 且未在测量拖动则清除测量条 */
        if (event.ControlDown()) {
            long long timeAtCursor = m_timeOffset + (long long)((mx - LEFT_MARGIN) / scale);
            if (m_maxTimestamp > 0)
                timeAtCursor = std::max(0LL, std::min(timeAtCursor, m_maxTimestamp));
            if (m_hasMarkerA && !m_measuringDrag && !event.LeftIsDown())
                m_markerB = timeAtCursor;
            Refresh();
        } else if (!m_measuringDrag && !event.LeftIsDown()) {
            if (m_hasMarkerA || m_isMeasuring) {
                m_hasMarkerA = false;
                m_isMeasuring = false;
                m_markerA = -1;
                m_markerB = -1;
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
            RequestDrawCacheRebuild();
            Refresh();
            return;
        }

        const int prevMk = m_hoverMarkerIndex;
        const int prevSig = m_hoverSignal;
        const int prevSeg = m_hoverSegment;
        const wxString prevRuler = m_rulerHoverLabel;
        const int prevPlotX = m_hoverPlotX;

        m_hoverMarkerIndex = -1;
        for (size_t i = 0; i < m_markers.size(); i++)
        {
            int x = TimeToX(m_markers[i].timestamp, scale);
            if (abs(mx - x) < 5) { m_hoverMarkerIndex = (int)i; break; }
        }

        m_hoverSignal = -1;
        m_hoverSegment = -1;
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        int safeCount = std::min((int)m_displayedSignals2.size(), (int)m_cachedSegments.size());
        bool hitSeg = false;
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
                else {
                    int yCurr = (seg.value == '0') ? yL : yH;
                    rect = wxRect(seg.x1, yCurr - 6, seg.x2 - seg.x1, 12);
                }
                if (rect.Contains(mx, my)) {
                    m_hoverSignal = i;
                    m_hoverSegment = k;
                    hitSeg = true;
                    break;
                }
            }
            if (hitSeg) break;
        }

        wxSize cs = GetClientSize();
        if (mx >= LEFT_MARGIN && mx < cs.GetWidth() - WAVE_PADDING && m_displayTimeRange > 0) {
            m_hoverPlotX = mx;
            m_hoverPlotSimTime = m_timeOffset + (long long)((mx - LEFT_MARGIN) / scale);
            if (m_maxTimestamp > 0)
                m_hoverPlotSimTime = std::max(0LL, std::min(m_maxTimestamp, m_hoverPlotSimTime));
            m_rulerHoverLabel = FormatTimeLabelFromSimTime(m_hoverPlotSimTime);
        } else {
            m_hoverPlotX = -1;
            m_rulerHoverLabel.clear();
        }

        if (prevMk != m_hoverMarkerIndex || prevSig != m_hoverSignal || prevSeg != m_hoverSegment
            || prevRuler != m_rulerHoverLabel || prevPlotX != m_hoverPlotX)
            Refresh(false);
    }

    /** Format VCD/FST value for display (b/o/d/h/scalar → target radix). */
    std::string FormatValueByDataFormat(
        const char* value,
        DataFormat format,
        int signalWidth = 0,
        signal_t* sig = nullptr,
        long long simTime = -1) const
    {
        if (!value)
            return "";

        if (format == FORMAT_TIME && simTime >= 0)
            return FormatTimeLabel((double)simTime).ToStdString();

        if (format == FORMAT_ENUM)
            return ParseBusValue(value);

        WaveformRadix::DecodedValue d = WaveformRadix::DecodeVcdLiteral(value, signalWidth);
        if (!d.valid)
            return value;

        std::string bits = d.bits;
        int tf = 0;
        if (sig) {
            auto it = m_signalTransforms.find(sig);
            if (it != m_signalTransforms.end())
                tf = it->second;
        }

        if (tf & TR_POPCNT) {
            if (!WaveformRadix::BitsHaveNon01(bits)) {
                int cnt = 0;
                for (char c : bits)
                    if (c == '1')
                        ++cnt;
                return std::to_string(cnt);
            }
            return bits;
        }

        if (tf & TR_REVERSE_BITS)
            std::reverse(bits.begin(), bits.end());
        if (tf & TR_INVERT) {
            for (char& c : bits) {
                if (c == '0')
                    c = '1';
                else if (c == '1')
                    c = '0';
            }
        }
        if (tf & TR_RIGHT_JUSTIFY) {
            const int w = signalWidth > 0 ? signalWidth : (int)bits.size();
            WaveformRadix::PadBitsLeft(bits, w);
        }

        if (sig) {
            auto sh = m_fixedPointShift.find(sig);
            if (sh != m_fixedPointShift.end() && sh->second != 0 && !bits.empty()
                && WaveformRadix::BitsHaveNon01(bits) == false) {
                unsigned long val = 0;
                for (char c : bits) {
                    val <<= 1;
                    if (c == '1')
                        val |= 1;
                }
                const int shift = sh->second;
                if (shift > 0)
                    val >>= (unsigned)shift;
                else {
                    const unsigned s = (unsigned)(-shift);
                    val <<= s;
                }
                bits.clear();
                if (val == 0) {
                    bits = "0";
                } else {
                    while (val) {
                        bits.insert(bits.begin(), (val & 1) ? '1' : '0');
                        val >>= 1;
                    }
                }
            }
        }

        std::string vcdBits = "b";
        vcdBits += bits;
        std::string out = WaveformRadix::FormatValue(vcdBits.c_str(), ToWaveformRadix(format), signalWidth);
        return ApplyTranslateRules(out);
    }

    void BuildDrawCacheAsync()
    {
        JoinCacheWorkerIfJoined();
        if (m_allSignals.empty()) {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            m_cachedSegments.clear();
            return;
        }
        if (m_displayedSignals2.empty()) {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            m_cachedSegments.clear();
            return;
        }

        // 强制获取最新、有效的面板尺寸
        wxSize size = GetClientSize();
        int viewW = size.GetWidth() - LEFT_MARGIN - WAVE_PADDING;
        // 确保viewW至少为100，避免缓存为空
        viewW = std::max(viewW, 100);
        long long range = m_displayTimeRange;
        if (range < 1)
            range = 1;
        const double scale = (double)viewW / (double)range;
        const long long offset = m_timeOffset;
        auto signals = m_displayedSignals2;

        std::map<signal_t*, DataFormat> signalDataFormats = m_signalDataFormats;

        std::vector<std::vector<DrawSegment>> priorCache;
        long long priorOffset = LLONG_MIN;
        bool tryPanShift = false;
        int panDeltaX = 0;
        {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            priorOffset = m_cacheKeyOffset;
            if (!m_cachedSegments.empty() && priorOffset != LLONG_MIN
                && m_cacheKeyRange == range && m_cacheKeyViewW == viewW) {
                const long long deltaT = offset - priorOffset;
                panDeltaX = (int)((double)deltaT * scale);
                const int maxShift = viewW + 400;
                if (panDeltaX != 0 && std::abs(panDeltaX) <= maxShift) {
                    tryPanShift = true;
                    priorCache = m_cachedSegments;
                }
            }
        }

        const uint64_t buildEpoch = ++m_cacheBuildEpoch;
        const long long cacheKeyOffset = offset;
        const long long cacheKeyRange = range;
        const int cacheKeyViewW = viewW;
        m_cacheBuildThread = std::thread([this, buildEpoch, signals, signalDataFormats, viewW, range, offset, scale,
                                          tryPanShift, panDeltaX, priorCache = std::move(priorCache),
                                          cacheKeyOffset, cacheKeyRange, cacheKeyViewW]() {
            const long long visibleStart = offset;
            const long long visibleEnd = offset + range;

            if (tryPanShift && priorCache.size() == signals.size()) {
                std::vector<std::vector<DrawSegment>> shifted;
                shifted.reserve(priorCache.size());
                const int clipL = LEFT_MARGIN - 64;
                const int clipR = LEFT_MARGIN + viewW + 64;
                for (const auto& row : priorCache) {
                    std::vector<DrawSegment> segs;
                    segs.reserve(row.size());
                    for (const DrawSegment& seg : row) {
                        DrawSegment s = seg;
                        s.x1 -= panDeltaX;
                        s.x2 -= panDeltaX;
                        if (s.x2 < clipL || s.x1 > clipR)
                            continue;
                        segs.push_back(std::move(s));
                    }
                    shifted.push_back(std::move(segs));
                }
                std::lock_guard<std::mutex> lock2(m_cacheMutex);
                if (buildEpoch != m_cacheBuildEpoch)
                    return;
                m_cachedSegments = std::move(shifted);
                m_cacheKeyOffset = cacheKeyOffset;
                m_cacheKeyRange = cacheKeyRange;
                m_cacheKeyViewW = cacheKeyViewW;
                CallAfter([this]() { Refresh(); });
                return;
            }

            std::vector<std::vector<DrawSegment>> newCache;
            newCache.resize(signals.size());

            const size_t maxSeg = (size_t)WaveformPerf::MaxDrawSegments();

            const auto buildRow = [&](size_t i) {
                signal_t* sig = signals[i];
                std::vector<DrawSegment> segments;
                if (!sig || sig->changes_count == 0 || !sig->value_changes)
                {
                    newCache[i] = {};
                    return;
                }
                vcd_ensure_signal_sorted(sig);

                int yBase = 25 + (int)i * SIGNAL_ROW_HEIGHT + 30;
                int yH = yBase - 15, yL = yBase + 15;
                auto* vec = sig->value_changes;
                size_t count = sig->changes_count;

                WaveTraceKind tk = ClassifyTraceKind(sig);
                if (tk == WaveTraceKind::RealAnalog) {
                    bool any = false;
                    for (size_t j = 0; j < count; j++) {
                        double tmp;
                        if (ParseTraceDouble(vec[j].value, &tmp)) { any = true; break; }
                    }
                    if (!any) tk = WaveTraceKind::DigitalScalar;
                }

                DataFormat format = FORMAT_BINARY;
                auto formatIt = signalDataFormats.find(sig);
                if (formatIt != signalDataFormats.end())
                    format = formatIt->second;

                size_t c;
                for (c = 0; c < count; c++) {
                    if ((long long)vec[c].timestamp >= visibleStart)
                        break;
                }

                const size_t pixelBudget = (size_t)std::max(256, viewW * 4);
                const size_t step = std::max((size_t)1, count / std::min(maxSeg, pixelBudget));

                if (tk == WaveTraceKind::RealAnalog) {
                    double vmin = std::numeric_limits<double>::infinity();
                    double vmax = -std::numeric_limits<double>::infinity();
                    auto consider = [&](const char* v) {
                        double d;
                        if (ParseTraceDouble(v, &d)) {
                            vmin = std::min(vmin, d);
                            vmax = std::max(vmax, d);
                        }
                    };
                    if (c > 0) consider(vec[c - 1].value);
                    for (size_t j = c; j < count; j++) {
                        const long long ts = (long long)vec[j].timestamp;
                        if (ts > visibleEnd) break;
                        if (ts >= visibleStart) consider(vec[j].value);
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
                        if (ParseTraceDouble(vec[k].value, &d)) {
                            lastD = d;
                            lastText = vec[k].value;
                            haveD = true;
                        }
                    }
                    if (!haveD && count > 0) {
                        double d;
                        if (ParseTraceDouble(vec[0].value, &d)) {
                            lastD = d;
                            lastText = vec[0].value;
                        }
                    }
                    int lastY = mapY(lastD);

                    int lastDrawX = LEFT_MARGIN;
                    int lastPixelX = -1;
                    for (; c < count; c += step) {
                        const long long ts = (long long)vec[c].timestamp;
                        if (ts > visibleEnd) break;

                        int currX = LEFT_MARGIN + (int)((double)(ts - offset) * scale);
                        if (currX == lastPixelX) continue;
                        lastPixelX = currX;
                        if (currX < lastDrawX) continue;

                        DrawSegment seg;
                        seg.x1 = lastDrawX;
                        seg.x2 = currX;
                        seg.traceKind = WaveTraceKind::RealAnalog;
                        seg.y = lastY;
                        seg.value = 'r';
                        seg.text = lastText;
                        segments.push_back(seg);

                        lastDrawX = currX;
                        if (ParseTraceDouble(vec[c].value, &lastD))
                            lastText = vec[c].value;
                        lastY = mapY(lastD);
                    }

                    int endX = LEFT_MARGIN + (int)((double)(visibleEnd - offset) * scale);
                    if (endX > lastDrawX) {
                        DrawSegment seg;
                        seg.x1 = lastDrawX;
                        seg.x2 = endX;
                        seg.traceKind = WaveTraceKind::RealAnalog;
                        seg.y = lastY;
                        seg.value = 'r';
                        seg.text = lastText;
                        segments.push_back(seg);
                    }

                    newCache[i] = std::move(segments);
                    return;
                }

                const bool isBus = (tk == WaveTraceKind::BusBits || tk == WaveTraceKind::TextString);

                char lastVal = '0';
                std::string lastText;
                int lastDrawX = LEFT_MARGIN;

                const int sigW = (int)sig->size;
                if (c > 0 && c - 1 < count) {
                    lastVal = ParseVcdValue(vec[c - 1].value);
                    lastText = FormatValueByDataFormat(
                        vec[c - 1].value, format, sigW, sig, (long long)vec[c - 1].timestamp);
                }
                else if (count > 0) {
                    lastVal = ParseVcdValue(vec[0].value);
                    lastText = FormatValueByDataFormat(vec[0].value, format, sigW, sig, (long long)vec[0].timestamp);
                }

                lastDrawX = LEFT_MARGIN;
                int lastPixelX = -1;

                for (; c < count; c += step)
                {
                    const long long ts = (long long)vec[c].timestamp;
                    if (ts > visibleEnd) break;

                    int currX = LEFT_MARGIN + (int)((double)(ts - offset) * scale);

                    if (currX == lastPixelX) continue;
                    lastPixelX = currX;

                    if (currX < lastDrawX) continue;

                    DrawSegment seg;
                    seg.x1 = lastDrawX;
                    seg.x2 = currX;
                    seg.traceKind = isBus ? tk : WaveTraceKind::DigitalScalar;
                    seg.value = isBus ? 'b' : lastVal;
                    seg.y = isBus ? yBase : (lastVal == '0' ? yL : yH);
                    seg.text = lastText;
                    segments.push_back(seg);

                    lastDrawX = currX;
                    lastVal = ParseVcdValue(vec[c].value);
                    lastText = FormatValueByDataFormat(
                        vec[c].value, format, sigW, sig, (long long)vec[c].timestamp);
                }

                int endX = LEFT_MARGIN + (int)((double)(visibleEnd - offset) * scale);
                if (endX > lastDrawX) {
                    DrawSegment seg;
                    seg.x1 = lastDrawX;
                    seg.x2 = endX;
                    seg.traceKind = isBus ? tk : WaveTraceKind::DigitalScalar;
                    seg.value = isBus ? 'b' : lastVal;
                    seg.y = isBus ? yBase : (lastVal == '0' ? yL : yH);
                    seg.text = lastText;
                    segments.push_back(seg);
                }

                newCache[i] = std::move(segments);
            };

            const unsigned nThreads = WaveformPerf::CacheBuildThreads();
            if (nThreads <= 1 || signals.size() < 2) {
                for (size_t i = 0; i < signals.size(); ++i)
                    buildRow(i);
            } else {
                std::vector<std::thread> workers;
                const size_t chunk = (signals.size() + nThreads - 1) / nThreads;
                workers.reserve(nThreads);
                for (unsigned t = 0; t < nThreads; ++t) {
                    const size_t i0 = (size_t)t * chunk;
                    const size_t i1 = std::min(signals.size(), i0 + chunk);
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

            std::lock_guard<std::mutex> lock2(m_cacheMutex);
            if (buildEpoch != m_cacheBuildEpoch)
                return;
            m_cachedSegments = std::move(newCache);
            m_cacheKeyOffset = cacheKeyOffset;
            m_cacheKeyRange = cacheKeyRange;
            m_cacheKeyViewW = cacheKeyViewW;
            CallAfter([this]() { Refresh(); });
        });
    }

    void OnResize(wxSizeEvent&)
    {
        if (!m_allSignals.empty()) RequestDrawCacheRebuild();
    }

    void OpenTraceFile(const wxString& path)
    {
        if (path.IsEmpty())
            return;
        ClearWavePanel();

        char err[512] = {};
#if defined(__WXMSW__)
        wxString pathOpen(path);
        wxFileName fn(path);
        const wxString shortPath = fn.GetShortPath();
        if (!shortPath.empty())
            pathOpen = shortPath;
        const wxScopedCharBuffer mb(pathOpen.mb_str(wxConvLocal));
        const char* narrow = mb.data() ? mb.data() : "";
        vcd_t* data = trace_load_from_path(narrow, err, sizeof(err));
#else
        wxCharBuffer utf8 = path.ToUTF8();
        vcd_t* data = trace_load_from_path(utf8.data(), err, sizeof(err));
#endif
        if (!data) {
            wxLogWarning("trace_load failed for \"%s\": %s", path, wxString::FromUTF8(err));
            return;
        }
        if (err[0] != '\0' && strncmp(err, "WARN:", 5) == 0) {
            wxMessageBox(
                wxString::FromUTF8(err + 5),
                wxT("Large VCD"),
                wxOK | wxICON_INFORMATION,
                this);
        }
        SetVcdData(data);
    }

    void OpenVCDFile(wxString path) { OpenTraceFile(path); }

    void OpenFSTFile(wxString path) { OpenTraceFile(path); }

    void ClearWavePanel()
    {
        CancelTraceLoad();
        JoinTraceWorkerIfJoined();
        JoinCacheWorkerIfJoined();
        if (m_cacheDebounceTimer)
            m_cacheDebounceTimer->Stop();
        if (m_traceLoadDebounceTimer)
            m_traceLoadDebounceTimer->Stop();
        if (m_vcdData) {
            vcd_free(m_vcdData);
            m_vcdData = nullptr;
        }
        m_allSignals.clear();
        m_displayedSignals2.clear();
        m_signalColors.clear();
        m_currentTimestamp = 0;
        m_displayTimeRange = 1000;
        m_maxTimestamp = 1000;
        {
            std::lock_guard<std::mutex> lock(m_cacheMutex);
            m_cachedSegments.clear();
        }
        m_cacheKeyOffset = LLONG_MIN;
        m_cacheKeyRange = -1;
        m_cacheKeyViewW = -1;
        m_traceLoadPadT0 = 0;
        m_traceLoadPadT1 = 0;
        Refresh();
    }

    void SetVcdData(vcd_t* vcdData)
    {
        m_vcdData = vcdData;
        if (!m_vcdData) return;
        m_maxTimestamp = vcd_get_max_timestamp(m_vcdData);
        if (m_maxTimestamp <= 0) m_maxTimestamp = 1000;
        m_displayTimeRange = m_maxTimestamp;
        m_timeOffset = 0;
        m_allSignals.clear();
        signal_node_t* node = m_vcdData->signals_head;
        while (node) { m_allSignals.push_back(&node->signal); node = node->next; }
        /* 波形区默认不铺信号：用户在左侧展开模块并从信号列表双击加入 */
        m_displayedSignals2.clear();
        AssignSignalColors();
        RequestDrawCacheRebuild(true);
        wxLogDebug(
            "[Bear2Wave][Wave] SetVcdData: signals=%zu maxTs=%lld displayRange=%lld",
            m_allSignals.size(),
            (long long)m_maxTimestamp,
            (long long)m_displayTimeRange);
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
        RequestDrawCacheRebuild();
        Refresh();
    }

    void SetPlayheadChangedCallback(std::function<void(long long)> cb) { m_onPlayheadChanged = std::move(cb); }

    void SetOpenScopeCallback(std::function<void(signal_t*)> cb) { m_onOpenScope = std::move(cb); }

    void SetTimeViewChangedCallback(std::function<void(long long, long long)> cb)
    {
        m_onTimeViewChanged = std::move(cb);
    }

    void SetCurrentTimestamp(long long ts, bool notifySlider = true)
    {
        const long long before = m_currentTimestamp;
        m_currentTimestamp = ts;
        m_currentTimestamp = std::max(0LL, m_currentTimestamp);
        if (m_maxTimestamp > 0)
            m_currentTimestamp = std::min(m_maxTimestamp, m_currentTimestamp);
        if (notifySlider && m_onPlayheadChanged)
            m_onPlayheadChanged(m_currentTimestamp);
        if (m_currentTimestamp != before) {
            wxLogDebug(
                "[Bear2Wave][Wave] SetCurrentTimestamp: req=%lld -> playhead=%lld maxTs=%lld notify=%d showCursorVal=%d",
                (long long)ts,
                m_currentTimestamp,
                m_maxTimestamp,
                notifySlider ? 1 : 0,
                m_showCursorValue ? 1 : 0);
        }
        Refresh();
    }

    void ZoomIn() { m_displayTimeRange = std::max(10LL, (long long)(m_displayTimeRange * 0.7)); ClampTimeView(); RequestDrawCacheRebuild(); Refresh(); }
    void ZoomOut() { m_displayTimeRange = std::min(m_maxTimestamp * 2, (long long)(m_displayTimeRange * 1.5)); ClampTimeView(); RequestDrawCacheRebuild(); Refresh(); }
    void ZoomReset() { m_displayTimeRange = m_maxTimestamp; ClampTimeView(); RequestDrawCacheRebuild(); Refresh(); }

    /** 视窗向左翻一整屏（offset -= displayTimeRange）。 */
    void PageLeft()
    {
        const long long step = std::max(1LL, m_displayTimeRange);
        m_timeOffset -= step;
        ClampTimeView();
        RequestDrawCacheRebuild();
        Refresh();
        EmitTimeViewChanged();
    }

    /** 视窗向右翻一整屏（offset += displayTimeRange）。 */
    void PageRight()
    {
        const long long step = std::max(1LL, m_displayTimeRange);
        m_timeOffset += step;
        ClampTimeView();
        RequestDrawCacheRebuild();
        Refresh();
        EmitTimeViewChanged();
    }

    /** 脚本/UI：设置可见时间窗 [from, to]（含钳位与重绘）。 */
    void ApplyVisibleTimeRange(long long from, long long to)
    {
        if (to < from)
            std::swap(from, to);
        m_timeOffset = from;
        m_displayTimeRange = std::max(1LL, to - from);
        ClampTimeView();
        RequestDrawCacheRebuild();
        Refresh();
        EmitTimeViewChanged();
    }

private:
    void EmitTimeViewChanged()
    {
        QueueTraceLoad();
        if (m_onTimeViewChanged)
            m_onTimeViewChanged(m_timeOffset, m_displayTimeRange);
    }
    /** 设环境变量 BEAR2WAVE_WAVE_DEBUG=1 打开逐帧/逐行详细 wxLogDebug（量很大）。 */
    static bool WaveDebugVerbose()
    {
        const char* e = std::getenv("BEAR2WAVE_WAVE_DEBUG");
        return e && e[0] != '\0' && e[0] != '0';
    }

    void JoinCacheWorkerIfJoined()
    {
        if (m_cacheBuildThread.joinable())
            m_cacheBuildThread.join();
    }

    void OnCacheDebounceTimer(wxTimerEvent&) { BuildDrawCacheAsync(); }

    void OnTraceLoadDebounceTimer(wxTimerEvent&) { StartTraceLoadAsync(); }

    void JoinTraceWorkerIfJoined()
    {
        if (m_traceLoadThread.joinable())
            m_traceLoadThread.join();
    }

    void CancelTraceLoad()
    {
        ++m_traceLoadEpoch;
        if (m_vcdData)
            trace_loader_request_cancel(m_vcdData);
    }

    void QueueTraceLoad()
    {
        if (!m_vcdData || !trace_uses_lazy_backend(m_vcdData))
            return;
        if (!m_traceLoadDebounceTimer)
            return;
        m_traceLoadDebounceTimer->Start(WaveformPerf::TraceLoadDebounceMs(), wxTIMER_ONE_SHOT);
    }

    static bool SignalNeedsLoadForRange(signal_t* sig, uint64_t t0, uint64_t t1)
    {
        if (!sig || !sig->trace_data_loaded)
            return true;
        if (t0 == TRACE_LOAD_T0_FULL && t1 == TRACE_LOAD_T1_FULL)
            return sig->trace_loaded_t1 != TRACE_LOAD_T1_FULL;
        return sig->trace_loaded_t0 > t0 || sig->trace_loaded_t1 < t1;
    }

    void TraceLoadTimeRange(uint64_t* out_t0, uint64_t* out_t1) const
    {
        const uint64_t fileMax = m_vcdData && m_vcdData->trace_max_timestamp > 0
            ? m_vcdData->trace_max_timestamp
            : (uint64_t)std::max<long long>(m_maxTimestamp, 1);
        const uint64_t v0 = (uint64_t)std::max<long long>(0, m_timeOffset);
        const uint64_t span = (uint64_t)std::max<long long>(1, m_displayTimeRange);
        uint64_t v1 = v0 + span;
        if (v1 > fileMax)
            v1 = fileMax;
        trace_compute_padded_range(v0, v1, fileMax, WaveformPerf::TraceLoadMarginRatio(), out_t0, out_t1);
    }

    void EnsureTraceSignalLoadedSync(signal_t* sig)
    {
        if (!sig || !m_vcdData || !trace_uses_lazy_backend(m_vcdData))
            return;
        uint64_t t0 = 0, t1 = TRACE_LOAD_T1_FULL;
        TraceLoadTimeRange(&t0, &t1);
        if (!SignalNeedsLoadForRange(sig, t0, t1))
            return;
        trace_load_signals(m_vcdData, &sig, 1, t0, t1);
    }

    void StartTraceLoadAsync()
    {
        if (!m_vcdData || !trace_uses_lazy_backend(m_vcdData))
            return;

        uint64_t t0 = 0, t1 = TRACE_LOAD_T1_FULL;
        TraceLoadTimeRange(&t0, &t1);

        if (m_traceLoadPadT0 <= t0 && m_traceLoadPadT1 >= t1) {
            bool anyNeed = false;
            for (signal_t* sig : m_displayedSignals2) {
                if (sig && SignalNeedsLoadForRange(sig, t0, t1)) {
                    anyNeed = true;
                    break;
                }
            }
            if (!anyNeed)
                return;
        }

        std::vector<signal_t*> need;
        need.reserve(m_displayedSignals2.size());
        for (signal_t* sig : m_displayedSignals2) {
            if (sig && SignalNeedsLoadForRange(sig, t0, t1))
                need.push_back(sig);
        }
        if (need.empty())
            return;

        JoinTraceWorkerIfJoined();
        CancelTraceLoad();
        const uint64_t epoch = m_traceLoadEpoch;
        vcd_t* vcd = m_vcdData;
        m_traceLoadThread = std::thread([this, epoch, vcd, need = std::move(need), t0, t1]() {
            const int rc = trace_load_signals(
                vcd, const_cast<signal_t**>(need.data()), need.size(), t0, t1);
            CallAfter([this, epoch, rc, t0, t1]() {
                if (epoch != m_traceLoadEpoch)
                    return;
                if (rc == 1) {
                    wxLogMessage(wxT("波形数据加载已取消"));
                    return;
                }
                if (rc < 0) {
                    wxLogWarning(wxT("波形数据加载失败"));
                    return;
                }
                m_traceLoadPadT0 = t0;
                m_traceLoadPadT1 = t1;
                RequestDrawCacheRebuild();
                Refresh();
            });
        });
    }

    std::unique_ptr<wxTimer> m_cacheDebounceTimer;
    std::unique_ptr<wxTimer> m_traceLoadDebounceTimer;
    std::thread m_traceLoadThread;
    uint64_t m_traceLoadEpoch = 0;
    uint64_t m_traceLoadPadT0 = 0;
    uint64_t m_traceLoadPadT1 = 0;
    std::function<void(long long)> m_onPlayheadChanged;
    std::function<void(long long, long long)> m_onTimeViewChanged;
    std::function<void(signal_t*)> m_onOpenScope;
    std::thread m_cacheBuildThread;
    uint64_t m_cacheBuildEpoch = 0;

    static wxColour ThemePlotBg() { return wxColour(252, 253, 255); }
    static wxColour ThemeRowStripe() { return wxColour(241, 244, 248); }
    static wxColour ThemeAxisBand() { return wxColour(232, 236, 242); }
    static wxColour ThemeTraceDefault() { return wxColour(44, 62, 80); }

    wxString FormatTimeLabel(double t) const
    {
        if (t < 1e3) return wxString::Format("%.0f ns", t);
        else if (t < 1e6) return wxString::Format("%.2f us", t / 1e3);
        else if (t < 1e9) return wxString::Format("%.2f ms", t / 1e6);
        else return wxString::Format("%.2f s", t / 1e9);
    }

    wxString FormatTimeLabelFromSimTime(long long t) const
    {
        const double td = static_cast<double>(std::min<long long>(t, (long long)9e15));
        return FormatTimeLabel(td);
    }

    void DrawMiniMap(wxAutoBufferedPaintDC& dc, wxSize size)
    {
        int w = 200, h = 80;
        int x = size.x - w - 10, y = 10;
        m_minimapRect = wxRect(x, y, w, h);
        dc.SetBrush(wxColour(30, 30, 30));
        dc.SetPen(wxPen(wxColour(100, 100, 100)));
        dc.DrawRectangle(m_minimapRect);
        if (m_allSignals.empty() || m_maxTimestamp <= 0) return;

        const double inv = 1.0 / (double)m_maxTimestamp;
        const double scale = (double)w * inv;
        int maxSignals = std::min(20, (int)m_displayedSignals2.size());
        const int rowH = maxSignals > 0 ? (h / maxSignals) : h;
        for (int i = 0; i < maxSignals; i++)
        {
            signal_t* sig = m_displayedSignals2[i];
            if (!sig || !sig->value_changes || sig->changes_count < 2)
                continue;
            int yBase = y + 5 + i * rowH;
            const size_t mstep = std::max((size_t)1, sig->changes_count / 500u);
            for (size_t c = 1; c < sig->changes_count; c += mstep)
            {
                long long t1 = (long long)sig->value_changes[c - 1].timestamp;
                long long t2 = (long long)sig->value_changes[c].timestamp;
                int x1 = x + (int)((double)t1 * scale);
                int x2 = x + (int)((double)t2 * scale);
                dc.SetPen(wxPen(wxColour(100, 200, 255), 1));
                dc.DrawLine(x1, yBase, x2, yBase);
            }
        }

        int viewX1 = x + (int)((double)m_timeOffset * inv * (double)w);
        int viewX2 = x + (int)((double)(m_timeOffset + m_displayTimeRange) * inv * (double)w);
        dc.SetBrush(*wxTRANSPARENT_BRUSH);
        dc.SetPen(wxPen(wxColour(255, 200, 0), 2));
        dc.DrawRectangle(viewX1, y, viewX2 - viewX1, h);
    }

    void OnPaint(wxPaintEvent&)
    {
        wxAutoBufferedPaintDC dc(this);
        wxSize size = GetSize();
        dc.SetBrush(ThemePlotBg());
        dc.SetPen(*wxTRANSPARENT_PEN);
        dc.DrawRectangle(0, 0, size.x, size.y);
        if (m_allSignals.empty())
        {
            dc.SetTextForeground(wxColour(90, 96, 110));
            if (!m_vcdData)
                dc.DrawText("Please Start Simulation first", size.x / 2 - 80, size.y / 2);
            else
                dc.DrawText("No signals in trace (check FST/VCD parse / hierarchy)", size.x / 2 - 160, size.y / 2);
            return;
        }

        int viewW = size.x - LEFT_MARGIN - WAVE_PADDING;
        if (viewW < 100) viewW = 100;
        double scale = (double)viewW / m_displayTimeRange;
        const int timeAxisY = 25;
        int cursorX = TimeToX(m_currentTimestamp, scale);
        cursorX = std::max(LEFT_MARGIN, std::min(cursorX, size.x - WAVE_PADDING - 1));
        double targetPixel = 80.0;
        double rawStep = targetPixel / scale;
        double step = NiceStep(rawStep);
        double start = floor((double)m_timeOffset / step) * step;

        /* Time ruler band */
        dc.SetBrush(ThemeAxisBand());
        dc.SetPen(wxPen(wxColour(210, 216, 226), 1));
        dc.DrawRectangle(0, 0, size.x, timeAxisY + 8);
        dc.DrawLine(LEFT_MARGIN, timeAxisY + 8, size.x - WAVE_PADDING, timeAxisY + 8);

        dc.SetFont(wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT));
        int lastTextX = -1000;
        int gi = 0;
        for (double ts = start; ts <= (double)m_timeOffset + (double)m_displayTimeRange; ts += step, ++gi)
        {
            const int x = TimeToX((long long)ts, scale);
            const bool major = (gi % 5 == 0);
            if (major && abs(x - lastTextX) > 56)
            {
                dc.SetTextForeground(wxColour(55, 65, 80));
                dc.DrawText(FormatTimeLabel(ts), std::max(LEFT_MARGIN + 2, x - 14), 4);
                lastTextX = x;
            }
            dc.SetPen(major ? wxPen(wxColour(188, 196, 210), 1) : wxPen(wxColour(220, 226, 236), 1, wxPENSTYLE_DOT));
            dc.DrawLine(x, timeAxisY + 8, x, size.y);
        }

        /* Hover tick + time (plot area) */
        if (m_hoverPlotX >= LEFT_MARGIN && m_hoverPlotX < size.x - WAVE_PADDING && !m_rulerHoverLabel.empty())
        {
            dc.SetPen(wxPen(wxColour(110, 128, 152), 1));
            dc.DrawLine(m_hoverPlotX, timeAxisY + 2, m_hoverPlotX, timeAxisY + 10);
            dc.SetTextForeground(wxColour(75, 85, 105));
            wxSize tw = dc.GetTextExtent(m_rulerHoverLabel);
            int tx = std::min(m_hoverPlotX + 6, size.x - (int)tw.x - WAVE_PADDING - 6);
            tx = std::max(LEFT_MARGIN + 2, tx);
            dc.DrawText(m_rulerHoverLabel, tx, 4);
        }

        wxRect clip;
        dc.GetClippingBox(&clip.x, &clip.y, &clip.width, &clip.height);
        std::lock_guard<std::mutex> lock(m_cacheMutex);
        int safeCount = std::min((int)m_displayedSignals2.size(), (int)m_cachedSegments.size());
        int yOffset = 30;

        if (safeCount == 0) {
            dc.SetTextForeground(wxColour(120, 125, 135));
            dc.DrawText(wxString::FromUTF8(
                "请展开左侧模块树，在信号列表中双击信号即可显示波形"),
                LEFT_MARGIN + 16, timeAxisY + yOffset + 36);
        }

        /* 调试：播放头变化时打一行（避免每帧刷屏） */
        if (m_showCursorValue && safeCount > 0) {
            static long long s_dbgLastPaintPlayhead = LLONG_MIN;
            if (m_currentTimestamp != s_dbgLastPaintPlayhead) {
                s_dbgLastPaintPlayhead = m_currentTimestamp;
                signal_t* s0 = m_displayedSignals2[0];
                if (s0) {
                    const std::string sample = GetValueAt(s0, m_currentTimestamp);
                    const value_change_t* vc = s0->value_changes;
                    const size_t nc = s0->changes_count;
                    timestamp_t tmin = nc ? vc[0].timestamp : 0, tmax = tmin;
                    for (size_t c = 1; c < nc; ++c) {
                        if (vc[c].timestamp < tmin) tmin = vc[c].timestamp;
                        if (vc[c].timestamp > tmax) tmax = vc[c].timestamp;
                    }
                    wxLogDebug(
                        "[Bear2Wave][Paint] ph=%lld cursorX=%d off=%lld range=%lld scale=%.8f | row0=%s "
                        "sample=\"%s\" chg=%zu tmin=%llu tmax=%llu",
                        m_currentTimestamp,
                        cursorX,
                        m_timeOffset,
                        m_displayTimeRange,
                        scale,
                        s0->full_name,
                        sample.c_str(),
                        nc,
                        (unsigned long long)tmin,
                        (unsigned long long)tmax);
                }
            }
        }

        for (int i = 0; i < safeCount; i++)
        {
            signal_t* sig = m_displayedSignals2[i];
            const auto commentIt = m_rowComments.find(i);
            const bool isCommentRow = (commentIt != m_rowComments.end());

            int yBase = timeAxisY + i * SIGNAL_ROW_HEIGHT + yOffset;
            // 根据模拟信号高度扩展值调整信号高度
            int height = 15 + (m_analogHeightExtension * 15 / 100);
            int yH = yBase - height, yL = yBase + height;

            if (i & 1) {
                dc.SetBrush(ThemeRowStripe());
                dc.SetPen(*wxTRANSPARENT_PEN);
                dc.DrawRectangle(0, yBase - 28, size.x, SIGNAL_ROW_HEIGHT - 2);
            }

            if (isCommentRow) {
                dc.SetBrush(wxColour(255, 252, 220));
                dc.SetPen(wxPen(wxColour(200, 180, 100), 1));
                dc.DrawRectangle(0, yBase - 28, size.x, SIGNAL_ROW_HEIGHT - 2);
                if (i == m_selectedSignalIndex) {
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
            const int tf = [&]() {
                auto it = m_signalTransforms.find(sig);
                return it != m_signalTransforms.end() ? it->second : 0;
            }();
            if (tf & TR_RANGE_FILL) {
                dc.SetBrush(wxColour(235, 245, 255));
                dc.SetPen(*wxTRANSPARENT_PEN);
                dc.DrawRectangle(LEFT_MARGIN, yBase - 15, size.x - LEFT_MARGIN - WAVE_PADDING, 30);
            }
            const WaveTraceKind rowKind = segments.empty() ? ClassifyTraceKind(sig) : segments[0].traceKind;
            for (size_t k = 0; k < segments.size(); k++)
            {
                auto& seg = segments[k];
                int drawX1 = std::max(seg.x1, clip.x);
                int drawX2 = std::min(seg.x2, clip.x + clip.width);
                if (drawX2 <= drawX1) continue;

                bool isHover = (i == m_hoverSignal && k == m_hoverSegment);
                if (rowKind == WaveTraceKind::BusBits || rowKind == WaveTraceKind::TextString)
                {
                    wxRect rect(drawX1, seg.y - 9, drawX2 - drawX1, 18);
                    const bool isText = (rowKind == WaveTraceKind::TextString);
                    dc.SetBrush(isHover ? wxColour(255, 220, 150)
                                        : (isText ? wxColour(235, 225, 200) : wxColour(200, 230, 255)));
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

                if (rowKind == WaveTraceKind::RealAnalog)
                {
                    wxColour color = TraceColourForSignal(sig, isMatch);
                    if (!m_searchKeyword.empty() && isMatch)
                        color = wxColour(255, 60, 60);
                    else if (!m_searchKeyword.empty())
                        color = wxColour(180, 180, 180);
                    else if (m_signalColors.find(sig->signal_id) == m_signalColors.end())
                        color = wxColour(0, 120, 200);
                    dc.SetPen(isHover ? wxPen(wxColour(255, 140, 0), 2) : wxPen(color, 2));
                    dc.DrawLine(drawX1, seg.y, drawX2, seg.y);
                    if (k > 0)
                    {
                        auto& prev = segments[k - 1];
                        if (prev.y != seg.y)
                            dc.DrawLine(drawX1, prev.y, drawX1, seg.y);
                    }
                    if (isHover && !seg.text.empty())
                        dc.DrawText(wxString::Format("Value: %s", seg.text.c_str()), seg.x1, seg.y - 20);
                    continue;
                }

                int yCurr = (seg.value == '0') ? yL : yH;
                wxColour color = TraceColourForSignal(sig, isMatch);

                dc.SetPen(isHover ? wxPen(wxColour(230, 126, 34), 2) : wxPen(color, 2));
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
                /* 必须与播放头/滑块一致：只用 m_currentTimestamp，勿用像素反推（缩放后多时刻共像素会“定死”一值） */
                std::string val = GetValueAt(sig, m_currentTimestamp);
                if (val.empty() && sig && i == 0) {
                    static long long s_dbgEmptyLabelPh = LLONG_MIN;
                    if (m_currentTimestamp != s_dbgEmptyLabelPh) {
                        s_dbgEmptyLabelPh = m_currentTimestamp;
                        wxLogDebug(
                            "[Bear2Wave][Label] skip row0 (empty val) sig=%s ph=%lld",
                            sig->full_name,
                            m_currentTimestamp);
                    }
                }
                if (!val.empty())
                {
                    /* %s 需要 const char*；勿把 std::string 直接作可变参传入，否则未定义行为（乱拼接、黑块等） */
                    wxString text = wxString::Format("%s = %s", sig->full_name, val.c_str());
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

        if (m_hoverSignal >= 0 && m_hoverSegment >= 0
            && m_hoverSignal < (int)m_cachedSegments.size()
            && m_hoverSegment < (int)m_cachedSegments[m_hoverSignal].size())
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
            if (isDrag) dc.DrawText(wxString::Format("T=%lld", mk.timestamp), x + 5, sz.y + 15);
        }

        if (m_hasMarkerA && m_markerA >= 0 && m_markerB >= 0)
        {
            int xA = TimeToX(m_markerA, scale);
            int xB = TimeToX(m_markerB, scale);
            dc.SetPen(wxPen(wxColour(255, 0, 0), 2));
            dc.DrawLine(xA, 0, xA, size.y);
            dc.DrawLine(xB, 0, xB, size.y);
            wxString text = wxString::Format("ΔT = %lld", (long long)(m_markerB - m_markerA));
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

        dc.SetPen(wxPen(wxColour(66, 133, 244), 2, wxPENSTYLE_SHORT_DASH));
        dc.DrawLine(cursorX, timeAxisY + 8, cursorX, size.y);
        DrawMiniMap(dc, size);
    }

public:
    void InitSignalTree() {
        if (m_signalTreeRoot) { FreeSignalTree(m_signalTreeRoot); m_signalTreeRoot = nullptr; }
    }
};
