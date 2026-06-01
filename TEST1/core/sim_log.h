#pragma once

#include <cstddef>
#include <string>
#include <vector>

struct SimLogLine {
    size_t lineNumber = 0;
    std::string text;
    long long timestamp = 0;
    bool hasTimestamp = false;
};

/** Parse a simulation log file; extracts timestamps when present. */
std::vector<SimLogLine> sim_log_parse_file(const std::string& path, std::string* error = nullptr);

/** Try to parse a timestamp prefix from one log line. */
bool sim_log_parse_timestamp(const std::string& line, long long* outTimestamp);
