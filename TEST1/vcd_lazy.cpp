#include "vcd_lazy.h"

#include "core/bear2wave_log.h"
#include "core/trace_load_gaps.h"
#include "core/trace_blackout.h"
#include "core/vcd_sidecar_idx.h"
#include "core/vcd_recode_cache.h"

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <limits>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

#define VCD_LAZY_BUF_SIZE (512u * 1024u)
#define VCD_TAIL_SCAN_SIZE 8192
#define VCD_DEFS_BUFFER_LENGTH 512
#define VCD_TIME_INDEX_INTERVAL 2000
#define VCD_TIME_INDEX_MAX_ENTRIES 20000

typedef void (*VcdLoaderProgressFn)(void* user, uint32_t block_cur, uint32_t block_total, uint32_t signals, uint64_t elapsed_ms);

struct VcdTraceSession {
    FILE* file = nullptr;
    std::string path;
    uint64_t file_size = 0;
    uint64_t data_section_offset = 0;
    uint64_t max_timestamp = 0;
    vcd_t* owner = nullptr;
    std::unordered_map<std::string, signal_t*> id_to_sig;
    std::atomic<bool> cancel{false};
    std::vector<std::pair<uint64_t, uint64_t>> time_index;
    VcdLoaderProgressFn progress_fn = nullptr;
    void* progress_user = nullptr;
    size_t progress_signal_count = 0;
    std::chrono::steady_clock::time_point progress_start{};
    uint32_t progress_last_pct = UINT32_MAX;
    bool recode_ready = false;
    VcdRecodeCache::Meta recode_meta;
};

static void vcd_emit_progress(VcdTraceSession* sess, uint32_t block_cur, uint32_t block_total)
{
    if (!sess || !sess->progress_fn)
        return;
    const auto now = std::chrono::steady_clock::now();
    const uint64_t elapsed = sess->progress_start.time_since_epoch().count()
        ? static_cast<uint64_t>(
              std::chrono::duration_cast<std::chrono::milliseconds>(now - sess->progress_start).count())
        : 0;
    sess->progress_fn(
        sess->progress_user,
        block_cur,
        block_total,
        static_cast<uint32_t>(sess->progress_signal_count),
        elapsed);
}

static void update_module_path_lazy(char* current_module_path, size_t cap, const char* module_name, bool is_upscope)
{
    if (!current_module_path || cap == 0)
        return;

    if (is_upscope) {
        char* last_dot = strrchr(current_module_path, '.');
        if (last_dot)
            *last_dot = '\0';
        else
            memset(current_module_path, 0, cap);
        return;
    }

    if (!module_name || !module_name[0])
        return;

    const size_t curr_len = strlen(current_module_path);
    const size_t name_len = strlen(module_name);
    if (curr_len + name_len + 2 > cap)
        return;

    if (curr_len > 0)
        snprintf(current_module_path + curr_len, cap - curr_len, ".%s", module_name);
    else
        strncpy(current_module_path, module_name, cap - 1);
    current_module_path[cap - 1] = '\0';
}

static signal_node_t* append_signal_node_lazy(
    vcd_t* vcd,
    size_t width,
    const char* signal_id,
    const char* signal_name,
    const char* module_path)
{
    signal_node_t* new_node = static_cast<signal_node_t*>(calloc(1, sizeof(signal_node_t)));
    if (!new_node)
        return nullptr;

    signal_t* sig = &new_node->signal;
    memset(sig, 0, sizeof(signal_t));
    sig->fst_var_type = -1;
    sig->size = width;

    strncpy(sig->name, signal_name, VCD_NAME_SIZE - 1);
    sig->name[VCD_NAME_SIZE - 1] = '\0';
    strncpy(sig->signal_id, signal_id, VCD_NAME_SIZE - 1);
    sig->signal_id[VCD_NAME_SIZE - 1] = '\0';
    strncpy(sig->module_path, module_path, VCD_SIGNAL_SIZE - 1);
    sig->module_path[VCD_SIGNAL_SIZE - 1] = '\0';

    if (sig->module_path[0])
        snprintf(sig->full_name, VCD_SIGNAL_SIZE, "%s.%s", sig->module_path, sig->name);
    else
        strncpy(sig->full_name, sig->name, VCD_SIGNAL_SIZE - 1);
    sig->full_name[VCD_SIGNAL_SIZE - 1] = '\0';

    if (!vcd->signals_head) {
        vcd->signals_head = new_node;
    } else {
        signal_node_t* tail = vcd->signals_head;
        while (tail->next)
            tail = tail->next;
        tail->next = new_node;
    }
    vcd->signals_count++;
    return new_node;
}

