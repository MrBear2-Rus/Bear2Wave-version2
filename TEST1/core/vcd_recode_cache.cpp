#include "core/vcd_recode_cache.h"

#include "core/bear2wave_log.h"
#include "core/trace_blackout.h"
#include "core/waveform_perf.h"
#include "vcd.h"

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstring>
#include <limits>
#include <vector>

#include <zlib.h>

namespace VcdRecodeCache {

namespace {

static constexpr char kMagic[8] = { 'B', 'W', 'V', 'R', 'E', 'C', '1', '\0' };
static constexpr char kBlackoutMagic[8] = { 'B', 'W', 'B', 'O', 'T', '1', '\0' };
static constexpr uint32_t kVersion = 1;
static constexpr size_t kHeaderSize = 128;
static constexpr size_t kBlockUncompressedLimit = 1u << 20;
static constexpr size_t kReadBufSize = 512u * 1024u;

enum RecordType : uint8_t {
    REC_TIMESTAMP = 0,
    REC_SCALAR = 1,
    REC_VECTOR = 2,
    REC_DUMPOFF = 3,
    REC_DUMPON = 4,
};

static void append_varint(std::vector<uint8_t>& buf, uint64_t v)
{
    while (v >= 0x80) {
        buf.push_back(static_cast<uint8_t>((v & 0x7f) | 0x80));
        v >>= 7;
    }
    buf.push_back(static_cast<uint8_t>(v));
}

static bool read_varint(const uint8_t*& p, const uint8_t* end, uint64_t& out)
{
    out = 0;
    unsigned shift = 0;
    while (p < end) {
        const uint8_t b = *p++;
        out |= static_cast<uint64_t>(b & 0x7f) << shift;
        if ((b & 0x80) == 0)
            return true;
        shift += 7;
        if (shift > 63)
            return false;
    }
    return false;
}

static bool zlib_compress_buf(const uint8_t* src, size_t src_len, std::vector<uint8_t>& dst)
{
    uLongf cap = compressBound(static_cast<uLong>(src_len));
    dst.resize(cap);
    const int rc = compress2(dst.data(), &cap, src, static_cast<uLong>(src_len), Z_DEFAULT_COMPRESSION);
    if (rc != Z_OK)
        return false;
    dst.resize(cap);
    return true;
}

static bool zlib_decompress_buf(const uint8_t* src, size_t src_len, std::vector<uint8_t>& dst)
{
    size_t cap = std::max<size_t>(src_len * 8, 65536);
    for (int attempt = 0; attempt < 10; ++attempt) {
        dst.resize(cap);
        uLongf out_len = static_cast<uLongf>(cap);
        const int rc = uncompress(dst.data(), &out_len, src, static_cast<uLong>(src_len));
        if (rc == Z_OK) {
            dst.resize(out_len);
            return true;
        }
        if (rc != Z_BUF_ERROR)
            return false;
        cap *= 2;
    }
    return false;
}

static void build_signal_table(
    vcd_t* vcd,
    std::vector<std::string>& ids,
    std::vector<uint16_t>& widths,
    std::unordered_map<std::string, uint16_t>& id_to_idx)
{
    ids.clear();
    widths.clear();
    id_to_idx.clear();
    if (!vcd)
        return;
    for (signal_node_t* n = vcd->signals_head; n; n = n->next) {
        const uint16_t idx = static_cast<uint16_t>(ids.size());
        ids.emplace_back(n->signal.signal_id);
        widths.push_back(static_cast<uint16_t>(std::min<size_t>(n->signal.size, 65535)));
        id_to_idx[ids.back()] = idx;
    }
}

static void snapshot_blackout(vcd_t* vcd, BlackoutSnapshot& out)
{
    out.spans.clear();
    out.open = false;
    out.open_start = 0;
    if (!vcd || !vcd->trace_blackout)
        return;
    const auto* store = static_cast<const TraceBlackoutStore*>(vcd->trace_blackout);
    const size_t n = trace_blackout_export_spans(store, nullptr, nullptr, 0, nullptr, nullptr);
    out.spans.reserve(n);
    for (size_t i = 0; i < n; ++i) {
        uint64_t t0 = 0, t1 = 0;
        if (trace_blackout_span_at(store, i, &t0, &t1))
            out.spans.emplace_back(t0, t1);
    }
    int open = 0;
    uint64_t open_start = 0;
    trace_blackout_export_spans(store, nullptr, nullptr, 0, &open, &open_start);
    out.open = open != 0;
    out.open_start = open_start;
}

static bool write_blackout_section(FILE* f, const BlackoutSnapshot& bo, uint64_t* out_offset)
{
    if (!f || !out_offset)
        return false;
    *out_offset = static_cast<uint64_t>(ftell(f));
    fwrite(kBlackoutMagic, 1, 8, f);
    const uint32_t n = static_cast<uint32_t>(bo.spans.size());
    fwrite(&n, sizeof(n), 1, f);
    for (const auto& span : bo.spans) {
        fwrite(&span.first, sizeof(span.first), 1, f);
        fwrite(&span.second, sizeof(span.second), 1, f);
    }
    const uint8_t open_flag = bo.open ? 1u : 0u;
    fwrite(&open_flag, sizeof(open_flag), 1, f);
    fwrite(&bo.open_start, sizeof(bo.open_start), 1, f);
    return true;
}

static bool read_blackout_section(FILE* f, uint64_t offset, BlackoutSnapshot& out)
{
    if (!f)
        return false;
    if (fseek(f, static_cast<long>(offset), SEEK_SET) != 0)
        return false;
    char magic[8] = {};
    if (fread(magic, 1, 8, f) != 8 || memcmp(magic, kBlackoutMagic, 8) != 0)
        return false;
    uint32_t n = 0;
    if (fread(&n, sizeof(n), 1, f) != 1)
        return false;
    out.spans.clear();
    out.spans.reserve(n);
    for (uint32_t i = 0; i < n; ++i) {
        uint64_t t0 = 0, t1 = 0;
        if (fread(&t0, sizeof(t0), 1, f) != 1 || fread(&t1, sizeof(t1), 1, f) != 1)
            return false;
        out.spans.emplace_back(t0, t1);
    }
    uint8_t open_flag = 0;
    if (fread(&open_flag, sizeof(open_flag), 1, f) != 1)
        return false;
    out.open = open_flag != 0;
    if (fread(&out.open_start, sizeof(out.open_start), 1, f) != 1)
        return false;
    return true;
}

struct EncodeState {
    uint64_t current_timestamp = 0;
    bool timestamp_valid = false;
};

static void encode_line(
    const char* line,
    size_t line_len,
    EncodeState* st,
    std::vector<uint8_t>& block_buf,
    uint64_t& block_beg,
    uint64_t& block_end,
    vcd_t* vcd,
    const std::unordered_map<std::string, uint16_t>& id_to_idx,
    uint64_t& max_timestamp)
{
    if (!line || line_len == 0 || !st)
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
        max_timestamp = std::max(max_timestamp, st->current_timestamp);

        block_buf.push_back(REC_TIMESTAMP);
        append_varint(block_buf, st->current_timestamp);
        if (block_beg == UINT64_MAX)
            block_beg = st->current_timestamp;
        block_end = st->current_timestamp;
        return;
    }

