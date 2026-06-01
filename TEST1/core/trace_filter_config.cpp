#include "core/trace_filter_config.h"

#include <filesystem>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#else
#include <unistd.h>
#endif

namespace fs = std::filesystem;

static std::string path_str(const fs::path& p)
{
    return p.string();
}

static std::string trim_copy(const std::string& s)
{
    size_t b = 0;
    while (b < s.size() && (s[b] == ' ' || s[b] == '\t' || s[b] == '\r' || s[b] == '\n'))
        ++b;
    size_t e = s.size();
    while (e > b && (s[e - 1] == ' ' || s[e - 1] == '\t' || s[e - 1] == '\r' || s[e - 1] == '\n'))
        --e;
    return s.substr(b, e - b);
}

static std::string exe_directory()
{
#ifdef _WIN32
    char buf[MAX_PATH];
    const DWORD n = GetModuleFileNameA(nullptr, buf, MAX_PATH);
    if (n == 0 || n >= MAX_PATH)
        return {};
    return path_str(fs::path(buf).parent_path());
#else
    char buf[4096];
    const ssize_t n = readlink("/proc/self/exe", buf, sizeof(buf) - 1);
    if (n <= 0)
        return {};
    buf[n] = '\0';
    return path_str(fs::path(buf).parent_path());
#endif
}

static void push_if_exists(std::vector<std::string>* out, const fs::path& p)
{
    if (!out)
        return;
    std::error_code ec;
    if (p.empty() || !fs::exists(p, ec))
        return;
    const std::string s = path_str(p);
    for (const auto& existing : *out) {
        if (existing == s)
            return;
    }
    out->push_back(s);
}

static void push_tool_names(std::vector<std::string>* out, const fs::path& dir, const char* base)
{
    if (!out || !base || !base[0] || dir.empty())
        return;
    push_if_exists(out, dir / (std::string(base) + ".exe"));
    push_if_exists(out, dir / (std::string(base) + ".cmd"));
    push_if_exists(out, dir / (std::string(base) + ".bat"));
    push_if_exists(out, dir / base);
}

static void walk_tools_dir(std::vector<std::string>* out, const fs::path& start, const char* standard, const char* mock)
{
    if (!out || start.empty())
        return;
    fs::path p = start;
    for (int depth = 0; depth < 8 && !p.empty(); ++depth) {
        const fs::path tools = p / "tools";
        if (standard)
            push_tool_names(out, tools, standard);
        if (mock)
            push_tool_names(out, tools, mock);
        if (!p.has_parent_path() || p == p.parent_path())
            break;
        p = p.parent_path();
    }
}

static std::vector<std::string> filter_tool_candidates(const char* standard, const char* mock, const std::string& configured)
{
    std::vector<std::string> out;
    if (configured.empty() == false)
        push_if_exists(&out, fs::path(configured));

    const std::string exe_dir = exe_directory();
    if (!exe_dir.empty())
        walk_tools_dir(&out, fs::path(exe_dir), standard, mock);

    std::error_code ec;
    const fs::path cwd = fs::current_path(ec);
    if (!ec)
        walk_tools_dir(&out, cwd, standard, mock);

#ifdef _WIN32
    if (standard) {
        char buf[MAX_PATH];
        DWORD n = SearchPathA(nullptr, standard, ".exe", MAX_PATH, buf, nullptr);
        if (n > 0 && n < MAX_PATH)
            push_if_exists(&out, fs::path(buf));
        n = SearchPathA(nullptr, (std::string(standard) + ".cmd").c_str(), nullptr, MAX_PATH, buf, nullptr);
        if (n > 0 && n < MAX_PATH)
            push_if_exists(&out, fs::path(buf));
    }
    if (mock) {
        char buf[MAX_PATH];
        DWORD n = SearchPathA(nullptr, (std::string(mock) + ".cmd").c_str(), nullptr, MAX_PATH, buf, nullptr);
        if (n > 0 && n < MAX_PATH)
            push_if_exists(&out, fs::path(buf));
    }
#endif

    return out;
}

TraceFilterProcessConfig trace_filter_load_config()
{
    return trace_filter_from_external(trace_external_load_config());
}

int trace_filter_save_config(const TraceFilterProcessConfig& fc)
{
    TraceExternalConfig ext = trace_external_load_config();
    trace_filter_to_external(&ext, fc);
    return trace_external_save_config(ext);
}

void trace_filter_apply_probed_paths(TraceFilterProcessConfig* cfg)
{
    if (!cfg)
        return;
    if (cfg->translate_proc_path.empty())
        cfg->translate_proc_path = trace_filter_probe_translate_proc(cfg);
    if (cfg->transaction_proc_path.empty())
        cfg->transaction_proc_path = trace_filter_probe_transaction_proc(cfg);
}

std::string trace_filter_probe_translate_proc(const TraceFilterProcessConfig* cfg)
{
    TraceFilterProcessConfig owned;
    const TraceFilterProcessConfig* use = cfg;
    if (!use) {
        owned = trace_filter_load_config();
        use = &owned;
    }
    const auto candidates = filter_tool_candidates("translate_proc", "mock_translate_proc", use->translate_proc_path);
    return candidates.empty() ? std::string() : candidates.front();
}

std::string trace_filter_probe_transaction_proc(const TraceFilterProcessConfig* cfg)
{
    TraceFilterProcessConfig owned;
    const TraceFilterProcessConfig* use = cfg;
    if (!use) {
        owned = trace_filter_load_config();
        use = &owned;
    }
    const auto candidates = filter_tool_candidates("transaction_proc", "mock_transaction_proc", use->transaction_proc_path);
    return candidates.empty() ? std::string() : candidates.front();
}
