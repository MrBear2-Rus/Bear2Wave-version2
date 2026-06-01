#include "panels/WaveformPanel.hpp"

#include "core/trace_gui_debug.h"
#include "core/trace_load_progress.h"
#include "core/trace_memory_budget.h"
#include "core/waveform_perf.h"
#include "trace_loader.h"

#include <wx/log.h>

void WaveformPanel::OnTraceLoadDebounceTimer(wxTimerEvent&)
{
    StartTraceLoadAsync();
}

void WaveformPanel::JoinTraceWorkerIfJoined()
{
    if (m_traceLoadThread.joinable())
        m_traceLoadThread.join();
}

void WaveformPanel::CancelTraceLoad()
{
    ++m_traceLoadEpoch;
    if (m_vcdData)
        trace_loader_request_cancel(m_vcdData);
}

void WaveformPanel::QueueTraceLoad()
{
    if (!m_vcdData || !trace_uses_lazy_backend(m_vcdData))
        return;
    if (!m_traceLoadDebounceTimer)
        return;
    m_traceLoadDebounceTimer->Start(WaveformPerf::TraceLoadDebounceMs(), wxTIMER_ONE_SHOT);
}

bool WaveformPanel::SignalNeedsLoadForRange(signal_t* sig, uint64_t t0, uint64_t t1)
{
    if (!sig || !sig->trace_data_loaded)
        return true;
    if (t0 == TRACE_LOAD_T0_FULL && t1 == TRACE_LOAD_T1_FULL)
        return sig->trace_loaded_t1 != TRACE_LOAD_T1_FULL;
    return sig->trace_loaded_t0 > t0 || sig->trace_loaded_t1 < t1;
}

void WaveformPanel::TraceLoadTimeRange(uint64_t* out_t0, uint64_t* out_t1) const
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

void WaveformPanel::EnsureTraceSignalLoadedSync(signal_t* sig)
{
    if (!sig || !m_vcdData || !trace_uses_lazy_backend(m_vcdData))
        return;
    trace_fst_log("SYNC_LOAD", "begin sig=%s join_worker=1", sig->full_name);
    JoinTraceWorkerIfJoined();
    uint64_t t0 = 0, t1 = TRACE_LOAD_T1_FULL;
    TraceLoadTimeRange(&t0, &t1);
    if (!SignalNeedsLoadForRange(sig, t0, t1)) {
        trace_fst_log("SYNC_LOAD", "skip sig=%s (already loaded)", sig->full_name);
        return;
    }
    const int rc = trace_load_signals(m_vcdData, &sig, 1, t0, t1);
    trace_fst_log("SYNC_LOAD", "done sig=%s rc=%d chg=%zu", sig->full_name, rc, (size_t)sig->changes_count);
}

void WaveformPanel::TraceLoadProgressBridge(void* user, const TraceLoadProgress& info)
{
    auto* panel = static_cast<WaveformPanel*>(user);
    if (!panel || !panel->m_traceLoadStatusFn)
        return;
    const uint64_t epoch = panel->m_traceLoadEpoch;
    panel->CallAfter([panel, epoch, info]() {
        if (epoch != panel->m_traceLoadEpoch || !panel->m_traceLoadStatusFn)
            return;
        panel->m_traceLoadStatusFn(info, false, 0);
    });
}

void WaveformPanel::StartTraceLoadAsync()
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
    trace_fst_log("ASYNC_LOAD", "start epoch=%llu signals=%zu range=[%llu,%llu]",
        (unsigned long long)epoch, need.size(), (unsigned long long)t0, (unsigned long long)t1);
    vcd_t* vcd = m_vcdData;
    if (m_traceLoadStatusFn) {
        TraceLoadProgress start;
        start.signal_count = static_cast<uint32_t>(need.size());
        m_traceLoadStatusFn(start, false, 0);
    }
    trace_loader_set_progress_callback(vcd, &WaveformPanel::TraceLoadProgressBridge, this);
    m_traceLoadThread = std::thread([this, epoch, vcd, need = std::move(need), t0, t1]() {
        trace_fst_log("ASYNC_LOAD", "worker begin epoch=%llu", (unsigned long long)epoch);
        const int rc = trace_load_signals(
            vcd, const_cast<signal_t**>(need.data()), need.size(), t0, t1);
        const size_t loaded = trace_count_loaded_changes(vcd);
        trace_fst_log("ASYNC_LOAD", "worker end epoch=%llu rc=%d loaded=%zu",
            (unsigned long long)epoch, rc, loaded);
        CallAfter([this, epoch, vcd, rc, t0, t1, loaded]() {
            trace_loader_set_progress_callback(vcd, nullptr, nullptr);
            if (epoch != m_traceLoadEpoch)
                return;
            if (rc == 1) {
                if (m_traceLoadStatusFn)
                    m_traceLoadStatusFn({}, true, loaded);
                wxLogMessage(wxT("波形数据加载已取消"));
                return;
            }
            if (rc < 0) {
                if (m_traceLoadStatusFn)
                    m_traceLoadStatusFn({}, true, loaded);
                wxLogWarning(wxT("波形数据加载失败"));
                return;
            }
            m_traceLoadPadT0 = t0;
            m_traceLoadPadT1 = t1;
            if (m_traceLoadStatusFn)
                m_traceLoadStatusFn({}, true, loaded);
            RequestDrawCacheRebuild();
            Refresh();
        });
    });
}
