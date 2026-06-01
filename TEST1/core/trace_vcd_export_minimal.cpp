#include "core/trace_vcd_export_minimal.h"

#include "core/trace_vc.h"
#include "trace_loader.h"
#include "vcd.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace {

static void set_err(char* err_buf, size_t err_buf_len, const char* msg)
{
    if (!err_buf || err_buf_len == 0)
        return;
    strncpy(err_buf, msg ? msg : "", err_buf_len - 1);
    err_buf[err_buf_len - 1] = '\0';
}

static std::string make_vcd_code(size_t idx)
{
    return "b2w" + std::to_string(idx);
}

static void split_module_path(const std::string& path, std::vector<std::string>& parts)
{
    parts.clear();
    size_t b = 0;
    while (b <= path.size()) {
        const size_t e = path.find('.', b);
        const std::string part = path.substr(b, e == std::string::npos ? std::string::npos : e - b);
        if (!part.empty())
            parts.push_back(part);
        if (e == std::string::npos)
            break;
        b = e + 1;
    }
    if (parts.empty())
        parts.push_back("TOP");
}

struct ExportEvent {
    uint64_t t = 0;
    const signal_t* sig = nullptr;
    char value[VCD_SIGNAL_SIZE] {};
};

static size_t value_index_at_or_before(const signal_t* sig, uint64_t t)
{
    const size_t n = trace_vc_count(sig);
    if (n == 0)
        return 0;
    size_t lo = 0;
    size_t hi = n;
    while (lo + 1 < hi) {
        const size_t mid = lo + (hi - lo) / 2;
        if (trace_vc_timestamp(sig, mid) <= t)
            lo = mid;
        else
            hi = mid;
    }
    if (trace_vc_timestamp(sig, lo) > t)
        return 0;
    return lo;
}

static void collect_window_events(
    const signal_t** signals,
    size_t signal_count,
    uint64_t t0,
    uint64_t t1,
    std::vector<ExportEvent>& events)
{
    events.clear();
    for (size_t si = 0; si < signal_count; ++si) {
        const signal_t* sig = signals[si];
        if (!sig)
            continue;
        vcd_ensure_signal_sorted(const_cast<signal_t*>(sig));
        const size_t n = trace_vc_count(sig);
        if (n == 0)
            continue;

        const size_t i0 = value_index_at_or_before(sig, t0);
        ExportEvent seed;
        seed.t = t0;
        seed.sig = sig;
        trace_vc_format_value(sig, i0, seed.value, sizeof(seed.value));
        events.push_back(seed);

        for (size_t i = i0 + 1; i < n; ++i) {
            const uint64_t ts = trace_vc_timestamp(sig, i);
            if (ts <= t0)
                continue;
            if (ts > t1)
                break;
            ExportEvent ev;
            ev.t = ts;
            ev.sig = sig;
            trace_vc_format_value(sig, i, ev.value, sizeof(ev.value));
            events.push_back(ev);
        }
    }

    std::sort(events.begin(), events.end(), [](const ExportEvent& a, const ExportEvent& b) {
        if (a.t != b.t)
            return a.t < b.t;
        return a.sig < b.sig;
    });
}

} // namespace

