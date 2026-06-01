#include "core/sim_log.h"

#include <cctype>
#include <cstdlib>
#include <fstream>
#include <string>

namespace {

std::string trim_copy(const std::string& s)
{
    size_t b = 0;
    while (b < s.size() && std::isspace((unsigned char)s[b]))
        ++b;
    size_t e = s.size();
    while (e > b && std::isspace((unsigned char)s[e - 1]))
        --e;
    return s.substr(b, e - b);
}

bool starts_with_ci(const std::string& s, const char* lit)
{
    const size_t n = std::strlen(lit);
    if (s.size() < n)
        return false;
    for (size_t i = 0; i < n; ++i) {
        if (std::tolower((unsigned char)s[i]) != std::tolower((unsigned char)lit[i]))
            return false;
    }
    return true;
}

bool parse_digits(const char* p, long long* out)
{
    if (!p || !*p || !out)
        return false;
    while (*p && std::isspace((unsigned char)*p))
        ++p;
    if (!std::isdigit((unsigned char)*p))
        return false;
    char* end = nullptr;
    const long long v = std::strtoll(p, &end, 10);
    if (end == p)
        return false;
    *out = v;
    return true;
}

bool parse_unit_suffix(const char* p, long long base, long long* out)
{
    while (p && *p && std::isspace((unsigned char)*p))
        ++p;
    if (!p || !*p) {
        *out = base;
        return true;
    }
    std::string u;
    while (*p && std::isalpha((unsigned char)*p)) {
        u.push_back((char)std::tolower((unsigned char)*p));
        ++p;
    }
    long long mul = 1;
    if (u.empty() || u == "ns")
        mul = 1;
    else if (u == "ps" || u == "fs")
        mul = 0;
    else if (u == "us")
        mul = 1000;
    else if (u == "ms")
        mul = 1000000;
    else if (u == "s")
        mul = 1000000000;
    else
        return false;
    *out = (mul == 0) ? base : (base * mul);
    return true;
}

} // namespace

bool sim_log_parse_timestamp(const std::string& line, long long* outTimestamp)
{
    if (!outTimestamp)
        return false;
    const std::string s = trim_copy(line);
    if (s.empty())
        return false;

    if (s[0] == '#') {
        const char* p = s.c_str() + 1;
        while (*p && std::isspace((unsigned char)*p))
            ++p;
        return parse_digits(p, outTimestamp);
    }

    if (s[0] == '@')
        return parse_digits(s.c_str() + 1, outTimestamp);

    if (s[0] == '[') {
        const size_t end = s.find(']');
        if (end == std::string::npos || end <= 1)
            return false;
        return parse_digits(s.substr(1, end - 1).c_str(), outTimestamp);
    }

    if (starts_with_ci(s, "time")) {
        size_t i = 4;
        while (i < s.size() && std::isspace((unsigned char)s[i]))
            ++i;
        if (i >= s.size() || (s[i] != ':' && s[i] != '='))
            return false;
        ++i;
        return parse_digits(s.c_str() + i, outTimestamp);
    }

    if (std::isdigit((unsigned char)s[0])) {
        char* end = nullptr;
        const long long base = std::strtoll(s.c_str(), &end, 10);
        if (end == s.c_str())
            return false;
        return parse_unit_suffix(end, base, outTimestamp);
    }

    return false;
}

std::vector<SimLogLine> sim_log_parse_file(const std::string& path, std::string* error)
{
    std::vector<SimLogLine> lines;
    std::ifstream in(path, std::ios::binary);
    if (!in) {
        if (error)
            *error = "cannot open log file";
        return lines;
    }

    std::string row;
    size_t lineNo = 0;
    while (std::getline(in, row)) {
        ++lineNo;
        while (!row.empty() && (row.back() == '\r' || row.back() == '\n'))
            row.pop_back();

        SimLogLine entry;
        entry.lineNumber = lineNo;
        entry.text = row;
        long long ts = 0;
        if (sim_log_parse_timestamp(row, &ts)) {
            entry.timestamp = ts;
            entry.hasTimestamp = true;
        }
        lines.push_back(std::move(entry));
    }

    if (lines.empty() && error)
        *error = "log file is empty";
    return lines;
}
