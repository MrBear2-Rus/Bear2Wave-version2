#pragma once

#include "vcd.h"

#include <cstddef>
#include <cstdint>
#include <string>
#include <utility>
#include <vector>

enum class PatternMatchKind {
    DontCare = 0,
    RisingEdge,
    FallingEdge,
    AnyEdge,
    High,
    Low,
    Value,
    String,
};

struct PatternCriterion {
    PatternMatchKind kind = PatternMatchKind::DontCare;
    std::string arg;
};

using PatternSearchSpec = std::vector<std::pair<signal_t*, PatternCriterion>>;

const char* pattern_match_kind_label(PatternMatchKind kind);
PatternMatchKind pattern_match_kind_from_label(const std::string& label);

/** Collect timestamps in [t0,t1] where all spec entries match simultaneously. */
size_t pattern_collect_matches(
    const PatternSearchSpec& spec,
    uint64_t t0,
    uint64_t t1,
    std::vector<uint64_t>& out_times);

/** Find the repeat_count-th match forward/backward from from_time (exclusive). */
bool pattern_find_from(
    const PatternSearchSpec& spec,
    uint64_t from_time,
    bool forward,
    int repeat_count,
    uint64_t& out_time,
    uint64_t t0 = 0,
    uint64_t t1 = UINT64_MAX);