int trace_vcd_export_minimal(
    vcd_t* vcd,
    const signal_t** signals,
    size_t signal_count,
    const TraceVcdExportOptions& opts,
    std::string* out_vcd,
    char* err_buf,
    size_t err_buf_len)
{
    if (!out_vcd) {
        set_err(err_buf, err_buf_len, "null output");
        return -1;
    }
    out_vcd->clear();
    if (!vcd || !signals || signal_count == 0) {
        set_err(err_buf, err_buf_len, "no signals to export");
        return -1;
    }

    std::vector<signal_t*> mutable_sigs;
    mutable_sigs.reserve(signal_count);
    for (size_t i = 0; i < signal_count; ++i) {
        if (!signals[i]) {
            set_err(err_buf, err_buf_len, "null signal in export list");
            return -1;
        }
        mutable_sigs.push_back(const_cast<signal_t*>(signals[i]));
    }

    if (trace_uses_lazy_backend(vcd)) {
        const int rc = trace_load_signals(
            vcd, mutable_sigs.data(), mutable_sigs.size(), 0, TRACE_LOAD_T1_FULL);
        if (rc != 0) {
            set_err(err_buf, err_buf_len,
                rc == 1 ? "export cancelled while loading signals"
                        : "failed to load signal data for export");
            return -1;
        }
    }

    for (signal_t* sig : mutable_sigs)
        vcd_ensure_signal_sorted(sig);

    const uint64_t t0 = opts.t0;
    const uint64_t t1 = opts.t1 == UINT64_MAX ? TRACE_LOAD_T1_FULL : opts.t1;

    std::vector<ExportEvent> events;
    collect_window_events(signals, signal_count, t0, t1, events);
    if (events.empty()) {
        set_err(err_buf, err_buf_len,
            "no value changes in export window (widen time range or load trace data)");
        return -1;
    }

    std::string body;
    body.reserve(events.size() * 24 + 512);

    body += "$date\n    Bear2Wave transaction export\n$end\n";
    if (vcd->version[0])
        body += std::string("$version\n    ") + vcd->version + "\n$end\n";
    else
        body += "$version\n    Bear2Wave trace_vcd_export_minimal\n$end\n";

    const char* unit = vcd->timescale.unit[0] ? vcd->timescale.unit : "ns";
    char ts_line[64];
    snprintf(ts_line, sizeof(ts_line), "$timescale 1 %s $end\n", unit);
    body += ts_line;

    std::vector<const signal_t*> sorted;
    sorted.assign(signals, signals + signal_count);
    std::sort(sorted.begin(), sorted.end(), [](const signal_t* a, const signal_t* b) {
        const int cmp = strcmp(a->module_path, b->module_path);
        if (cmp != 0)
            return cmp < 0;
        return strcmp(a->name, b->name) < 0;
    });

    std::map<const signal_t*, std::string> codes;
    std::vector<std::string> cur_parts;
    for (size_t i = 0; i < sorted.size(); ++i) {
        const signal_t* sig = sorted[i];
        std::vector<std::string> target_parts;
        split_module_path(sig->module_path[0] ? sig->module_path : "TOP", target_parts);

        size_t common = 0;
        while (common < cur_parts.size() && common < target_parts.size()
               && cur_parts[common] == target_parts[common])
            ++common;
        while (cur_parts.size() > common) {
            body += "$upscope $end\n";
            cur_parts.pop_back();
        }
        for (size_t j = common; j < target_parts.size(); ++j) {
            body += "$scope module " + target_parts[j] + " $end\n";
            cur_parts.push_back(target_parts[j]);
        }

        const std::string code = make_vcd_code(i);
        codes[sig] = code;
        const unsigned width = static_cast<unsigned>(std::max<size_t>(1, sig->size));
        char var_line[256];
        snprintf(var_line, sizeof(var_line), "$var wire %u %s %s $end\n", width, code.c_str(), sig->name);
        body += var_line;
    }
    while (!cur_parts.empty()) {
        body += "$upscope $end\n";
        cur_parts.pop_back();
    }
    body += "$enddefinitions $end\n";

    uint64_t cur_t = UINT64_MAX;
    for (const ExportEvent& ev : events) {
        if (ev.t != cur_t) {
            char tline[48];
            snprintf(tline, sizeof(tline), "#%llu\n", static_cast<unsigned long long>(ev.t));
            body += tline;
            cur_t = ev.t;
        }
        const auto it = codes.find(ev.sig);
        if (it == codes.end())
            continue;
        const unsigned width = static_cast<unsigned>(std::max<size_t>(1, ev.sig->size));
        if (width <= 1) {
            const char ch = ev.value[0] ? ev.value[0] : '0';
            body += ch;
            body += it->second;
            body += '\n';
        } else {
            body += "b";
            body += ev.value;
            body += ' ';
            body += it->second;
            body += '\n';
        }
    }

    *out_vcd = std::move(body);
    return 0;
}
