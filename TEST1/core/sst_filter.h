#pragma once

#include "vcd.h"

#include <regex>
#include <string>
#include <vector>

/** GTKWave-style SST filter direction prefix (no-op when direction unknown). */
enum class SstDirFilter {
    RequireIn,
    RequireOut,
    RequireIO,
    ExcludeIn,
    ExcludeOut,
    ExcludeIO
};

struct SstRegexClause {
    std::regex pattern;
    std::string source;
};

/** Parsed SST filter expression (AND semantics across all clauses). */
struct SstFilterSpec {
    bool empty = true;
    std::vector<SstDirFilter> directions;
    std::vector<std::string> must_contain;
    std::vector<std::string> must_not_contain;
    std::vector<SstRegexClause> regexes;
    /** Non-empty when one or more /.../ clauses failed to compile. */
    std::string parse_error;
};

/** Parse filter text: +I+ +O+ ++tok++ --tok-- /regex/ plain substrings. */
SstFilterSpec sst_filter_parse(const std::string& text);

/** True when spec is empty or signal satisfies every clause. */
bool sst_filter_match(const SstFilterSpec& spec, const signal_t* sig);

/** Lowercase full_name (or name) for matching. */
std::string sst_filter_signal_name(const signal_t* sig);
