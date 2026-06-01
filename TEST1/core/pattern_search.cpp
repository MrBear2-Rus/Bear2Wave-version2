#include "core/pattern_search.h"

#include "core/ghw_state.h"
#include "core/trace_vc.h"

#include <algorithm>
#include <cctype>
#include <cstring>
#include <unordered_set>

namespace {

std::string lower_copy(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

std::string normalize_token(std::string s)
{
    s = lower_copy(std::move(s));
    if (s.size() >= 2 && s[0] == 'b')
        s.erase(0, 1);
    while (!s.empty() && s[0] == '0' && s.size() > 1)
        s.erase(0, 1);
    return s;
}

bool is_high_char(char c)
{
    return c == '1' || c == 'h' || c == 'H';
}

bool is_low_char(char c)
{
    return c == '0' || c == 'l' || c == 'L';
}

bool value_is_high(const char* v)
{
    if (!v || !v[0])
        return false;
    if (v[0] == 'b' || v[0] == 'B') {
        for (size_t i = 1; v[i]; ++i) {
            if (v[i] == '1')
                return true;
            if (v[i] != '0' && v[i] != 'x' && v[i] != 'z' && v[i] != 'X' && v[i] != 'Z')
                return true;
        }
        return false;
    }
    if (strlen(v) == 1)
        return is_high_char(v[0]);
    for (size_t i = 0; v[i]; ++i) {
        if (is_high_char(v[i]))
            return true;
    }
    return false;
}

bool value_is_low(const char* v)
{
    if (!v || !v[0])
        return true;
    if (v[0] == 'b' || v[0] == 'B') {
        for (size_t i = 1; v[i]; ++i) {
            if (v[i] != '0')
                return false;
        }
        return true;
    }
    if (strlen(v) == 1)
        return is_low_char(v[0]);
    for (size_t i = 0; v[i]; ++i) {
        if (!is_low_char(v[i]) && v[i] != 'x' && v[i] != 'z' && v[i] != 'X' && v[i] != 'Z')
            return false;
    }
    return true;
}

void format_at_index(const signal_t* sig, size_t idx, char* buf, size_t len)
{
    trace_vc_format_value(sig, idx, buf, len);
}

bool value_matches_arg(const char* formatted, const char* raw, const std::string& arg, bool substring)
{
    if (!arg.empty() && (!formatted || !formatted[0]) && (!raw || !raw[0]))
        return false;
    const std::string needle = normalize_token(arg);
    if (needle.empty())
        return true;

    auto check = [&](const char* s) {
        if (!s)
            return false;
        const std::string hay = normalize_token(s);
        if (substring)
            return hay.find(needle) != std::string::npos;
        return hay == needle;
    };

    if (check(formatted))
        return true;
    if (raw && raw != formatted)
        return check(raw);
    return false;
}

bool edge_rising(const char* prev, const char* curr)
{
    return value_is_low(prev) && value_is_high(curr);
}

bool edge_falling(const char* prev, const char* curr)
{
    return value_is_high(prev) && value_is_low(curr);
}

bool criterion_at_change(const signal_t* sig, size_t idx, const PatternCriterion& c)
{
    if (c.kind == PatternMatchKind::DontCare)
        return true;
    if (!sig || idx >= trace_vc_count(sig))
        return false;

    char curr[VCD_SIGNAL_SIZE] = {};
    format_at_index(sig, idx, curr, sizeof(curr));
    const char* raw = trace_vc_is_compact(sig) ? curr : trace_vc_legacy_ptr(sig)[idx].value;

    switch (c.kind) {
    case PatternMatchKind::AnyEdge:
        return idx > 0;
    case PatternMatchKind::RisingEdge:
    case PatternMatchKind::FallingEdge: {
        if (idx == 0)
            return false;
        char prev[VCD_SIGNAL_SIZE] = {};
        format_at_index(sig, idx - 1, prev, sizeof(prev));
        const char* prevRaw = trace_vc_is_compact(sig) ? prev : trace_vc_legacy_ptr(sig)[idx - 1].value;
        return c.kind == PatternMatchKind::RisingEdge
            ? edge_rising(prevRaw ? prevRaw : prev, raw ? raw : curr)
            : edge_falling(prevRaw ? prevRaw : prev, raw ? raw : curr);
    }
    case PatternMatchKind::High:
        return value_is_high(raw ? raw : curr);
    case PatternMatchKind::Low:
        return value_is_low(raw ? raw : curr);
    case PatternMatchKind::Value:
        return value_matches_arg(curr, raw, c.arg, false);
    case PatternMatchKind::String:
        return value_matches_arg(curr, raw, c.arg, true);
    default:
        return false;
    }
}

bool criterion_at_time(const signal_t* sig, uint64_t t, const PatternCriterion& c)
{
    if (c.kind == PatternMatchKind::DontCare)
        return true;
    if (!sig || trace_vc_count(sig) == 0)
        return false;

    switch (c.kind) {
    case PatternMatchKind::RisingEdge:
    case PatternMatchKind::FallingEdge:
    case PatternMatchKind::AnyEdge: {
        for (size_t i = 0; i < trace_vc_count(sig); ++i) {
            if (trace_vc_timestamp(sig, i) != t)
                continue;
            return criterion_at_change(sig, i, c);
        }
        return false;
    }
    default: {
        char* sampled = vcd_signal_get_value_at_timestamp(const_cast<signal_t*>(sig), (timestamp_t)t);
        if (!sampled)
            return false;
        char display[VCD_SIGNAL_SIZE] = {};
        bear2wave_nine_state_display_label(sampled, display, sizeof(display));
        switch (c.kind) {
        case PatternMatchKind::High:
            return value_is_high(sampled);
        case PatternMatchKind::Low:
            return value_is_low(sampled);
        case PatternMatchKind::Value:
            return value_matches_arg(display, sampled, c.arg, false);
        case PatternMatchKind::String:
            return value_matches_arg(display, sampled, c.arg, true);
        default:
            return false;
        }
    }
    }
}

bool spec_matches_at_time(const PatternSearchSpec& spec, uint64_t t)
{
    for (const auto& row : spec) {
        if (!row.first)
            return false;
        if (!criterion_at_time(row.first, t, row.second))
            return false;
    }
    return true;
}

void collect_candidate_times(
    const PatternSearchSpec& spec,
    uint64_t t0,
    uint64_t t1,
    std::vector<uint64_t>& out)
{
    std::unordered_set<uint64_t> uniq;
    for (const auto& row : spec) {
        const signal_t* sig = row.first;
        if (!sig)
            continue;
        const size_t n = trace_vc_count(sig);
        for (size_t i = 0; i < n; ++i) {
            const uint64_t ts = trace_vc_timestamp(sig, i);
            if (ts < t0 || ts > t1)
                continue;
            uniq.insert(ts);
        }
    }
    out.assign(uniq.begin(), uniq.end());
    std::sort(out.begin(), out.end());
}

} // namespace

const char* pattern_match_kind_label(PatternMatchKind kind)
{
    switch (kind) {
    case PatternMatchKind::DontCare: return "(Don't care)";
    case PatternMatchKind::RisingEdge: return "Rising Edge";
    case PatternMatchKind::FallingEdge: return "Falling Edge";
    case PatternMatchKind::AnyEdge: return "Any Edge";
    case PatternMatchKind::High: return "High";
    case PatternMatchKind::Low: return "Low";
    case PatternMatchKind::Value: return "Value";
    case PatternMatchKind::String: return "String";
    default: return "(Don't care)";
    }
}

PatternMatchKind pattern_match_kind_from_label(const std::string& label)
{
    static const struct {
        const char* text;
        PatternMatchKind kind;
    } kTable[] = {
        {"(Don't care)", PatternMatchKind::DontCare},
        {"Rising Edge", PatternMatchKind::RisingEdge},
        {"Falling Edge", PatternMatchKind::FallingEdge},
        {"Any Edge", PatternMatchKind::AnyEdge},
        {"High", PatternMatchKind::High},
        {"Low", PatternMatchKind::Low},
        {"Value", PatternMatchKind::Value},
        {"String", PatternMatchKind::String},
    };
    for (const auto& row : kTable) {
        if (label == row.text)
            return row.kind;
    }
    return PatternMatchKind::DontCare;
}

size_t pattern_collect_matches(
    const PatternSearchSpec& spec,
    uint64_t t0,
    uint64_t t1,
    std::vector<uint64_t>& out_times)
{
    out_times.clear();
    if (spec.empty())
        return 0;

    std::vector<uint64_t> candidates;
    collect_candidate_times(spec, t0, t1, candidates);
    for (uint64_t t : candidates) {
        if (spec_matches_at_time(spec, t))
            out_times.push_back(t);
    }
    return out_times.size();
}

bool pattern_find_from(
    const PatternSearchSpec& spec,
    uint64_t from_time,
    bool forward,
    int repeat_count,
    uint64_t& out_time,
    uint64_t t0,
    uint64_t t1)
{
    if (spec.empty() || repeat_count < 1)
        return false;

    std::vector<uint64_t> matches;
    pattern_collect_matches(spec, t0, t1, matches);
    if (matches.empty())
        return false;

    if (forward) {
        auto it = std::upper_bound(matches.begin(), matches.end(), from_time);
        if (it == matches.end())
            return false;
        it = std::next(it, repeat_count - 1);
        if (it == matches.end())
            return false;
        out_time = *it;
        return true;
    }

    auto it = std::lower_bound(matches.begin(), matches.end(), from_time);
    if (it != matches.end() && *it == from_time) {
        if (it == matches.begin())
            return false;
        --it;
    } else if (it == matches.begin()) {
        return false;
    } else {
        --it;
    }
    for (int i = 1; i < repeat_count; ++i) {
        if (it == matches.begin())
            return false;
        --it;
    }
    out_time = *it;
    return true;
}