static int parse_defs_section(FILE* file, vcd_t* vcd, VcdTraceSession* sess)
{
    char instruction[VCD_DEFS_BUFFER_LENGTH];
    char current_module_path[VCD_NAME_SIZE] = {};
    int character = 0;

    while ((character = fgetc(file)) != EOF) {
        if (character != '$')
            continue;

        if (fscanf(file, "%511s", instruction) != 1)
            return -1;

        if (strcmp(instruction, "enddefinitions") == 0) {
            const long pos = ftell(file);
            if (pos < 0)
                return -1;
            sess->data_section_offset = static_cast<uint64_t>(pos);
            return 0;
        }

        if (strcmp(instruction, "end") == 0
            || strcmp(instruction, "dumpvars") == 0
            || strcmp(instruction, "dumpall") == 0
            || strcmp(instruction, "comment") == 0)
        {
            continue;
        }

        if (strcmp(instruction, "scope") == 0) {
            char module_type[VCD_DEFS_BUFFER_LENGTH];
            char module_name[VCD_DEFS_BUFFER_LENGTH];
            if (fscanf(file, " %511s %511s", module_type, module_name) == 2)
                update_module_path_lazy(current_module_path, VCD_NAME_SIZE, module_name, false);
            continue;
        }

        if (strcmp(instruction, "upscope") == 0) {
            update_module_path_lazy(current_module_path, VCD_NAME_SIZE, nullptr, true);
            continue;
        }

        if (strcmp(instruction, "var") == 0) {
            char type[VCD_DEFS_BUFFER_LENGTH];
            char sig_id[VCD_NAME_SIZE];
            char sig_name[VCD_NAME_SIZE];
            size_t width = 0;

            const int ret = fscanf(file, " %511s %zu %31[^ ] %31[^ $]%*[^$]",
                type, &width, sig_id, sig_name);
            if (ret != 4)
                continue;

            signal_node_t* node = append_signal_node_lazy(vcd, width, sig_id, sig_name, current_module_path);
            if (node)
                sess->id_to_sig[std::string(sig_id)] = &node->signal;
            continue;
        }

        if (strcmp(instruction, "date") == 0) {
            fscanf(file, "\n%63[^$\n]", vcd->date);
            continue;
        }

        if (strcmp(instruction, "version") == 0) {
            fscanf(file, "\n%63[^$\n]", vcd->version);
            continue;
        }

        if (strcmp(instruction, "timescale") == 0) {
            fscanf(file, "\n%zu%7[^ \n$]", &vcd->timescale.scale, vcd->timescale.unit);
            continue;
        }

        int ch = 0;
        while ((ch = fgetc(file)) != EOF && ch != '\n') {
        }
    }

    return -1;
}

static uint64_t tail_scan_max_timestamp(FILE* file, uint64_t file_size)
{
    if (!file || file_size == 0)
        return 0;

    const long scan_size = (file_size < VCD_TAIL_SCAN_SIZE) ? static_cast<long>(file_size) : VCD_TAIL_SCAN_SIZE;
    if (scan_size <= 0)
        return 0;
    if (fseek(file, -scan_size, SEEK_END) != 0)
        return 0;

    std::vector<char> buf(static_cast<size_t>(scan_size) + 1u);
    const size_t n = fread(buf.data(), 1, static_cast<size_t>(scan_size), file);
    if (n == 0)
        return 0;
    buf[n] = '\0';

    const char* begin = buf.data();
    const char* end = begin + n;
    for (const char* p = end; p > begin; --p) {
        const char* s = p - 1;
        if (*s != '#')
            continue;
        if (s != begin && *(s - 1) != '\n' && *(s - 1) != '\r')
            continue;

        char ts_buf[32];
        size_t i = 0;
        const char* q = s + 1;
        while (q < end && isdigit(static_cast<unsigned char>(*q)) && i + 1 < sizeof(ts_buf))
            ts_buf[i++] = *q++;
        ts_buf[i] = '\0';
        if (i > 0)
            return strtoull(ts_buf, nullptr, 10);
    }

    return 0;
}

