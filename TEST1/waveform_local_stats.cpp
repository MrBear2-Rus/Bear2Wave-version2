#include "waveform_local_stats.h"

#include "core/WaveformRadix.h"
#include "panels/WaveformPanel.hpp"
#include "ui/WaveformCompareHub.h"
#include "core/trace_vc.h"
#include "waveform_analysis.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace WaveformLocalStats {

static long long glitch_threshold()
{
    const char* e = std::getenv("BEAR2WAVE_GLITCH_MAX_WIDTH");
    if (e && e[0])
        return std::max(1LL, std::atoll(e));
    return 3;
}

static bool name_has_token(const char* name, const char* token)
{
    if (!name || !token)
        return false;
    std::string s(name);
    for (char& c : s)
        c = (char)std::tolower((unsigned char)c);
    return s.find(token) != std::string::npos;
}

static void collect_indices_in_window(
    signal_t* sig,
    long long t0,
    long long t1,
    std::vector<size_t>& idx)
{
    idx.clear();
    if (!sig || trace_vc_count(sig) == 0)
        return;
    vcd_ensure_signal_sorted(sig);
    for (size_t i = 0; i < trace_vc_count(sig); ++i) {
        const long long ts = (long long)trace_vc_timestamp(sig, i);
        if (ts >= t0 && ts <= t1)
            idx.push_back(i);
    }
}

static bool value_has_xz(const char* v)
{
    if (!v)
        return false;
    for (const char* p = v; *p; ++p) {
        const char c = (char)std::tolower((unsigned char)*p);
        if (c == 'x' || c == 'z')
            return true;
    }
    return false;
}

static SignalRow compute_row(signal_t* sig, long long t0, long long t1)
{
    SignalRow row;
    if (!sig)
        return row;
    row.name = sig->full_name[0] ? sig->full_name : sig->name;
    row.width = (unsigned)sig->size;

    std::vector<size_t> idx;
    collect_indices_in_window(sig, t0, t1, idx);
    row.transitions = idx.size();
    if (idx.empty())
        return row;

    vcd_ensure_signal_sorted(sig);

    for (size_t ii : idx) {
        char vbuf[VCD_SIGNAL_SIZE];
        trace_vc_format_value(sig, ii, vbuf, sizeof(vbuf));
        if (value_has_xz(vbuf))
            row.has_xz = true;
    }

    if (sig->size > 1)
        return row;

    std::vector<long long> rising_ts;
    char prev = 0;
    bool havePrev = false;
    long long high_time = 0;
    char level_at_start = '0';
    for (size_t k = 0; k < trace_vc_count(sig); ++k) {
        const long long ts = (long long)trace_vc_timestamp(sig, k);
        char vbuf[VCD_SIGNAL_SIZE];
        trace_vc_format_value(sig, k, vbuf, sizeof(vbuf));
        if (ts < t0)
            level_at_start = vbuf[0];
        if (ts > t1)
            break;
    }
    char cur = level_at_start;
    long long cur_ts = t0;

    const long long glitch_max = glitch_threshold();
    for (size_t ii : idx) {
        const long long ts = (long long)trace_vc_timestamp(sig, ii);
        char vbuf[VCD_SIGNAL_SIZE];
        trace_vc_format_value(sig, ii, vbuf, sizeof(vbuf));
        const char v = vbuf[0];
        if (cur == '1')
            high_time += std::max(0LL, ts - cur_ts);
        if (havePrev && v != prev) {
            if (v == '1' && (prev == '0' || prev == '1'))
                rising_ts.push_back(ts);
            if (prev == '1' && v == '0') {
                const long long pulse = ts - cur_ts;
                if (pulse > 0 && pulse <= glitch_max)
                    ++row.glitch_count;
            }
        }
        cur = v;
        cur_ts = ts;
        prev = v;
        havePrev = true;
    }
    if (cur == '1')
        high_time += std::max(0LL, t1 - cur_ts);

    const long long span = std::max(1LL, t1 - t0);
    row.duty_percent = (int)(100.0 * (double)high_time / (double)span + 0.5);

    if (rising_ts.size() >= 2) {
        std::vector<long long> gaps;
        for (size_t i = 1; i < rising_ts.size(); ++i)
            gaps.push_back(rising_ts[i] - rising_ts[i - 1]);
        std::sort(gaps.begin(), gaps.end());
        row.period_median = gaps[gaps.size() / 2];
    }

    if (rising_ts.empty() && idx.size() <= 1)
        row.constant = true;

    if (name_has_token(row.name.c_str(), "rst") || name_has_token(sig->name, "reset")) {
        for (size_t ii : idx) {
            char vbuf[VCD_SIGNAL_SIZE];
            trace_vc_format_value(sig, ii, vbuf, sizeof(vbuf));
            if (vbuf[0] == '1') {
                row.reset_release_ts = (long long)trace_vc_timestamp(sig, ii);
                break;
            }
        }
    }
    return row;
}

