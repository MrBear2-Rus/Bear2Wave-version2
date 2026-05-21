#include "waveform_analysis.h"

#include "core/waveform_perf.h"
#include "panels/WaveformPanel.hpp"
#include "trace_loader.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <algorithm>
#include <cstdio>
#include <sstream>

namespace WaveformAnalysis {

int MaxEdgesPerSignal()
{
    return std::max(20, WaveformPerf::EnvInt("BEAR2WAVE_AI_MAX_EDGES_PER_SIG", 200));
}

int MaxContextChars()
{
    return std::max(2000, WaveformPerf::EnvInt("BEAR2WAVE_AI_MAX_CONTEXT_CHARS", 30000));
}

bool IsPlaceholderApiKey(const std::string& key)
{
    if (key.empty())
        return true;
    if (key.find("your") != std::string::npos && key.find("key") != std::string::npos)
        return true;
    if (key == "your-deepseek-api-key-here")
        return true;
    return false;
}

void PrepareSignals(WaveformPanel* panel, const std::vector<signal_t*>& signals)
{
    if (!panel || signals.empty())
        return;

    vcd_t* vcd = panel->m_vcdData;
    if (!vcd)
        return;

    std::vector<signal_t*> need;
    need.reserve(signals.size());

    if (trace_uses_lazy_backend(vcd)) {
        const uint64_t fileMax = vcd->trace_max_timestamp > 0
            ? vcd->trace_max_timestamp
            : (uint64_t)std::max<long long>(panel->m_maxTimestamp, 1);
        const uint64_t v0 = (uint64_t)std::max<long long>(0, panel->m_timeOffset);
        const uint64_t span = (uint64_t)std::max<long long>(1, panel->m_displayTimeRange);
        uint64_t v1 = v0 + span;
        if (v1 > fileMax)
            v1 = fileMax;

        uint64_t t0 = 0, t1 = TRACE_LOAD_T1_FULL;
        trace_compute_padded_range(v0, v1, fileMax, WaveformPerf::TraceLoadMarginRatio(), &t0, &t1);

        for (signal_t* sig : signals) {
            if (!sig)
                continue;
            if (!sig->trace_data_loaded
                || sig->trace_loaded_t0 > t0
                || sig->trace_loaded_t1 < t1) {
                need.push_back(sig);
            }
        }
        if (!need.empty())
            trace_load_signals(vcd, need.data(), need.size(), t0, t1);
    }

    for (signal_t* sig : signals) {
        if (sig)
            vcd_ensure_signal_sorted(sig);
    }
}

static void append_signal_edges(
    std::ostringstream& out,
    signal_t* sig,
    long long t0,
    long long t1,
    int maxEdges)
{
    if (!sig)
        return;

    const char* label = sig->full_name[0] ? sig->full_name : sig->name;
    out << "Signal: " << label << " (module=" << sig->module_path << ", width=" << sig->size << ")\n";

    if (sig->changes_count == 0 || !sig->value_changes) {
        out << "  (no value changes loaded; add to wave view and pan to activity)\n";
        return;
    }

    vcd_ensure_signal_sorted(sig);
    const value_change_t* vec = sig->value_changes;
    const size_t count = sig->changes_count;

    std::vector<size_t> idx;
    idx.reserve(count);
    for (size_t i = 0; i < count; ++i) {
        const long long ts = (long long)vec[i].timestamp;
        if (ts >= t0 && ts <= t1)
            idx.push_back(i);
    }

    if (idx.empty()) {
        const char* atStart = nullptr;
        for (size_t i = 0; i < count; ++i) {
            if ((long long)vec[i].timestamp <= t0)
                atStart = vec[i].value;
            else
                break;
        }
        out << "  stable in window";
        if (atStart && atStart[0])
            out << ", value at window start: \"" << atStart << "\"";
        out << "\n";
        return;
    }

    size_t rising = 0;
    if (sig->size <= 1) {
        char prev = 0;
        bool havePrev = false;
        for (size_t k : idx) {
            const char v = vec[k].value[0];
            if (havePrev && v != prev && (v == '0' || v == '1') && (prev == '0' || prev == '1'))
                ++rising;
            prev = v;
            havePrev = true;
        }
        if (rising > 0)
            out << "  edges in window: ~" << rising << "\n";
    }

    const size_t n = idx.size();
    const size_t step = (n > (size_t)maxEdges) ? ((n + (size_t)maxEdges - 1) / (size_t)maxEdges) : 1;
    size_t emitted = 0;
    for (size_t j = 0; j < n && emitted < (size_t)maxEdges; j += step) {
        const size_t i = idx[j];
        char line[512];
        snprintf(line, sizeof(line), "  @ %llu: %s\n",
            (unsigned long long)vec[i].timestamp,
            vec[i].value);
        out << line;
        ++emitted;
    }
    if (n > (size_t)maxEdges)
        out << "  ... (" << n << " changes in window, showing " << emitted << " samples)\n";
}

std::string BuildContext(WaveformPanel* panel, const std::vector<signal_t*>& signals)
{
    if (!panel)
        return {};

    std::ostringstream out;
    const long long t0 = panel->m_timeOffset;
    const long long t1 = panel->m_timeOffset + std::max(1LL, panel->m_displayTimeRange);

    out << "=== Waveform context (Bear2Wave) ===\n";
    if (panel->m_vcdData) {
        out << "Timescale: " << panel->m_vcdData->timescale.scale << " " << panel->m_vcdData->timescale.unit << "\n";
        if (panel->m_vcdData->version[0])
            out << "Trace: " << panel->m_vcdData->version << "\n";
    }
    out << "Viewport: " << t0 << " .. " << t1 << " (sim time units)\n";
    out << "Playhead: " << panel->m_currentTimestamp << "\n";
    if (panel->m_hasMarkerA && panel->m_markerB >= 0 && panel->m_markerA >= 0) {
        out << "Markers: A=" << panel->m_markerA << " B=" << panel->m_markerB
            << " delta=" << (panel->m_markerB - panel->m_markerA) << "\n";
    }

    const int maxEdges = MaxEdgesPerSignal();
    for (signal_t* sig : signals)
        append_signal_edges(out, sig, t0, t1, maxEdges);

    std::string s = out.str();
    const int cap = MaxContextChars();
    if ((int)s.size() > cap) {
        s.resize((size_t)cap);
        s += "\n... (context truncated)\n";
    }
    return s;
}

} // namespace WaveformAnalysis
