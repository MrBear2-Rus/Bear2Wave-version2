#include "core/trace_transaction_process.h"

#include "core/trace_filter_config.h"
#include "core/trace_process_runner.h"
#include "core/trace_translate_debug.h"
#include "core/trace_var_types.h"
#include "core/trace_vc.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <string>

namespace {

static void set_err(char* err_buf, size_t err_buf_len, const char* msg)
{
    if (!err_buf || err_buf_len == 0)
        return;
    strncpy(err_buf, msg ? msg : "", err_buf_len - 1);
    err_buf[err_buf_len - 1] = '\0';
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

static bool value_is_scalar_digital(const std::string& v)
{
    if (v.empty())
        return true;
    if (v.size() == 1) {
        const char c = v[0];
        return c == '0' || c == '1' || c == 'x' || c == 'X' || c == 'z' || c == 'Z';
    }
    return false;
}

static void split_gtkwave_color_value(const std::string& raw, std::string* color, std::string* text)
{
    if (!color || !text)
        return;
    color->clear();
    *text = raw;
    if (raw.size() >= 3 && raw[0] == '?') {
        const size_t q2 = raw.find('?', 1);
        if (q2 != std::string::npos) {
            *color = raw.substr(1, q2 - 1);
            *text = raw.substr(q2 + 1);
            return;
        }
    }
    const size_t q = raw.find('?');
    if (q != std::string::npos && q > 0) {
        *color = raw.substr(0, q);
        *text = raw.substr(q + 1);
    }
}

} // namespace

int trace_transaction_parse_stdout(
    const std::string& stdout_text,
    TransactionProcessResult* out,
    char* err_buf,
    size_t err_buf_len)
{
    if (!out) {
        set_err(err_buf, err_buf_len, "null output");
        return -1;
    }
    out->traces.clear();
    out->markers.clear();

    TransactionVirtualTrace* cur = nullptr;
    size_t line_no = 0;

    size_t pos = 0;
    while (pos <= stdout_text.size()) {
        const size_t eol = stdout_text.find('\n', pos);
        const std::string raw = stdout_text.substr(pos, eol == std::string::npos ? std::string::npos : eol - pos);
        pos = (eol == std::string::npos) ? stdout_text.size() + 1 : eol + 1;
        ++line_no;

        const std::string line = trim_line(raw);
        if (line.empty())
            continue;
        if (line.size() >= 2 && line[0] == '/' && line[1] == '/')
            continue;

        if (line.rfind("$name ", 0) == 0) {
            out->traces.emplace_back();
            cur = &out->traces.back();
            cur->name = trim_line(line.substr(6));
            continue;
        }
        if (line == "$finish") {
            cur = nullptr;
            continue;
        }
        if (line.rfind("$next", 0) == 0) {
            out->traces.emplace_back();
            cur = &out->traces.back();
            if (out->traces.size() >= 2 && !out->traces[out->traces.size() - 2].name.empty()) {
                cur->name = out->traces[out->traces.size() - 2].name + "_next";
            }
            continue;
        }

        if (line[0] == 'M' || line[0] == 'm') {
            const char* p = line.c_str() + 1;
            while (*p == ' ' || *p == '\t')
                ++p;
            char* endp = nullptr;
            const unsigned long long t = std::strtoull(p, &endp, 10);
            if (endp != p) {
                while (*endp == ' ' || *endp == '\t')
                    ++endp;
                TransactionMarker mk;
                mk.time = static_cast<uint64_t>(t);
                mk.label = trim_line(endp);
                if (!mk.label.empty())
                    out->markers.push_back(std::move(mk));
            }
            continue;
        }

        if (line[0] == '#') {
            if (line.size() < 2 || !std::isdigit(static_cast<unsigned char>(line[1])))
                continue;
            if (!cur) {
                set_err(err_buf, err_buf_len, "value line before $name");
                return -1;
            }
            char* endp = nullptr;
            const unsigned long long t = std::strtoull(line.c_str() + 1, &endp, 10);
            if (endp == line.c_str() + 1) {
                set_err(err_buf, err_buf_len, "invalid #time line");
                return -1;
            }
            while (*endp == ' ' || *endp == '\t')
                ++endp;
            TransactionTraceChange ch;
            ch.time = static_cast<uint64_t>(t);
            std::string color;
            split_gtkwave_color_value(trim_line(endp), &color, &ch.value);
            if (!color.empty() && cur->row_color.empty())
                cur->row_color = color;
            cur->changes.push_back(std::move(ch));
            continue;
        }

        trace_translate_error_log("transaction_parse skip unknown line %zu: %s", line_no, line.c_str());
    }

    out->traces.erase(
        std::remove_if(out->traces.begin(), out->traces.end(),
            [](const TransactionVirtualTrace& t) { return t.name.empty(); }),
        out->traces.end());

    if (out->traces.empty()) {
        set_err(err_buf, err_buf_len, "no traces in transaction output");
        return -1;
    }
    return 0;
}

void trace_transaction_fill_synthetic(
    const TransactionVirtualTrace& trace,
    signal_t* out_sig,
    std::vector<value_change_t>* storage)
{
    if (!out_sig || !storage)
        return;

    std::memset(out_sig, 0, sizeof(*out_sig));
    storage->clear();

    const std::string& label = trace.name;
    const char* dot = strrchr(label.c_str(), '.');
    const char* short_name = dot ? dot + 1 : label.c_str();

    std::strncpy(out_sig->name, short_name, sizeof(out_sig->name) - 1);
    std::strncpy(out_sig->full_name, label.c_str(), sizeof(out_sig->full_name) - 1);
    std::strncpy(out_sig->module_path, "TXN", sizeof(out_sig->module_path) - 1);
    snprintf(out_sig->signal_id, sizeof(out_sig->signal_id), "txn_%s", short_name);

    bool all_digital = !trace.changes.empty();
    size_t max_bits = 1;
    for (const TransactionTraceChange& ch : trace.changes) {
        if (!value_is_scalar_digital(ch.value)) {
            all_digital = false;
            max_bits = std::max(max_bits, ch.value.size());
        }
    }

    if (all_digital) {
        out_sig->size = 1;
        out_sig->fst_var_type = -1;
    } else {
        out_sig->size = std::max<size_t>(1, max_bits);
        out_sig->fst_var_type = BEAR2WAVE_VT_STRING;
    }

    storage->reserve(trace.changes.size());
    uint64_t tmin = UINT64_MAX;
    uint64_t tmax = 0;
    for (const TransactionTraceChange& ch : trace.changes) {
        value_change_t vc {};
        vc.timestamp = static_cast<timestamp_t>(ch.time);
        std::strncpy(vc.value, ch.value.c_str(), sizeof(vc.value) - 1);
        storage->push_back(vc);
        tmin = std::min(tmin, ch.time);
        tmax = std::max(tmax, ch.time);
    }

    out_sig->value_changes = storage->data();
    out_sig->changes_count = storage->size();
    out_sig->changes_capacity = storage->size();
    out_sig->vc_storage = TRACE_VC_STORAGE_FULL;
    out_sig->changes_sorted = 1;
    out_sig->trace_data_loaded = 1;
    out_sig->trace_loaded_t0 = storage->empty() ? 0 : tmin;
    out_sig->trace_loaded_t1 = storage->empty() ? TRACE_LOAD_T1_FULL : tmax;
}

int trace_transaction_run(
    const std::string& input_vcd,
    TransactionProcessResult* out,
    char* err_buf,
    size_t err_buf_len)
{
    if (!out) {
        set_err(err_buf, err_buf_len, "null output");
        return -1;
    }
    out->traces.clear();
    out->stderr_text.clear();
    out->exit_code = -1;
    out->timed_out = false;

    TraceFilterProcessConfig cfg = trace_filter_load_config();
    if (cfg.transaction_proc_path.empty())
        cfg.transaction_proc_path = trace_filter_probe_transaction_proc(&cfg);

    if (cfg.transaction_proc_path.empty()) {
        set_err(err_buf, err_buf_len, "transaction_proc not configured");
        trace_translate_error_log("transaction_run missing transaction_proc");
        return -2;
    }

    trace_translate_error_log("transaction_run path=\"%s\" stdin_bytes=%zu timeout_ms=%d",
        cfg.transaction_proc_path.c_str(), input_vcd.size(), cfg.process_timeout_ms);

    const TraceProcessResult proc = trace_process_run(
        cfg.transaction_proc_path, input_vcd, cfg.process_timeout_ms);

    out->exit_code = proc.exit_code;
    out->timed_out = proc.timed_out;
    out->stderr_text = proc.stderr_text;

    if (proc.timed_out) {
        set_err(err_buf, err_buf_len, "transaction_proc timed out");
        return -1;
    }
    if (proc.exit_code != 0) {
        set_err(err_buf, err_buf_len, "transaction_proc failed");
        return -1;
    }

    const int prc = trace_transaction_parse_stdout(proc.stdout_text, out, err_buf, err_buf_len);
    if (prc != 0)
        return prc;

    trace_translate_error_log("transaction_run ok traces=%zu", out->traces.size());
    return 0;
}
