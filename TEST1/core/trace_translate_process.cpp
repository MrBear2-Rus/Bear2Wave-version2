#include "core/trace_translate_process.h"

#include "core/trace_filter_config.h"
#include "core/trace_process_runner.h"
#include "core/trace_translate_debug.h"

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <list>
#include <mutex>
#include <unordered_map>

static std::mutex g_cache_mu;
static std::unordered_map<std::string, std::string> g_cache;
static std::list<std::string> g_lru;
static size_t g_cache_max = 4096;

static size_t cache_max_entries()
{
    const char* env = std::getenv("BEAR2WAVE_TRANSLATE_CACHE");
    if (!env || !env[0])
        return g_cache_max;
    try {
        const long v = std::stol(env);
        if (v > 0)
            return (size_t)v;
    } catch (...) {
    }
    return g_cache_max;
}

static std::string make_cache_key(const char* full_name, long long sim_time, const char* raw_value)
{
    return std::string(full_name ? full_name : "") + "\t" + std::to_string(sim_time) + "\t"
        + (raw_value ? raw_value : "");
}

static std::string trim_line(std::string s)
{
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t'))
        s.pop_back();
    size_t b = 0;
    while (b < s.size() && (s[b] == ' ' || s[b] == '\t'))
        ++b;
    return s.substr(b);
}

static bool cache_lookup(const std::string& key, std::string* out)
{
    std::lock_guard<std::mutex> lock(g_cache_mu);
    auto it = g_cache.find(key);
    if (it == g_cache.end())
        return false;
    g_lru.remove(key);
    g_lru.push_front(key);
    if (out)
        *out = it->second;
    return true;
}

static void cache_store(const std::string& key, const std::string& value)
{
    std::lock_guard<std::mutex> lock(g_cache_mu);
    auto it = g_cache.find(key);
    if (it != g_cache.end()) {
        it->second = value;
        g_lru.remove(key);
        g_lru.push_front(key);
        return;
    }
    const size_t max_n = cache_max_entries();
    while (g_cache.size() >= max_n && !g_lru.empty()) {
        const std::string evict = g_lru.back();
        g_lru.pop_back();
        g_cache.erase(evict);
    }
    g_cache[key] = value;
    g_lru.push_front(key);
}

void trace_translate_cache_clear()
{
    std::lock_guard<std::mutex> lock(g_cache_mu);
    g_cache.clear();
    g_lru.clear();
    trace_translate_error_log("cache_clear");
}

static std::string resolve_translate_proc()
{
    TraceFilterProcessConfig cfg = trace_filter_load_config();
    if (cfg.translate_proc_path.empty())
        cfg.translate_proc_path = trace_filter_probe_translate_proc(&cfg);
    trace_translate_error_log("resolve_proc path=\"%s\" timeout_ms=%d",
        cfg.translate_proc_path.c_str(), cfg.process_timeout_ms);
    return cfg.translate_proc_path;
}

int trace_translate_via_process(
    const char* full_name,
    long long sim_time,
    const char* raw_value,
    std::string* out_display,
    char* err_buf,
    size_t err_buf_len)
{
    auto set_err = [&](const char* msg) {
        if (err_buf && err_buf_len > 0) {
            std::snprintf(err_buf, err_buf_len, "%s", msg ? msg : "");
        }
    };

    if (!out_display) {
        set_err("null out_display");
        return -1;
    }
    out_display->clear();

    if (!full_name || !full_name[0]) {
        set_err("empty signal name");
        return -1;
    }
    if (!raw_value)
        raw_value = "";

    const std::string cache_key = make_cache_key(full_name, sim_time, raw_value);
    if (cache_lookup(cache_key, out_display)) {
        trace_translate_error_log("via_process CACHE sig=%s t=%lld raw=\"%s\" out=\"%s\"",
            full_name, (long long)sim_time, raw_value, out_display->c_str());
        return 0;
    }

    const std::string proc = resolve_translate_proc();
    if (proc.empty()) {
        set_err("translate_proc not configured");
        trace_translate_error_log("via_process FAIL rc=-2 sig=%s t=%lld raw=\"%s\" err=%s",
            full_name, (long long)sim_time, raw_value, err_buf && err_buf[0] ? err_buf : "not configured");
        return -2;
    }

    {
        std::error_code ec;
        if (!std::filesystem::exists(proc, ec)) {
            set_err("translate_proc not found");
            trace_translate_error_log("via_process FAIL rc=-2 sig=%s proc_missing=\"%s\"",
                full_name, proc.c_str());
            return -2;
        }
    }

    TraceFilterProcessConfig cfg = trace_filter_load_config();
    const int timeout_ms = cfg.process_timeout_ms > 0 ? cfg.process_timeout_ms : 500;

    std::string stdin_line = std::string(full_name) + "\t" + std::to_string(sim_time) + "\t" + raw_value + "\n";
    trace_translate_error_log("via_process RUN proc=\"%s\" stdin=%s",
        proc.c_str(), stdin_line.c_str());
    const TraceProcessResult run = trace_process_run(proc, stdin_line, timeout_ms);
    if (run.timed_out) {
        set_err("translate_proc timed out");
        trace_translate_error_log("via_process FAIL rc=-3 timeout_ms=%d stderr=\"%s\"",
            timeout_ms, run.stderr_text.c_str());
        return -3;
    }
    if (run.exit_code != 0) {
        if (!run.stderr_text.empty())
            set_err(run.stderr_text.c_str());
        else
            set_err("translate_proc failed");
        trace_translate_error_log("via_process FAIL rc=-4 exit=%d stdout=\"%s\" stderr=\"%s\"",
            run.exit_code, run.stdout_text.c_str(), run.stderr_text.c_str());
        return -4;
    }

    std::string display = trim_line(run.stdout_text);
    if (display.empty()) {
        set_err("translate_proc returned empty output");
        trace_translate_error_log("via_process FAIL rc=-5 exit=0 empty stdout stderr=\"%s\"",
            run.stderr_text.c_str());
        return -5;
    }

    *out_display = display;
    cache_store(cache_key, display);
    trace_translate_error_log("via_process OK sig=%s t=%lld raw=\"%s\" out=\"%s\"",
        full_name, (long long)sim_time, raw_value, display.c_str());
    return 0;
}