static void build_time_index(VcdTraceSession* sess)
{
    if (!sess || !sess->file || sess->data_section_offset == 0)
        return;
    if (sess->max_timestamp == 0)
        return;

    long save_pos = ftell(sess->file);
    if (fseek(sess->file, static_cast<long>(sess->data_section_offset), SEEK_SET) != 0)
        return;

    sess->time_index.clear();
    sess->time_index.reserve(static_cast<size_t>(VCD_TIME_INDEX_MAX_ENTRIES));

    std::vector<char> buf(VCD_LAZY_BUF_SIZE);
    uint64_t last_indexed_ts = 0;
    uint64_t line_ts = 0;
    bool in_timestamp = false;
    uint64_t file_offset = sess->data_section_offset;

    while (!sess->cancel.load(std::memory_order_relaxed) &&
           sess->time_index.size() < static_cast<size_t>(VCD_TIME_INDEX_MAX_ENTRIES)) {
        const size_t n = fread(buf.data(), 1, buf.size(), sess->file);
        if (n == 0)
            break;

        const char* p = buf.data();
        const char* end = p + n;

        while (p < end) {
            if (in_timestamp) {
                if (isdigit(static_cast<unsigned char>(*p))) {
                    line_ts = line_ts * 10 + static_cast<uint64_t>(*p - '0');
                } else {
                    in_timestamp = false;
                    if (sess->time_index.empty() || line_ts > last_indexed_ts + VCD_TIME_INDEX_INTERVAL) {
                        const uint64_t entry_offset = file_offset +
                            static_cast<uint64_t>(p - buf.data());
                        sess->time_index.emplace_back(line_ts, entry_offset);
                        last_indexed_ts = line_ts;
                    }
                }
                ++p;
            } else if (*p == '#') {
                line_ts = 0;
                in_timestamp = true;
                ++p;
            } else {
                while (p < end && *p != '\n') ++p;
                if (p < end) ++p;
            }
        }

        file_offset += static_cast<uint64_t>(n);
    }

    fseek(sess->file, save_pos, SEEK_SET);
}

static void scan_vcd_blackout(VcdTraceSession* sess, vcd_t* vcd)
{
    if (!sess || !sess->file || !vcd || sess->data_section_offset == 0)
        return;

    long save_pos = ftell(sess->file);
    if (fseek(sess->file, static_cast<long>(sess->data_section_offset), SEEK_SET) != 0)
        return;

    std::vector<char> buf(VCD_LAZY_BUF_SIZE);
    uint64_t current_timestamp = 0;
    bool timestamp_valid = false;
    std::string line;
    line.reserve(256);

    auto flush_line = [&]() {
        if (line.empty())
            return;
        while (!line.empty() && isspace(static_cast<unsigned char>(line.front())))
            line.erase(line.begin());
        if (line.empty())
            return;

        if (line[0] == '#') {
            const char* p = line.c_str() + 1;
            while (*p && isspace(static_cast<unsigned char>(*p)))
                ++p;
            if (isdigit(static_cast<unsigned char>(*p))) {
                current_timestamp = strtoull(p, nullptr, 10);
                timestamp_valid = true;
            }
            line.clear();
            return;
        }

        if (line.size() >= 8 && line[0] == '$' && timestamp_valid) {
            if (line == "$dumpoff") {
                if (!vcd->trace_blackout)
                    vcd->trace_blackout = trace_blackout_create();
                trace_blackout_on_dumpoff(static_cast<TraceBlackoutStore*>(vcd->trace_blackout), current_timestamp);
            } else if (line == "$dumpon" && vcd->trace_blackout) {
                trace_blackout_on_dumpon(static_cast<TraceBlackoutStore*>(vcd->trace_blackout), current_timestamp);
            }
        }
        line.clear();
    };

    while (!sess->cancel.load(std::memory_order_relaxed)) {
        const size_t n = fread(buf.data(), 1, buf.size(), sess->file);
        if (n == 0)
            break;
        for (size_t i = 0; i < n; ++i) {
            const char c = buf[i];
            if (c == '\n' || c == '\r') {
                flush_line();
            } else {
                line.push_back(c);
            }
        }
    }
    flush_line();

    fseek(sess->file, save_pos, SEEK_SET);
}

struct DataScanState {
    uint64_t current_timestamp = 0;
    bool timestamp_valid = false;
};