    if (line_len >= 8 && line[0] == '$' && st->timestamp_valid && vcd) {
        if (line_len == 8 && memcmp(line, "$dumpoff", 8) == 0) {
            if (!vcd->trace_blackout)
                vcd->trace_blackout = trace_blackout_create();
            trace_blackout_on_dumpoff(static_cast<TraceBlackoutStore*>(vcd->trace_blackout),
                st->current_timestamp);
            block_buf.push_back(REC_DUMPOFF);
            return;
        }
        if (line_len == 7 && memcmp(line, "$dumpon", 7) == 0 && vcd->trace_blackout) {
            trace_blackout_on_dumpon(static_cast<TraceBlackoutStore*>(vcd->trace_blackout),
                st->current_timestamp);
            block_buf.push_back(REC_DUMPON);
            return;
        }
    }

    if (!st->timestamp_valid)
        return;
    if (!strchr("-0123456789zZxXbBrR", line[0]))
        return;

    std::string sig_id;
    const char* last_space = nullptr;
    for (size_t i = line_len; i > 0; --i) {
        if (line[i - 1] == ' ') {
            last_space = line + (i - 1);
            break;
        }
    }

    size_t val_len = 0;
    const char* val_ptr = nullptr;
    if (last_space) {
        val_ptr = line;
        val_len = static_cast<size_t>(last_space - line);
        sig_id.assign(last_space + 1, line_len - val_len - 1);
    } else {
        if (line_len < 2)
            return;
        val_ptr = line;
        val_len = 1;
        sig_id.assign(line + 1, line_len - 1);
    }

