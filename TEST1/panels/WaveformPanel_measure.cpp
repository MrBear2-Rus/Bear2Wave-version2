#include "panels/WaveformPanel.hpp"

#include "core/trace_vc.h"

#include <climits>
#include <vector>

double WaveformPanel::ComputeFrequency(signal_t* sig)
{
    const size_t n = trace_vc_count(sig);
    if (!sig || n < 2)
        return 0;
    static thread_local char s_freqBuf[VCD_SIGNAL_SIZE];
    std::vector<long long> risingEdges;
    for (size_t i = 1; i < n; i++) {
        trace_vc_format_value(sig, i - 1, s_freqBuf, sizeof(s_freqBuf));
        char prev = ParseVcdValue(s_freqBuf);
        trace_vc_format_value(sig, i, s_freqBuf, sizeof(s_freqBuf));
        char curr = ParseVcdValue(s_freqBuf);
        if (prev == '0' && curr == '1')
            risingEdges.push_back((long long)trace_vc_timestamp(sig, i));
    }
    if (risingEdges.size() < 2)
        return 0;
    double period = (double)(risingEdges.back() - risingEdges.front()) / (double)(risingEdges.size() - 1);
    return period <= 0 ? 0 : 1.0 / period;
}

double WaveformPanel::ComputeDuty(signal_t* sig)
{
    const size_t n = trace_vc_count(sig);
    if (!sig || n < 2)
        return 0;
    static thread_local char s_dutyBuf[VCD_SIGNAL_SIZE];
    double highTime = 0, totalTime = 0;
    for (size_t i = 1; i < n; i++) {
        long long t1 = (long long)trace_vc_timestamp(sig, i - 1);
        long long t2 = (long long)trace_vc_timestamp(sig, i);
        trace_vc_format_value(sig, i - 1, s_dutyBuf, sizeof(s_dutyBuf));
        char val = ParseVcdValue(s_dutyBuf);
        if (val == '1')
            highTime += (double)(t2 - t1);
        totalTime += (double)(t2 - t1);
    }
    return totalTime <= 0 ? 0 : highTime / totalTime;
}
