#pragma once

#include <string>
#include <vector>

#include "vcd.h"

class WaveformPanel;

namespace WaveformLocalStats {

struct TimeWindow {
    long long t0 = 0;
    long long t1 = 0;
    bool valid = false;
};

struct SignalRow {
    std::string name;
    unsigned width = 0;
    size_t transitions = 0;
    int duty_percent = -1;
    long long period_median = 0;
    bool has_xz = false;
    size_t glitch_count = 0;
    long long reset_release_ts = -1;
    bool constant = false;
};

TimeWindow ViewportWindow(WaveformPanel* panel);
TimeWindow MarkerWindow(WaveformPanel* panel);

void PrepareForAnalysis(WaveformPanel* panel, const std::vector<signal_t*>& signals);

std::vector<SignalRow> ComputeRows(
    WaveformPanel* panel,
    const std::vector<signal_t*>& signals,
    const TimeWindow& window);

std::string BuildReport(
    WaveformPanel* panel,
    const std::vector<signal_t*>& signals,
    const TimeWindow& window);

/** Full local report: stats + bus + rules + optional compare. */
std::string BuildFullReport(
    WaveformPanel* panel,
    const std::vector<signal_t*>& signals,
    const TimeWindow& window,
    bool include_compare = false);

std::string BuildIntervalSummary(WaveformPanel* panel, const TimeWindow& window);

std::string BuildBusSection(
    WaveformPanel* panel,
    const std::vector<signal_t*>& signals,
    const TimeWindow& window,
    int max_samples_per_signal = 40);

std::string BuildRulesSection(
    WaveformPanel* panel,
    const std::vector<signal_t*>& signals,
    const TimeWindow& window);

std::string BuildSetupCheck(
    WaveformPanel* panel,
    signal_t* sig_a,
    signal_t* sig_b,
    const TimeWindow& window,
    long long max_response_delay);

std::string BuildCompareSection(
    WaveformPanel* panel,
    const std::vector<signal_t*>& signals,
    const TimeWindow& window);

std::string BuildTimeSkewEstimate(
    WaveformPanel* panel,
    signal_t* sig_a,
    signal_t* sig_b,
    const TimeWindow& window);

} // namespace WaveformLocalStats