static void process_data_line(
    const char* line,
    size_t line_len,
    DataScanState* st,
    VcdTraceSession* sess,
    const std::unordered_set<std::string>& wanted_ids,
    uint64_t t0,
    uint64_t t1)
{
    if (!line || line_len == 0 || !st || !sess)
        return;

    while (line_len > 0 && isspace(static_cast<unsigned char>(*line))) {
        ++line;
        --line_len;
    }
    if (line_len == 0)
        return;

    if (line[0] == '#') {
        const char* p = line + 1;
        while (p < line + line_len && isspace(static_cast<unsigned char>(*p)))
            ++p;
        if (p >= line + line_len || !isdigit(static_cast<unsigned char>(*p)))
            return;

        char ts_buf[32];
        size_t i = 0;
        while (p < line + line_len && isdigit(static_cast<unsigned char>(*p)) && i + 1 < sizeof(ts_buf))
            ts_buf[i++] = *p++;
        ts_buf[i] = '\0';
        st->current_timestamp = strtoull(ts_buf, nullptr, 10);
        st->timestamp_valid = true;
        return;
    }

    if (line_len >= 8 && line[0] == '$' && st->timestamp_valid && sess && sess->owner) {
        if (line_len == 8 && memcmp(line, "$dumpoff", 8) == 0) {
            if (!sess->owner->trace_blackout)
                sess->owner->trace_blackout = trace_blackout_create();
            trace_blackout_on_dumpoff(static_cast<TraceBlackoutStore*>(sess->owner->trace_blackout),
                st->current_timestamp);
            return;
        }
        if (line_len == 7 && memcmp(line, "$dumpon", 7) == 0 && sess->owner->trace_blackout) {
            trace_blackout_on_dumpon(static_cast<TraceBlackoutStore*>(sess->owner->trace_blackout),
                st->current_timestamp);
            return;
        }
    }

    if (!st->timestamp_valid)
        return;
    if (!strchr("-0123456789zZxXbBrR", line[0]))
        return;

    std::string sig_id;
    std::string value;
    const char* last_space = nullptr;
    for (size_t i = line_len; i > 0; --i) {
        if (line[i - 1] == ' ') {
            last_space = line + (i - 1);
            break;
        }
    }

    if (last_space) {
        value.assign(line, static_cast<size_t>(last_space - line));
        sig_id.assign(last_space + 1, line_len - static_cast<size_t>(last_space - line) - 1);
    } else {
        if (line_len < 2)
            return;
        value.assign(line, 1);
        sig_id.assign(line + 1, line_len - 1);
    }

    if (!wanted_ids.empty() && wanted_ids.find(sig_id) == wanted_ids.end())
        return;

    const uint64_t ts = st->current_timestamp;
    if (ts < t0 || ts > t1)
        return;

    auto it = sess->id_to_sig.find(sig_id);
    if (it == sess->id_to_sig.end() || !it->second)
        return;

    vcd_signal_append_change_lazy(it->second, static_cast<timestamp_t>(ts), value.c_str());
}

static int scan_data_section(
    VcdTraceSession* sess,
    const std::unordered_set<std::string>& wanted_ids,
    uint64_t t0,
    uint64_t t1)
{
    if (!sess || !sess->file)
        return -1;
    if (sess->data_section_offset > static_cast<uint64_t>(std::numeric_limits<long>::max()))
        return -1;

    uint64_t seek_offset = sess->data_section_offset;
    if (t0 > 0 && !sess->time_index.empty()) {
        auto it = std::lower_bound(
            sess->time_index.begin(), sess->time_index.end(), t0,
            [](const std::pair<uint64_t, uint64_t>& entry, uint64_t ts) { return entry.first < ts; });
        if (it != sess->time_index.begin())
            --it;
        if (it->first <= t0)
            seek_offset = it->second;
    }

    if (fseek(sess->file, static_cast<long>(seek_offset), SEEK_SET) != 0)
        return -1;

    DataScanState st;
    std::vector<char> buf(VCD_LAZY_BUF_SIZE);
    std::string carry;
    const uint64_t scan_span = (sess->file_size > seek_offset) ? (sess->file_size - seek_offset) : 1;

    while (!sess->cancel.load(std::memory_order_relaxed)) {
        const size_t n = fread(buf.data(), 1, buf.size(), sess->file);
        if (n == 0)
            break;

        if (sess->progress_fn && scan_span > 0) {
            const long pos = ftell(sess->file);
            if (pos >= 0) {
                const uint64_t done = static_cast<uint64_t>(pos) - seek_offset;
                const uint32_t pct = static_cast<uint32_t>(
                    std::min<uint64_t>(99, (done * 100) / scan_span));
                if (pct != sess->progress_last_pct) {
                    sess->progress_last_pct = pct;
                    vcd_emit_progress(sess, pct, 100);
                }
            }
        }

        const char* p = buf.data();
        const char* end = p + n;
        const char* line_start = p;

        if (!carry.empty()) {
            const char* nl = static_cast<const char*>(memchr(p, '\n', static_cast<size_t>(end - p)));
            if (!nl) {
                carry.append(p, static_cast<size_t>(end - p));
                continue;
            }
            carry.append(p, static_cast<size_t>(nl - p));
            process_data_line(carry.c_str(), carry.size(), &st, sess, wanted_ids, t0, t1);
            carry.clear();
            line_start = nl + 1;
        }

        while (line_start < end) {
            const char* nl = static_cast<const char*>(memchr(line_start, '\n', static_cast<size_t>(end - line_start)));
            if (!nl) {
                carry.assign(line_start, static_cast<size_t>(end - line_start));
                break;
            }

            const char* line_end = nl;
            if (line_end > line_start && *(line_end - 1) == '\r')
                --line_end;

            process_data_line(
                line_start,
                static_cast<size_t>(line_end - line_start),
                &st,
                sess,
                wanted_ids,
                t0,
                t1);

            if (st.timestamp_valid && st.current_timestamp > t1)
                goto done;

            line_start = nl + 1;
        }
    }

done:
    if (sess->cancel.load(std::memory_order_relaxed))
        return 1;
    return 0;
}

