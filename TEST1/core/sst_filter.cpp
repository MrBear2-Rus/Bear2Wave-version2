#include "core/sst_filter.h"

#include <algorithm>
#include <cctype>
#include <cstring>

namespace {

std::string to_lower_copy(std::string s)
{
    std::transform(s.begin(), s.end(), s.begin(),
        [](unsigned char c) { return (char)std::tolower(c); });
    return s;
}

bool contains_ci(const std::string& haystack, const std::string& needle)
{
    if (needle.empty())
        return true;
    const std::string h = to_lower_copy(haystack);
    const std::string n = to_lower_copy(needle);
    return h.find(n) != std::string::npos;
}

bool direction_ok(SstDirFilter dir, const signal_t* sig)
{
    (void)sig;
    /* VCD / current signal_t has no port direction; do not exclude signals. */
    (void)dir;
    return true;
}

bool starts_with(const std::string& s, size_t pos, const char* lit)
{
    const size_t n = std::strlen(lit);
    return pos + n <= s.size() && s.compare(pos, n, lit) == 0;
}

void skip_spaces(const std::string& s, size_t& i)
{
    while (i < s.size() && std::isspace((unsigned char)s[i]))
        ++i;
}

bool read_delimited(const std::string& s, size_t& i, char open, char close, std::string* out)
{
    if (i >= s.size() || s[i] != open)
        return false;
    ++i;
    const size_t start = i;
    while (i < s.size() && s[i] != close)
        ++i;
    if (i >= s.size())
        return false;
    *out = s.substr(start, i - start);
    ++i;
    return true;
}

} // namespace

std::string sst_filter_signal_name(const signal_t* sig)
{
    if (!sig)
        return {};
    if (sig->full_name[0])
        return sig->full_name;
    return sig->name;
}

SstFilterSpec sst_filter_parse(const std::string& text)
{
    SstFilterSpec spec;
    size_t i = 0;
    skip_spaces(text, i);
    if (i >= text.size())
        return spec;

    spec.empty = false;

    while (i < text.size()) {
        skip_spaces(text, i);
        if (i >= text.size())
            break;

        if (starts_with(text, i, "+I+")) {
            spec.directions.push_back(SstDirFilter::RequireIn);
            i += 3;
            continue;
        }
        if (starts_with(text, i, "+O+")) {
            spec.directions.push_back(SstDirFilter::RequireOut);
            i += 3;
            continue;
        }
        if (starts_with(text, i, "+IO+")) {
            spec.directions.push_back(SstDirFilter::RequireIO);
            i += 4;
            continue;
        }
        if (starts_with(text, i, "-I-")) {
            spec.directions.push_back(SstDirFilter::ExcludeIn);
            i += 3;
            continue;
        }
        if (starts_with(text, i, "-O-")) {
            spec.directions.push_back(SstDirFilter::ExcludeOut);
            i += 3;
            continue;
        }
        if (starts_with(text, i, "-IO-")) {
            spec.directions.push_back(SstDirFilter::ExcludeIO);
            i += 4;
            continue;
        }
        if (starts_with(text, i, "++")) {
            i += 2;
            std::string tok;
            const size_t start = i;
            while (i + 1 < text.size() && !(text[i] == '+' && text[i + 1] == '+')) {
                if (std::isspace((unsigned char)text[i]))
                    break;
                ++i;
            }
            if (i > start)
                tok = text.substr(start, i - start);
            if (i + 1 < text.size() && text[i] == '+' && text[i + 1] == '+')
                i += 2;
            if (!tok.empty())
                spec.must_contain.push_back(tok);
            continue;
        }
        if (starts_with(text, i, "--")) {
            i += 2;
            std::string tok;
            const size_t start = i;
            while (i + 1 < text.size() && !(text[i] == '-' && text[i + 1] == '-')) {
                if (std::isspace((unsigned char)text[i]))
                    break;
                ++i;
            }
            if (i > start)
                tok = text.substr(start, i - start);
            if (i + 1 < text.size() && text[i] == '-' && text[i + 1] == '-')
                i += 2;
            if (!tok.empty())
                spec.must_not_contain.push_back(tok);
            continue;
        }
        if (text[i] == '/') {
            std::string body;
            if (!read_delimited(text, i, '/', '/', &body)) {
                spec.parse_error = "unclosed /regex/";
                break;
            }
            try {
                SstRegexClause clause;
                clause.source = body;
                clause.pattern = std::regex(body, std::regex_constants::icase);
                spec.regexes.push_back(std::move(clause));
            } catch (const std::regex_error& ex) {
                spec.parse_error = std::string("regex error: ") + ex.what();
            }
            continue;
        }

        size_t start = i;
        while (i < text.size() && !std::isspace((unsigned char)text[i])
            && text[i] != '+' && text[i] != '-' && text[i] != '/')
            ++i;
        if (i == start) {
            ++i;
            continue;
        }
        spec.must_contain.push_back(text.substr(start, i - start));
    }

    if (spec.directions.empty() && spec.must_contain.empty() && spec.must_not_contain.empty()
        && spec.regexes.empty() && spec.parse_error.empty()) {
        spec.empty = true;
    }

    return spec;
}

bool sst_filter_match(const SstFilterSpec& spec, const signal_t* sig)
{
    if (spec.empty || !sig)
        return spec.empty;

    if (!spec.parse_error.empty())
        return false;

    const std::string name = sst_filter_signal_name(sig);

    for (SstDirFilter dir : spec.directions) {
        if (!direction_ok(dir, sig))
            return false;
    }

    for (const std::string& bad : spec.must_not_contain) {
        if (contains_ci(name, bad))
            return false;
    }

    for (const std::string& need : spec.must_contain) {
        if (!contains_ci(name, need))
            return false;
    }

    for (const SstRegexClause& re : spec.regexes) {
        if (!std::regex_search(name, re.pattern))
            return false;
    }

    return true;
}