    const auto it = id_to_idx.find(sig_id);
    if (it == id_to_idx.end())
        return;

    const uint16_t idx = it->second;
    if (val_len <= 1) {
        block_buf.push_back(REC_SCALAR);
        block_buf.push_back(static_cast<uint8_t>(idx & 0xff));
        block_buf.push_back(static_cast<uint8_t>(idx >> 8));
        block_buf.push_back(static_cast<uint8_t>(val_len ? val_ptr[0] : '0'));
    } else {
        if (val_len > 65535)
            return;
        block_buf.push_back(REC_VECTOR);
        block_buf.push_back(static_cast<uint8_t>(idx & 0xff));
        block_buf.push_back(static_cast<uint8_t>(idx >> 8));
        const uint16_t len16 = static_cast<uint16_t>(val_len);
        block_buf.push_back(static_cast<uint8_t>(len16 & 0xff));
        block_buf.push_back(static_cast<uint8_t>(len16 >> 8));
        block_buf.insert(block_buf.end(), val_ptr, val_ptr + val_len);
    }
}

static bool flush_block(
    FILE* out,
    std::vector<uint8_t>& block_buf,
    uint64_t block_beg,
    uint64_t block_end,
    std::vector<TraceSidecar::VcBlock>& blocks)
{
    if (block_buf.empty())
        return true;

    std::vector<uint8_t> compressed;
    if (!zlib_compress_buf(block_buf.data(), block_buf.size(), compressed))
        return false;

    TraceSidecar::VcBlock b{};
    b.beg_tim = block_beg == UINT64_MAX ? 0 : block_beg;
    b.end_tim = block_end;
    b.blkpos = static_cast<uint64_t>(ftell(out));
    b.seclen = compressed.size();
    if (fwrite(compressed.data(), 1, compressed.size(), out) != compressed.size())
        return false;
    blocks.push_back(b);
    block_buf.clear();
    return true;
}

} // namespace

bool Enabled()
{
    return WaveformPerf::VcdRecodeEnabled() != 0
        && TraceSidecar::IdxCacheEnabled();
}

std::string PathForVcd(const char* utf8_path)
{
    if (!utf8_path || !utf8_path[0])
        return {};
    return std::string(utf8_path) + ".bwvc";
}

