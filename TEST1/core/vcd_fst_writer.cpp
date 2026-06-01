#include "core/vcd_fst_writer.h"

#ifndef BEAR2WAVE_FST_WRITER_STANDALONE
#include "core/trace_sidecar_idx.h"
#endif

#include "fstapi.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <unordered_map>
#include <vector>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#endif

namespace {

#define VCD_DEFS_BUF 512
#define READ_BUF (512u * 1024u)

struct SigDef {
    std::string id;
    std::string name;
    std::string module_path;
    unsigned width = 1;
    fstHandle handle = 0;
};

static void set_err(char* err_buf, size_t err_buf_len, const char* msg)
{
    if (!err_buf || err_buf_len == 0)
        return;
    strncpy(err_buf, msg ? msg : "", err_buf_len - 1);
    err_buf[err_buf_len - 1] = '\0';
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

static void fst_goto_scope(fstWriterContext* w, const std::string& target, std::vector<std::string>* cur)
{
    std::vector<std::string> target_parts;
    split_module_path(target.empty() ? "TOP" : target, target_parts);
    size_t common = 0;
    while (common < cur->size() && common < target_parts.size() && (*cur)[common] == target_parts[common])
        ++common;
    while (cur->size() > common) {
        fstWriterSetUpscope(w);
        cur->pop_back();
    }
    for (size_t i = common; i < target_parts.size(); ++i) {
        fstWriterSetScope(w, FST_ST_VCD_MODULE, target_parts[i].c_str(), "");
        cur->push_back(target_parts[i]);
    }
}

static int fst_timescale_exp(int scale, const char* unit)
{
    (void)scale;
    if (!unit || !unit[0])
        return -9;
    if (!strcmp(unit, "s"))
        return 0;
    if (!strcmp(unit, "ms"))
        return -3;
    if (!strcmp(unit, "us"))
        return -6;
    if (!strcmp(unit, "ns"))
        return -9;
    if (!strcmp(unit, "ps"))
        return -12;
    if (!strcmp(unit, "fs"))
        return -15;
    return -9;
}

static void update_module_path(char* cur, size_t cap, const char* name, bool upscope)
{
    if (!cur || cap == 0)
        return;
    if (upscope) {
        char* dot = strrchr(cur, '.');
        if (dot)
            *dot = '\0';
        else
            cur[0] = '\0';
        return;
    }
    if (!name || !name[0])
        return;
    const size_t cl = strlen(cur);
    const size_t nl = strlen(name);
    if (cl + nl + 2 > cap)
        return;
    if (cl > 0)
        snprintf(cur + cl, cap - cl, ".%s", name);
    else
        strncpy(cur, name, cap - 1);
    cur[cap - 1] = '\0';
}

static void fst_writer_close_safe(fstWriterContext* w)
{
    if (!w)
        return;
#ifdef _WIN32
    __try {
        fstWriterClose(w);
    } __except (EXCEPTION_EXECUTE_HANDLER) {
        /* vcpkg zlib + fstapi writer teardown can AV on Windows */
    }
#else
    fstWriterClose(w);
#endif
}

static int scan_line_field(FILE* f, char* dst, size_t cap)
{
    if (!f || !dst || cap < 2)
        return 0;
    char fmt[32];
    snprintf(fmt, sizeof(fmt), "\n%%%us", static_cast<unsigned>(cap - 1));
    return fscanf(f, fmt, dst) == 1 ? 1 : 0;
}

static int parse_defs(
    FILE* f,
    std::vector<SigDef>& sigs,
    std::unordered_map<std::string, fstHandle>* id_to_handle,
    char* date,
    size_t date_cap,
    char* version,
    size_t ver_cap,
    int* timescale_exp,
    uint64_t* data_offset)
{
    char instruction[VCD_DEFS_BUF];
    char current_module[VCD_DEFS_BUF] = {};
    int ch = 0;

    while ((ch = fgetc(f)) != EOF) {
        if (ch != '$')
            continue;
        if (fscanf(f, "%511s", instruction) != 1)
            return -1;

        if (!strcmp(instruction, "enddefinitions")) {
            const long pos = ftell(f);
            if (pos < 0)
                return -1;
            *data_offset = static_cast<uint64_t>(pos);
            return 0;
        }

        if (!strcmp(instruction, "end") || !strcmp(instruction, "dumpvars")
            || !strcmp(instruction, "dumpall") || !strcmp(instruction, "comment"))
            continue;

        if (!strcmp(instruction, "scope")) {
            char t[VCD_DEFS_BUF], n[VCD_DEFS_BUF];
            if (fscanf(f, " %511s %511s", t, n) == 2)
                update_module_path(current_module, sizeof(current_module), n, false);
            continue;
        }
        if (!strcmp(instruction, "upscope")) {
            update_module_path(current_module, sizeof(current_module), nullptr, true);
            continue;
        }
        if (!strcmp(instruction, "var")) {
            char type[VCD_DEFS_BUF];
            char sig_id[64];
            char sig_name[256];
            size_t width = 0;
            if (fscanf(f, " %511s %zu %63s %255[^ $]%*[^$]", type, &width, sig_id, sig_name) != 4)
                continue;
            SigDef sd;
            sd.id = sig_id;
            sd.name = sig_name;
            sd.module_path = current_module;
            sd.width = static_cast<unsigned>(std::max<size_t>(1, width));
            sigs.push_back(std::move(sd));
            continue;
        }
        if (!strcmp(instruction, "date") && date && date_cap > 1) {
            scan_line_field(f, date, date_cap);
            continue;
        }
        if (!strcmp(instruction, "version") && version && ver_cap > 1) {
            scan_line_field(f, version, ver_cap);
            continue;
        }
        if (!strcmp(instruction, "timescale") && timescale_exp) {
            size_t scale = 0;
            char unit[16] = {};
            if (fscanf(f, "\n%zu%15[^ \n$]", &scale, unit) == 2)
                *timescale_exp = fst_timescale_exp(static_cast<int>(scale), unit);
            continue;
        }
        while ((ch = fgetc(f)) != EOF && ch != '\n') {
        }
    }
    return -1;
}

static void process_line(
    const char* line,
    size_t len,
    uint64_t* cur_ts,
    bool* ts_ok,
    fstWriterContext* w,
    const std::unordered_map<std::string, fstHandle>& id_to_handle,
    uint64_t* last_emit_ts)
{
    while (len > 0 && isspace(static_cast<unsigned char>(*line))) {
        ++line;
        --len;
    }
    if (len == 0)
        return;

    if (line[0] == '#') {
        const char* p = line + 1;
        while (p < line + len && isspace(static_cast<unsigned char>(*p)))
            ++p;
        char buf[32];
        size_t i = 0;
        while (p < line + len && isdigit(static_cast<unsigned char>(*p)) && i + 1 < sizeof(buf))
            buf[i++] = *p++;
        buf[i] = '\0';
        if (i > 0) {
            *cur_ts = strtoull(buf, nullptr, 10);
            *ts_ok = true;
            if (*cur_ts != *last_emit_ts) {
                fstWriterEmitTimeChange(w, *cur_ts);
                *last_emit_ts = *cur_ts;
            }
        }
        return;
    }

    if (!*ts_ok || !strchr("-0123456789zZxXbBrR", line[0]))
        return;

    std::string sig_id;
    std::string value;
    const char* last_space = nullptr;
    for (size_t i = len; i > 0; --i) {
        if (line[i - 1] == ' ') {
            last_space = line + (i - 1);
            break;
        }
    }
    if (last_space) {
        value.assign(line, static_cast<size_t>(last_space - line));
        sig_id.assign(last_space + 1, len - static_cast<size_t>(last_space - line) - 1);
    } else {
        if (len < 2)
            return;
        value.assign(line, 1);
        sig_id.assign(line + 1, len - 1);
    }

    const auto it = id_to_handle.find(sig_id);
    if (it == id_to_handle.end() || it->second == 0)
        return;
    fstWriterEmitValueChange(w, it->second, value.c_str());
}

} // namespace

extern "C" int vcd_stream_write_fst(const char* vcd_path, const char* fst_path, char* err_buf, size_t err_buf_len)
{
    if (!vcd_path || !vcd_path[0] || !fst_path || !fst_path[0]) {
        set_err(err_buf, err_buf_len, "invalid vcd2fst paths");
        return -1;
    }

    FILE* in = fopen(vcd_path, "rb");
    if (!in) {
        set_err(err_buf, err_buf_len, "cannot open VCD input");
        return -1;
    }

    std::vector<SigDef> sigs;
    char date[128] = {};
    char version[128] = {};
    int ts_exp = -9;
    uint64_t data_offset = 0;
    if (parse_defs(in, sigs, nullptr, date, sizeof(date), version, sizeof(version), &ts_exp, &data_offset) != 0
        || data_offset == 0) {
        fclose(in);
        set_err(err_buf, err_buf_len, "VCD definitions parse failed");
        return -1;
    }

    fstWriterContext* w = fstWriterCreate(fst_path, 1);
    if (!w) {
        fclose(in);
        set_err(err_buf, err_buf_len, "fstWriterCreate failed");
        return -1;
    }
    if (date[0])
        fstWriterSetDate(w, date);
    if (version[0])
        fstWriterSetVersion(w, version);
    fstWriterSetTimescale(w, ts_exp);

    std::sort(sigs.begin(), sigs.end(), [](const SigDef& a, const SigDef& b) {
        const int cmp = a.module_path.compare(b.module_path);
        if (cmp != 0)
            return cmp < 0;
        return a.name < b.name;
    });

    std::unordered_map<std::string, fstHandle> id_to_handle;
    id_to_handle.reserve(sigs.size() * 2);
    std::vector<std::string> cur_scope;

    for (SigDef& sd : sigs) {
        fst_goto_scope(w, sd.module_path, &cur_scope);
        const unsigned width = std::max(1u, sd.width);
        sd.handle = fstWriterCreateVar(w, FST_VT_VCD_WIRE, FST_VD_IMPLICIT, width, sd.name.c_str(), 0);
        if (sd.handle == 0) {
            fst_writer_close_safe(w);
            fclose(in);
            set_err(err_buf, err_buf_len, "fstWriterCreateVar failed");
            return -1;
        }
        id_to_handle[sd.id] = sd.handle;
    }

    if (fseek(in, static_cast<long>(data_offset), SEEK_SET) != 0) {
        fst_writer_close_safe(w);
        fclose(in);
        set_err(err_buf, err_buf_len, "seek to VCD data section failed");
        return -1;
    }

    std::vector<char> buf(READ_BUF);
    std::string carry;
    uint64_t cur_ts = 0;
    bool ts_ok = false;
    uint64_t last_emit_ts = UINT64_MAX;

    while (true) {
        const size_t n = fread(buf.data(), 1, buf.size(), in);
        if (n == 0)
            break;
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
            process_line(carry.c_str(), carry.size(), &cur_ts, &ts_ok, w, id_to_handle, &last_emit_ts);
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
            process_line(line_start, static_cast<size_t>(line_end - line_start), &cur_ts, &ts_ok, w, id_to_handle,
                &last_emit_ts);
            line_start = nl + 1;
        }
    }
    if (!carry.empty())
        process_line(carry.c_str(), carry.size(), &cur_ts, &ts_ok, w, id_to_handle, &last_emit_ts);

    fst_writer_close_safe(w);
    fclose(in);

#if !defined(_WIN32) && !defined(BEAR2WAVE_FST_WRITER_STANDALONE)
    if (TraceSidecar::IdxCacheEnabled()) {
        fstReaderContext* ctx = fstReaderOpen(fst_path);
        if (ctx) {
            const auto blocks = TraceSidecar::CollectBlocksFromFstReader(ctx);
            const TraceSidecar::FileIdentity fid = TraceSidecar::QueryFileIdentity(fst_path);
            TraceSidecar::SaveFstBlocks(fst_path, fid, blocks);
            fstReaderClose(ctx);
        }
    }
#endif

    return 0;
}