TimeWindow ViewportWindow(WaveformPanel* panel)
{
    TimeWindow w;
    if (!panel)
        return w;
    w.t0 = panel->m_timeOffset;
    w.t1 = panel->m_timeOffset + std::max(1LL, panel->m_displayTimeRange);
    w.valid = true;
    return w;
}

TimeWindow MarkerWindow(WaveformPanel* panel)
{
    TimeWindow w;
    if (!panel || !panel->m_hasMarkerA || panel->m_markerA < 0 || panel->m_markerB < 0)
        return w;
    w.t0 = std::min(panel->m_markerA, panel->m_markerB);
    w.t1 = std::max(panel->m_markerA, panel->m_markerB);
    if (w.t1 <= w.t0)
        w.t1 = w.t0 + 1;
    w.valid = true;
    return w;
}

void PrepareForAnalysis(WaveformPanel* panel, const std::vector<signal_t*>& signals)
{
    WaveformAnalysis::PrepareSignals(panel, signals);
}

std::vector<SignalRow> ComputeRows(
    WaveformPanel* panel,
    const std::vector<signal_t*>& signals,
    const TimeWindow& window)
{
    std::vector<SignalRow> rows;
    if (!panel || !window.valid)
        return rows;
    PrepareForAnalysis(panel, signals);
    for (signal_t* sig : signals) {
        if (sig)
            rows.push_back(compute_row(sig, window.t0, window.t1));
    }
    return rows;
}

static void append_row_text(std::ostringstream& out, const SignalRow& r)
{
    out << "--- " << r.name << " (" << r.width << " bit) ---\n";
    out << "  transitions: " << r.transitions << "\n";
    if (r.width == 1) {
        if (r.duty_percent >= 0)
            out << "  duty: " << r.duty_percent << "%\n";
        if (r.period_median > 0)
            out << "  period (median): " << r.period_median << "\n";
        if (r.glitch_count)
            out << "  narrow pulses: " << r.glitch_count << "\n";
        if (r.reset_release_ts >= 0)
            out << "  reset release: @" << r.reset_release_ts << "\n";
        if (r.constant)
            out << "  nearly constant\n";
    }
    if (r.has_xz)
        out << "  contains X/Z\n";
}

std::string BuildReport(
    WaveformPanel* panel,
    const std::vector<signal_t*>& signals,
    const TimeWindow& window)
{
    if (!panel || !window.valid)
        return "Error: invalid time window.\n";

    std::ostringstream out;
    out << "=== Local stats ===\n";
    if (panel->GetVcd()) {
        out << "Timescale: " << panel->GetVcd()->timescale.scale << " "
            << panel->GetVcd()->timescale.unit << "\n";
    }
    out << "Range: " << window.t0 << " .. " << window.t1
        << " (delta=" << (window.t1 - window.t0) << ")\n\n";

    if (signals.empty()) {
        out << "No signals selected.\n";
        return out.str();
    }

    for (const SignalRow& r : ComputeRows(panel, signals, window))
        append_row_text(out, r);
    return out.str();
}

std::string BuildIntervalSummary(WaveformPanel* panel, const TimeWindow& window)
{
    std::ostringstream out;
    if (!panel || !window.valid)
        return "Invalid window.\n";
    out << "Viewport: " << panel->m_timeOffset << " .. "
        << (panel->m_timeOffset + panel->m_displayTimeRange) << "\n";
    out << "Playhead: " << panel->m_currentTimestamp << "\n";
    if (panel->m_hasMarkerA && panel->m_markerA >= 0 && panel->m_markerB >= 0) {
        out << "Marker A: " << panel->m_markerA << "\n";
        out << "Marker B: " << panel->m_markerB << "\n";
        out << "Marker delta: " << (panel->m_markerB - panel->m_markerA) << "\n";
    } else {
        out << "Markers: not set (Shift+click on wave)\n";
    }
    out << "Analysis range: " << window.t0 << " .. " << window.t1
        << " (delta=" << (window.t1 - window.t0) << ")\n";
    return out.str();
}