static VcdTraceSession* session_from_vcd(vcd_t* vcd)
{
    if (!vcd || vcd->trace_backend != VCD_TRACE_BACKEND_VCD_LAZY || !vcd->trace_session)
        return nullptr;
    return static_cast<VcdTraceSession*>(vcd->trace_session);
}

extern "C" vcd_t* vcd_open_lazy(const char* utf8_path)
{
    if (!utf8_path || !utf8_path[0])
        return nullptr;

    FILE* file = fopen(utf8_path, "rb");
    if (!file)
        return nullptr;

    if (fseek(file, 0, SEEK_END) != 0) {
        fclose(file);
        return nullptr;
    }
    const long fsize = ftell(file);
    rewind(file);
    if (fsize <= 0) {
        fclose(file);
        return nullptr;
    }

    vcd_t* vcd = vcd_alloc_empty();
    if (!vcd) {
        fclose(file);
        return nullptr;
    }

    auto* sess = new VcdTraceSession();
    sess->file = file;
    sess->path = utf8_path;
    sess->file_size = static_cast<uint64_t>(fsize);

    if (parse_defs_section(file, vcd, sess) != 0 || sess->data_section_offset == 0) {
        vcd->trace_session = nullptr;
        fclose(file);
        delete sess;
        free(vcd);
        return nullptr;
    }

    sess->max_timestamp = tail_scan_max_timestamp(file, sess->file_size);
    setvbuf(file, nullptr, _IOFBF, static_cast<size_t>(VCD_LAZY_BUF_SIZE));

    if (VcdRecodeCache::Enabled()) {
        if (VcdRecodeCache::LoadMeta(utf8_path, sess->data_section_offset, &sess->recode_meta)) {
            sess->recode_ready = true;
            sess->max_timestamp = std::max(sess->max_timestamp, sess->recode_meta.max_timestamp);
            VcdRecodeCache::ApplyBlackout(vcd, sess->recode_meta);
            B2W_LOG_INFO(
                "VCD recode cache hit \"%s\" blocks=%zu max_ts=%llu",
                VcdRecodeCache::PathForVcd(utf8_path).c_str(),
                sess->recode_meta.blocks.size(),
                static_cast<unsigned long long>(sess->max_timestamp));
        } else {
            const int rc = VcdRecodeCache::BuildAndSave(
                utf8_path,
                file,
                sess->file_size,
                sess->data_section_offset,
                vcd,
                sess->id_to_sig,
                &sess->cancel,
                sess->progress_fn,
                sess->progress_user);
            if (rc == 0 && VcdRecodeCache::LoadMeta(utf8_path, sess->data_section_offset, &sess->recode_meta)) {
                sess->recode_ready = true;
                sess->max_timestamp = std::max(sess->max_timestamp, sess->recode_meta.max_timestamp);
            } else if (rc == 1) {
                vcd->trace_session = nullptr;
                fclose(file);
                delete sess;
                free(vcd);
                return nullptr;
            }
        }
    }

    if (!sess->recode_ready) {
        if (!VcdSidecar::LoadTimeIndex(utf8_path, sess->data_section_offset, sess->time_index)) {
            build_time_index(sess);
            if (!sess->time_index.empty())
                VcdSidecar::SaveTimeIndex(utf8_path, sess->data_section_offset, sess->time_index);
        } else {
            B2W_LOG_DEBUG(
                "VCD time index loaded from sidecar (%zu entries) for \"%s\"",
                sess->time_index.size(),
                utf8_path);
        }
        scan_vcd_blackout(sess, vcd);
    }

    vcd->trace_session = sess;
    sess->owner = vcd;
    vcd->trace_backend = VCD_TRACE_BACKEND_VCD_LAZY;
    vcd->trace_max_timestamp = sess->max_timestamp;

    B2W_LOG_INFO(
        "VCD lazy open \"%s\" size=%llu signals=%zu data_offset=%llu max_ts=%llu index=%zu",
        utf8_path,
        static_cast<unsigned long long>(sess->file_size),
        static_cast<size_t>(vcd->signals_count),
        static_cast<unsigned long long>(sess->data_section_offset),
        static_cast<unsigned long long>(sess->max_timestamp),
        sess->time_index.size());

    return vcd;
}