bool LoadMeta(const char* utf8_path, uint64_t data_section_offset, Meta* out)
{
    if (!utf8_path || !out)
        return false;

    const std::string path = PathForVcd(utf8_path);
    FILE* f = fopen(path.c_str(), "rb");
    if (!f)
        return false;

    char magic[8] = {};
    if (fread(magic, 1, 8, f) != 8 || memcmp(magic, kMagic, 8) != 0) {
        fclose(f);
        return false;
    }

    uint32_t ver = 0;
    TraceSidecar::FileIdentity id{};
    uint64_t stored_data_offset = 0;
    uint32_t signal_count = 0;
    uint32_t block_count = 0;
    uint64_t payload_offset = 0;
    uint64_t block_table_offset = 0;
    uint64_t blackout_offset = 0;

    if (fread(&ver, sizeof(ver), 1, f) != 1 || ver != kVersion) {
        fclose(f);
        return false;
    }
    if (fread(&id.size_bytes, sizeof(id.size_bytes), 1, f) != 1 ||
        fread(&id.mtime_unix, sizeof(id.mtime_unix), 1, f) != 1 ||
        fread(&stored_data_offset, sizeof(stored_data_offset), 1, f) != 1 ||
        fread(&out->max_timestamp, sizeof(out->max_timestamp), 1, f) != 1 ||
        fread(&signal_count, sizeof(signal_count), 1, f) != 1 ||
        fread(&block_count, sizeof(block_count), 1, f) != 1 ||
        fread(&payload_offset, sizeof(payload_offset), 1, f) != 1 ||
        fread(&block_table_offset, sizeof(block_table_offset), 1, f) != 1 ||
        fread(&blackout_offset, sizeof(blackout_offset), 1, f) != 1) {
        fclose(f);
        return false;
    }

    if (stored_data_offset != data_section_offset) {
        fclose(f);
        return false;
    }

    const TraceSidecar::FileIdentity cur = TraceSidecar::QueryFileIdentity(utf8_path);
    if (cur.size_bytes != id.size_bytes || cur.mtime_unix != id.mtime_unix) {
        fclose(f);
        return false;
    }

    if (fseek(f, static_cast<long>(kHeaderSize), SEEK_SET) != 0) {
        fclose(f);
        return false;
    }

    out->signal_ids.clear();
    out->signal_widths.clear();
    out->signal_ids.reserve(signal_count);
    out->signal_widths.reserve(signal_count);
    for (uint32_t i = 0; i < signal_count; ++i) {
        uint8_t id_len = 0;
        if (fread(&id_len, sizeof(id_len), 1, f) != 1 || id_len == 0) {
            fclose(f);
            return false;
        }
        std::string sid(id_len, '\0');
        if (fread(sid.data(), 1, id_len, f) != id_len) {
            fclose(f);
            return false;
        }
        uint16_t width = 0;
        if (fread(&width, sizeof(width), 1, f) != 1) {
            fclose(f);
            return false;
        }
        out->signal_ids.push_back(std::move(sid));
        out->signal_widths.push_back(width);
    }

    out->blocks.resize(block_count);
    if (block_table_offset != 0) {
        if (fseek(f, static_cast<long>(block_table_offset), SEEK_SET) != 0) {
            fclose(f);
            return false;
        }
        for (uint32_t i = 0; i < block_count; ++i) {
            TraceSidecar::VcBlock& b = out->blocks[i];
            if (fread(&b.beg_tim, sizeof(b.beg_tim), 1, f) != 1 ||
                fread(&b.end_tim, sizeof(b.end_tim), 1, f) != 1 ||
                fread(&b.blkpos, sizeof(b.blkpos), 1, f) != 1 ||
                fread(&b.seclen, sizeof(b.seclen), 1, f) != 1) {
                fclose(f);
                return false;
            }
        }
    }

    if (blackout_offset != 0)
        read_blackout_section(f, blackout_offset, out->blackout);

    fclose(f);
    out->cache_path = path;
    (void)payload_offset;
    return true;
}

void ApplyBlackout(vcd_t* vcd, const Meta& meta)
{
    if (!vcd)
        return;
    if (meta.blackout.spans.empty() && !meta.blackout.open)
        return;
    if (!vcd->trace_blackout)
        vcd->trace_blackout = trace_blackout_create();
    auto* store = static_cast<TraceBlackoutStore*>(vcd->trace_blackout);
    std::vector<uint64_t> t0;
    std::vector<uint64_t> t1;
    t0.reserve(meta.blackout.spans.size());
    t1.reserve(meta.blackout.spans.size());
    for (const auto& span : meta.blackout.spans) {
        t0.push_back(span.first);
        t1.push_back(span.second);
    }
    trace_blackout_import_spans(
        store,
        t0.data(),
        t1.data(),
        meta.blackout.spans.size(),
        meta.blackout.open ? 1 : 0,
        meta.blackout.open_start);
}