std::string BuildBusSection(
    WaveformPanel* panel,
    const std::vector<signal_t*>& signals,
    const TimeWindow& window,
    int max_samples_per_signal)
{
    if (!panel || !window.valid)
        return {};
    PrepareForAnalysis(panel, signals);

    std::ostringstream out;
    out << "=== Bus / radix ===\n";
    bool any = false;
    for (signal_t* sig : signals) {
        if (!sig || sig->size <= 1)
            continue;
        any = true;
        const char* label = sig->full_name[0] ? sig->full_name : sig->name;
        out << "--- " << label << " (" << sig->size << " bit) ---\n";

        std::vector<size_t> idx;
        collect_indices_in_window(sig, window.t0, window.t1, idx);
        if (idx.empty()) {
            out << "  (no changes in range)\n";
            continue;
        }

        std::map<std::string, int> hist;
        const size_t step = idx.size() > (size_t)max_samples_per_signal
            ? (idx.size() + (size_t)max_samples_per_signal - 1) / (size_t)max_samples_per_signal
            : 1;
        size_t n = 0;
        vcd_ensure_signal_sorted(sig);
        for (size_t j = 0; j < idx.size() && n < (size_t)max_samples_per_signal; j += step) {
            char raw[VCD_SIGNAL_SIZE];
            trace_vc_format_value(sig, idx[j], raw, sizeof(raw));
            const std::string hex = WaveformRadix::FormatValue(raw, WaveformRadix::Radix::Hex, (int)sig->size);
            const std::string asc = (sig->size == 8)
                ? WaveformRadix::FormatValue(raw, WaveformRadix::Radix::Ascii, 8)
                : std::string();
            out << "  @" << trace_vc_timestamp(sig, idx[j]) << ": hex=" << hex;
            if (!asc.empty())
                out << " ascii=\"" << asc << "\"";
            out << "\n";
            hist[hex] += 1;
            ++n;
        }
        if (!hist.empty()) {
            std::string top;
            int topc = 0;
            for (const auto& kv : hist) {
                if (kv.second > topc) {
                    topc = kv.second;
                    top = kv.first;
                }
            }
            out << "  most common value: " << top << " (" << topc << " samples)\n";
        }
    }
    if (!any)
        out << "(no multi-bit signals in selection)\n";
    return out.str();
}

std::string BuildRulesSection(
    WaveformPanel* panel,
    const std::vector<signal_t*>& signals,
    const TimeWindow& window)
{
    if (!panel || !window.valid)
        return {};

    std::ostringstream out;
    out << "=== Rules / checklist ===\n";

    const std::vector<SignalRow> rows = ComputeRows(panel, signals, window);
    int pass = 0;
    int fail = 0;
    auto check = [&](bool ok, const char* msg) {
        out << (ok ? "[PASS] " : "[FAIL] ") << msg << "\n";
        if (ok) ++pass; else ++fail;
    };

    bool any_rst_release = false;
    bool any_xz = false;
    bool any_activity = false;
    for (const SignalRow& r : rows) {
        if (r.reset_release_ts >= 0)
            any_rst_release = true;
        if (r.has_xz)
            any_xz = true;
        if (r.transitions > 0)
            any_activity = true;
    }
    check(any_activity, "At least one signal toggles in range");
    check(!any_xz, "No X/Z in selected signals (range)");
    if (any_rst_release)
        check(true, "Reset release detected on rst-like signal");

    std::vector<signal_t*> valids;
    std::vector<signal_t*> readys;
    for (signal_t* s : signals) {
        if (!s) continue;
        const char* n = s->full_name[0] ? s->full_name : s->name;
        if (name_has_token(n, "valid") || name_has_token(n, "tvalid"))
            valids.push_back(s);
        if (name_has_token(n, "ready") || name_has_token(n, "tready"))
            readys.push_back(s);
    }
    if (!valids.empty() && !readys.empty()) {
        out << "\nHandshake pairs:\n";
        for (signal_t* v : valids) {
            for (signal_t* r : readys) {
                const SignalRow ev = compute_row(v, window.t0, window.t1);
                const SignalRow er = compute_row(r, window.t0, window.t1);
                out << "  " << ev.name << " vs " << er.name
                    << ": edges " << ev.transitions << " / " << er.transitions << "\n";
            }
        }
    }

    if (signals.size() >= 2) {
        signal_t* a = signals[0];
        signal_t* b = signals[1];
        if (a && b && a->size == 1 && b->size == 1) {
            PrepareForAnalysis(panel, {a, b});
            std::vector<size_t> ia, ib;
            collect_indices_in_window(a, window.t0, window.t1, ia);
            collect_indices_in_window(b, window.t0, window.t1, ib);
            size_t mism = 0;
            size_t compared = 0;
            for (size_t i = 0; i < ia.size() && i < ib.size(); ++i) {
                const long long ta = (long long)trace_vc_timestamp(a, ia[i]);
                const long long tb = (long long)trace_vc_timestamp(b, ib[i]);
                if (ta != tb)
                    continue;
                ++compared;
                char va[8], vb[8];
                trace_vc_format_value(a, ia[i], va, sizeof(va));
                trace_vc_format_value(b, ib[i], vb, sizeof(vb));
                if (va[0] != vb[0])
                    ++mism;
            }
            check(mism == 0, "First two 1-bit signals: same timestamps same value (coarse)");
            if (compared == 0)
                out << "  (pair compare: no aligned timestamps)\n";
        }
    }

    out << "\nSummary: " << pass << " pass, " << fail << " fail\n";
    return out.str();
}