static bool vcd_signal_range_covers(const signal_t* sig, uint64_t t0, uint64_t t1)
{
    if (!sig || !sig->trace_data_loaded)
        return false;
    return sig->trace_loaded_t0 <= t0 && sig->trace_loaded_t1 >= t1;
}

extern "C" int vcd_load_signals(vcd_t* vcd, signal_t** sigs, size_t count, uint64_t t0, uint64_t t1)
{
    VcdTraceSession* sess = session_from_vcd(vcd);
    if (!sess || !sigs || count == 0)
        return -1;

    sess->cancel.store(false, std::memory_order_relaxed);
    sess->progress_signal_count = count;
    sess->progress_last_pct = UINT32_MAX;
    sess->progress_start = std::chrono::steady_clock::now();
    vcd_emit_progress(sess, 0, 100);

    uint64_t batch_t0 = UINT64_MAX;
    uint64_t batch_t1 = 0;
    std::unordered_set<std::string> wanted_ids;
    std::vector<signal_t*> pending;
    wanted_ids.reserve(count * 2);
    pending.reserve(count);

    for (size_t i = 0; i < count; ++i) {
        signal_t* sig = sigs[i];
        if (!sig)
            continue;

        wanted_ids.insert(std::string(sig->signal_id));
        if (vcd_signal_range_covers(sig, t0, t1))
            continue;

        uint64_t g0 = UINT64_MAX;
        uint64_t g1 = 0;
        trace_compute_sig_gaps(sig, t0, t1, g0, g1);
        if (g0 != UINT64_MAX) {
            batch_t0 = std::min(batch_t0, g0);
            batch_t1 = std::max(batch_t1, g1);
            pending.push_back(sig);
        }
    }

    if (batch_t0 == UINT64_MAX) {
        vcd_emit_progress(sess, 100, 100);
        return 0;
    }

    const int rc = sess->recode_ready
        ? VcdRecodeCache::DecodeWindow(sess->recode_meta, sess->id_to_sig, wanted_ids, batch_t0, batch_t1, &sess->cancel)
        : scan_data_section(sess, wanted_ids, batch_t0, batch_t1);
    vcd_emit_progress(sess, 100, 100);
    if (rc != 0)
        return rc;

    for (signal_t* sig : pending) {
        vcd_ensure_signal_sorted(sig);
        trace_extend_loaded_span(sig, t0, t1);
        vcd_signal_shrink_to_fit(sig);
    }

    B2W_LOG_DEBUG(
        "VCD load_signals done: %zu signals window [%llu,%llu] scan [%llu,%llu] recode=%d",
        pending.size(),
        static_cast<unsigned long long>(t0),
        static_cast<unsigned long long>(t1),
        static_cast<unsigned long long>(batch_t0),
        static_cast<unsigned long long>(batch_t1),
        sess->recode_ready ? 1 : 0);

    return 0;
}

void vcd_loader_set_progress_callback(vcd_t* vcd, VcdLoaderProgressFn fn, void* user)
{
    if (VcdTraceSession* sess = session_from_vcd(vcd)) {
        sess->progress_fn = fn;
        sess->progress_user = user;
    }
}

extern "C" void vcd_lazy_session_destroy(void* session)
{
    auto* sess = static_cast<VcdTraceSession*>(session);
    if (!sess)
        return;
    if (sess->file)
        fclose(sess->file);
    delete sess;
}

extern "C" void vcd_loader_request_cancel(vcd_t* vcd)
{
    if (VcdTraceSession* sess = session_from_vcd(vcd))
        sess->cancel.store(true, std::memory_order_relaxed);
}