int BuildAndSave(
    const char* utf8_path,
    FILE* file,
    uint64_t file_size,
    uint64_t data_section_offset,
    vcd_t* vcd,
    const std::unordered_map<std::string, signal_t*>& id_to_sig,
    std::atomic<bool>* cancel,
    void (*progress_fn)(void*, uint32_t, uint32_t, uint32_t, uint64_t),
    void* progress_user)
{
    (void)id_to_sig;
    if (!utf8_path || !file || !vcd || data_section_offset == 0)
        return -1;

    std::vector<std::string> signal_ids;
    std::vector<uint16_t> signal_widths;
    std::unordered_map<std::string, uint16_t> id_to_idx;
    build_signal_table(vcd, signal_ids, signal_widths, id_to_idx);

    const std::string cache_path = PathForVcd(utf8_path);
    FILE* out = fopen(cache_path.c_str(), "wb");
    if (!out)
        return -1;

    const TraceSidecar::FileIdentity id = TraceSidecar::QueryFileIdentity(utf8_path);
    std::vector<uint8_t> header(kHeaderSize, 0);
    memcpy(header.data(), kMagic, 8);
    fwrite(header.data(), 1, header.size(), out);

    const uint32_t signal_count = static_cast<uint32_t>(signal_ids.size());
    for (uint32_t i = 0; i < signal_count; ++i) {
        const std::string& sid = signal_ids[i];
        const uint8_t id_len = static_cast<uint8_t>(std::min<size_t>(sid.size(), 255));
        fwrite(&id_len, sizeof(id_len), 1, out);
        fwrite(sid.data(), 1, id_len, out);
        const uint16_t w = signal_widths[i];
        fwrite(&w, sizeof(w), 1, out);
    }

    const uint64_t payload_offset = static_cast<uint64_t>(ftell(out));

    if (fseek(file, static_cast<long>(data_section_offset), SEEK_SET) != 0) {
        fclose(out);
        remove(cache_path.c_str());
        return -1;
    }

    std::vector<TraceSidecar::VcBlock> blocks;
    std::vector<uint8_t> block_buf;
    block_buf.reserve(65536);
    uint64_t block_beg = UINT64_MAX;
    uint64_t block_end = 0;
    uint64_t max_timestamp = 0;
    EncodeState st;

    std::vector<char> buf(kReadBufSize);
    std::string carry;
    const uint64_t scan_span = (file_size > data_section_offset) ? (file_size - data_section_offset) : 1;
    uint32_t progress_last_pct = UINT32_MAX;

    while (!cancel || !cancel->load(std::memory_order_relaxed)) {
        const size_t n = fread(buf.data(), 1, buf.size(), file);
        if (n == 0)
            break;

        if (progress_fn && scan_span > 0) {
            const long pos = ftell(file);
            if (pos >= 0) {
                const uint64_t done = static_cast<uint64_t>(pos) - data_section_offset;
                const uint32_t pct = static_cast<uint32_t>(std::min<uint64_t>(99, (done * 100) / scan_span));
                if (pct != progress_last_pct) {
                    progress_last_pct = pct;
                    progress_fn(progress_user, pct, 100, 0, 0);
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
            encode_line(carry.c_str(), carry.size(), &st, block_buf, block_beg, block_end, vcd, id_to_idx, max_timestamp);
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
            encode_line(line_start, static_cast<size_t>(line_end - line_start), &st, block_buf, block_beg, block_end,
                vcd, id_to_idx, max_timestamp);
            if (block_buf.size() >= kBlockUncompressedLimit) {
                if (!flush_block(out, block_buf, block_beg, block_end, blocks)) {
                    fclose(out);
                    remove(cache_path.c_str());
                    return -1;
                }
                block_beg = UINT64_MAX;
                block_end = 0;
            }
            line_start = nl + 1;
        }
    }

    if (cancel && cancel->load(std::memory_order_relaxed)) {
        fclose(out);
        remove(cache_path.c_str());
        return 1;
    }

    if (!flush_block(out, block_buf, block_beg, block_end, blocks)) {
        fclose(out);
        remove(cache_path.c_str());
        return -1;
    }

    const long after_payload = ftell(out);
    const uint64_t block_table_offset = static_cast<uint64_t>(after_payload);
    const uint32_t block_count = static_cast<uint32_t>(blocks.size());
    for (const TraceSidecar::VcBlock& b : blocks)
        fwrite(&b, sizeof(b), 1, out);

    BlackoutSnapshot bo;
    snapshot_blackout(vcd, bo);
    uint64_t blackout_offset = 0;
    write_blackout_section(out, bo, &blackout_offset);

    fseek(out, 0, SEEK_SET);
    fwrite(kMagic, 1, 8, out);
    const uint32_t ver = kVersion;
    fwrite(&ver, sizeof(ver), 1, out);
    fwrite(&id.size_bytes, sizeof(id.size_bytes), 1, out);
    fwrite(&id.mtime_unix, sizeof(id.mtime_unix), 1, out);
    fwrite(&data_section_offset, sizeof(data_section_offset), 1, out);
    fwrite(&max_timestamp, sizeof(max_timestamp), 1, out);
    fwrite(&signal_count, sizeof(signal_count), 1, out);
    fwrite(&block_count, sizeof(block_count), 1, out);
    fwrite(&payload_offset, sizeof(payload_offset), 1, out);
    fwrite(&block_table_offset, sizeof(block_table_offset), 1, out);
    fwrite(&blackout_offset, sizeof(blackout_offset), 1, out);

    fclose(out);

    B2W_LOG_INFO(
        "VCD recode cache written \"%s\" signals=%u blocks=%u max_ts=%llu",
        cache_path.c_str(),
        signal_count,
        block_count,
        static_cast<unsigned long long>(max_timestamp));

    if (progress_fn)
        progress_fn(progress_user, 100, 100, 0, 0);
    return 0;
}

static void decode_block_records(
    const std::vector<uint8_t>& raw,
    const Meta& meta,
    const std::unordered_map<std::string, signal_t*>& id_to_sig,
    const std::unordered_set<std::string>& wanted_ids,
    uint64_t t0,
    uint64_t t1)
{
    const uint8_t* p = raw.data();
    const uint8_t* end = p + raw.size();
    uint64_t cur_ts = 0;

    while (p < end) {
        const uint8_t tag = *p++;
        switch (tag) {
        case REC_TIMESTAMP: {
            if (!read_varint(p, end, cur_ts))
                return;
            break;
        }
        case REC_SCALAR: {
            if (p + 3 > end)
                return;
            const uint16_t idx = static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
            const char val = static_cast<char>(p[2]);
            p += 3;
            if (cur_ts < t0 || cur_ts > t1)
                break;
            if (idx >= meta.signal_ids.size())
                break;
            const std::string& sid = meta.signal_ids[idx];
            if (!wanted_ids.empty() && wanted_ids.find(sid) == wanted_ids.end())
                break;
            auto it = id_to_sig.find(sid);
            if (it == id_to_sig.end() || !it->second)
                break;
            char val_buf[2] = { val, '\0' };
            vcd_signal_append_change_lazy(it->second, static_cast<timestamp_t>(cur_ts), val_buf);
            break;
        }
        case REC_VECTOR: {
            if (p + 4 > end)
                return;
            const uint16_t idx = static_cast<uint16_t>(p[0] | (static_cast<uint16_t>(p[1]) << 8));
            const uint16_t len = static_cast<uint16_t>(p[2] | (static_cast<uint16_t>(p[3]) << 8));
            p += 4;
            if (p + len > end)
                return;
            if (cur_ts < t0 || cur_ts > t1) {
                p += len;
                break;
            }
            if (idx >= meta.signal_ids.size()) {
                p += len;
                break;
            }
            const std::string& sid = meta.signal_ids[idx];
            if (!wanted_ids.empty() && wanted_ids.find(sid) == wanted_ids.end()) {
                p += len;
                break;
            }
            auto it = id_to_sig.find(sid);
            if (it == id_to_sig.end() || !it->second) {
                p += len;
                break;
            }
            std::string val(reinterpret_cast<const char*>(p), len);
            p += len;
            vcd_signal_append_change_lazy(it->second, static_cast<timestamp_t>(cur_ts), val.c_str());
            break;
        }
        case REC_DUMPOFF:
        case REC_DUMPON:
            break;
        default:
            return;
        }
    }
}

int DecodeWindow(
    const Meta& meta,
    const std::unordered_map<std::string, signal_t*>& id_to_sig,
    const std::unordered_set<std::string>& wanted_ids,
    uint64_t t0,
    uint64_t t1,
    std::atomic<bool>* cancel)
{
    if (meta.cache_path.empty() || meta.blocks.empty())
        return -1;

    FILE* f = fopen(meta.cache_path.c_str(), "rb");
    if (!f)
        return -1;

    uint32_t first = 0;
    uint32_t last = 0;
    TraceSidecar::FindOverlappingBlocks(meta.blocks, t0, t1, &first, &last);
    if (first > last) {
        fclose(f);
        return 0;
    }

    std::vector<uint8_t> compressed;
    std::vector<uint8_t> raw;

    for (uint32_t bi = first; bi <= last; ++bi) {
        if (cancel && cancel->load(std::memory_order_relaxed)) {
            fclose(f);
            return 1;
        }
        const TraceSidecar::VcBlock& b = meta.blocks[bi];
        if (b.seclen == 0)
            continue;
        compressed.resize(static_cast<size_t>(b.seclen));
        if (fseek(f, static_cast<long>(b.blkpos), SEEK_SET) != 0) {
            fclose(f);
            return -1;
        }
        if (fread(compressed.data(), 1, static_cast<size_t>(b.seclen), f) != b.seclen) {
            fclose(f);
            return -1;
        }
        if (!zlib_decompress_buf(compressed.data(), compressed.size(), raw)) {
            fclose(f);
            return -1;
        }
        decode_block_records(raw, meta, id_to_sig, wanted_ids, t0, t1);
    }

    fclose(f);
    return 0;
}

} // namespace VcdRecodeCache