std::string BuildSetupCheck(
    WaveformPanel* panel,
    signal_t* sig_a,
    signal_t* sig_b,
    const TimeWindow& window,
    long long max_response_delay)
{
    if (!panel || !sig_a || !sig_b || !window.valid)
        return "Setup check: invalid args.\n";
    PrepareForAnalysis(panel, {sig_a, sig_b});

    std::ostringstream out;
    out << "=== Setup check ===\n";
    out << "A: " << (sig_a->full_name[0] ? sig_a->full_name : sig_a->name) << "\n";
    out << "B: " << (sig_b->full_name[0] ? sig_b->full_name : sig_b->name) << "\n";
    out << "Max response: " << max_response_delay << " time units\n";

    std::vector<size_t> ia;
    collect_indices_in_window(sig_a, window.t0, window.t1, ia);
    vcd_ensure_signal_sorted(sig_a);
    vcd_ensure_signal_sorted(sig_b);

    int violations = 0;
    int checked = 0;
    for (size_t ii : ia) {
        const long long t_a = (long long)trace_vc_timestamp(sig_a, ii);
        bool b_changed = false;
        for (size_t k = 0; k < trace_vc_count(sig_b); ++k) {
            const long long ts = (long long)trace_vc_timestamp(sig_b, k);
            if (ts > t_a && ts <= t_a + max_response_delay) {
                b_changed = true;
                break;
            }
        }
        ++checked;
        if (!b_changed) {
            ++violations;
            if (violations <= 8)
                out << "  @" << t_a << ": no B transition within " << max_response_delay << "\n";
        }
    }
    out << "Checked " << checked << " A edges, violations: " << violations << "\n";
    return out.str();
}

std::string BuildCompareSection(
    WaveformPanel* panel,
    const std::vector<signal_t*>& signals,
    const TimeWindow& window)
{
    if (!panel || !window.valid)
        return {};
    WaveformPanel* secondary = WaveformCompare::SecondaryPanelFor(panel);
    if (!secondary) {
        return "=== Compare ===\n(Open second trace via Compare menu)\n";
    }
    WaveformAnalysis::AnalysisWindow aw;
    aw.t0 = window.t0;
    aw.t1 = window.t1;
    aw.valid = true;
    return WaveformAnalysis::BuildCompareDiffSummary(panel, secondary, signals, aw);
}

std::string BuildTimeSkewEstimate(
    WaveformPanel* panel,
    signal_t* sig_a,
    signal_t* sig_b,
    const TimeWindow& window)
{
    if (!panel || !sig_a || !sig_b || !window.valid || sig_a->size != 1 || sig_b->size != 1)
        return "Time skew: need two 1-bit signals.\n";
    PrepareForAnalysis(panel, {sig_a, sig_b});

    auto first_rise = [](signal_t* s, long long t0, long long t1) -> long long {
        std::vector<size_t> idx;
        collect_indices_in_window(s, t0, t1, idx);
        char prev = '0';
        for (size_t ii : idx) {
            char vbuf[8];
            trace_vc_format_value(s, ii, vbuf, sizeof(vbuf));
            const char v = vbuf[0];
            if (v == '1' && prev == '0')
                return (long long)trace_vc_timestamp(s, ii);
            prev = v;
        }
        return -1;
    };

    const long long ra = first_rise(sig_a, window.t0, window.t1);
    const long long rb = first_rise(sig_b, window.t0, window.t1);
    std::ostringstream out;
    out << "=== Time skew (first rising edge) ===\n";
    if (ra < 0 || rb < 0) {
        out << "Could not find rising edges in range.\n";
        return out.str();
    }
    out << "A rise @" << ra << ", B rise @" << rb << ", skew B-A=" << (rb - ra) << "\n";
    return out.str();
}

std::string BuildFullReport(
    WaveformPanel* panel,
    const std::vector<signal_t*>& signals,
    const TimeWindow& window,
    bool include_compare)
{
    std::string s = BuildReport(panel, signals, window);
    s += "\n" + BuildBusSection(panel, signals, window);
    s += "\n" + BuildRulesSection(panel, signals, window);
    if (include_compare)
        s += "\n" + BuildCompareSection(panel, signals, window);
    return s;
}

} // namespace WaveformLocalStats
