#include "lxt1_loader.h"

#include "core/trace_vc.h"
#include "lxt_format.h"

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <queue>
#include <string>
#include <vector>

#ifdef BEAR2WAVE_WITH_LXT2
#include "lxt_write.h"
#include <zlib.h>
#endif

namespace {

static void append_signal_node(vcd_t* vcd, signal_node_t* new_node)
{
    if (!vcd->signals_head)
        vcd->signals_head = new_node;
    else {
        signal_node_t* p = vcd->signals_head;
        while (p->next)
            p = p->next;
        p->next = new_node;
    }
    vcd->signals_count++;
}

static void split_lxt_name(const char* full, char* mod_path, char* sig_name)
{
    if (!full || !full[0]) {
        strncpy(mod_path, "$root", VCD_SIGNAL_SIZE - 1);
        mod_path[VCD_SIGNAL_SIZE - 1] = '\0';
        strncpy(sig_name, "sig", VCD_NAME_SIZE - 1);
        sig_name[VCD_NAME_SIZE - 1] = '\0';
        return;
    }
    const char* dot = strrchr(full, '.');
    if (!dot || dot == full) {
        strncpy(mod_path, "$root", VCD_SIGNAL_SIZE - 1);
        mod_path[VCD_SIGNAL_SIZE - 1] = '\0';
        strncpy(sig_name, full, VCD_NAME_SIZE - 1);
        sig_name[VCD_NAME_SIZE - 1] = '\0';
        return;
    }
    const size_t mod_len = static_cast<size_t>(dot - full);
    if (mod_len >= VCD_SIGNAL_SIZE)
        strncpy(mod_path, full, VCD_SIGNAL_SIZE - 1);
    else {
        memcpy(mod_path, full, mod_len);
        mod_path[mod_len] = '\0';
    }
    strncpy(sig_name, dot + 1, VCD_NAME_SIZE - 1);
    sig_name[VCD_NAME_SIZE - 1] = '\0';
}

#ifdef BEAR2WAVE_WITH_LXT2

struct Lxt1Sections {
    uint32_t facname_off = 0;
    uint32_t facgeom_off = 0;
    uint32_t timescale_off = 0;
    uint32_t change_off = 0;
    uint32_t time_table_off = 0;
    uint32_t sync_off = 0;
    uint32_t zfacname_size = 0;
    uint32_t zfacgeom_size = 0;
    uint32_t zchg_size = 0;
    uint32_t zchg_predec_size = 0;
    uint32_t ztime_table_size = 0;
    uint32_t zsync_size = 0;
};

struct Lxt1Fac {
    std::string name;
    uint32_t rows = 1;
    int32_t msb = 0;
    int32_t lsb = 0;
    uint32_t flags = 0;
    uint32_t len = 1;
    int alias_root = -1;
};

static uint8_t rd_u8(const std::vector<uint8_t>& b, size_t o)
{
    return o < b.size() ? b[o] : 0;
}

static uint16_t rd_u16(const std::vector<uint8_t>& b, size_t o)
{
    if (o + 2 > b.size())
        return 0;
    return static_cast<uint16_t>((static_cast<uint16_t>(b[o]) << 8) | b[o + 1]);
}

static uint32_t rd_u24(const std::vector<uint8_t>& b, size_t o)
{
    if (o + 3 > b.size())
        return 0;
    return (static_cast<uint32_t>(b[o]) << 16) | (static_cast<uint32_t>(b[o + 1]) << 8) | b[o + 2];
}

static uint32_t rd_u32(const std::vector<uint8_t>& b, size_t o)
{
    if (o + 4 > b.size())
        return 0;
    return (static_cast<uint32_t>(b[o]) << 24) | (static_cast<uint32_t>(b[o + 1]) << 16)
        | (static_cast<uint32_t>(b[o + 2]) << 8) | b[o + 3];
}

static bool zlib_inflate(const uint8_t* src, size_t src_len, std::vector<uint8_t>& out, size_t expect)
{
    out.clear();
    // Always provide a non-null buffer for zlib; estimate if not given.
    size_t init_sz = expect;
    if (init_sz == 0) {
        init_sz = src_len * 4u;
        if (init_sz < 4096u) init_sz = 4096u;
    }
    out.resize(init_sz);
    z_stream zs {};
    zs.next_in = const_cast<Bytef*>(reinterpret_cast<const Bytef*>(src));
    zs.avail_in = static_cast<uInt>(src_len);
    zs.next_out = reinterpret_cast<Bytef*>(out.data());
    zs.avail_out = static_cast<uInt>(out.size());
    if (inflateInit2(&zs, 15 + 32) != Z_OK)
        return false;
    int rc = inflate(&zs, Z_FINISH);
    // If output buffer was too small, try again with a larger buffer.
    while (rc == Z_BUF_ERROR) {
        out.resize(out.size() * 2u);
        zs.next_out = reinterpret_cast<Bytef*>(out.data()) + zs.total_out;
        zs.avail_out = static_cast<uInt>(out.size() - zs.total_out);
        rc = inflate(&zs, Z_FINISH);
    }
    inflateEnd(&zs);
    if (rc != Z_STREAM_END)
        return false;
    out.resize(zs.total_out);
    return true;
}

static bool load_change_section(
    const std::vector<uint8_t>& file,
    const Lxt1Sections& sec,
    std::vector<uint8_t>& chg)
{
    if (sec.change_off == 0 || sec.zchg_size == 0 || sec.change_off + sec.zchg_size > file.size())
        return false;

    const uint8_t* zsrc = file.data() + sec.change_off;
    const size_t zlen = sec.zchg_size;
    if (zlen >= 3 && zsrc[0] == 'B' && zsrc[1] == 'Z' && zsrc[2] == 'h') {
        fprintf(stderr, "[LXT1] bzip2 linear LXT requires bzlib (use interlaced LXT or LXT2)\n");
        return false;
    }
    return zlib_inflate(zsrc, zlen, chg, sec.zchg_predec_size);
}

static bool parse_section_index(const std::vector<uint8_t>& file, Lxt1Sections& sec)
{
    if (file.size() < 6)
        return false;
    if (rd_u8(file, file.size() - 1) != BEAR2WAVE_LXT1_TRLID)
        return false;
    if (rd_u16(file, 0) != BEAR2WAVE_LXT1_HDRID)
        return false;

    size_t tagpnt = file.size() - 2;
    while (tagpnt >= 5) {
        const uint8_t tag = rd_u8(file, tagpnt);
        if (tag == LT_SECTION_END)
            break;
        const uint32_t offset = rd_u32(file, tagpnt - 4);
        tagpnt -= 5;
        switch (tag) {
        case LT_SECTION_FACNAME: sec.facname_off = offset; break;
        case LT_SECTION_FACNAME_GEOMETRY: sec.facgeom_off = offset; break;
        case LT_SECTION_TIMESCALE: sec.timescale_off = offset; break;
        case LT_SECTION_CHG: sec.change_off = offset; break;
        case LT_SECTION_TIME_TABLE: sec.time_table_off = offset; break;
        case LT_SECTION_TIME_TABLE64: sec.time_table_off = offset; break;
        case LT_SECTION_SYNC_TABLE: sec.sync_off = offset; break;
        case LT_SECTION_ZFACNAME_SIZE: sec.zfacname_size = offset; break;
        case LT_SECTION_ZFACNAME_GEOMETRY_SIZE: sec.zfacgeom_size = offset; break;
        case LT_SECTION_ZCHG_SIZE: sec.zchg_size = offset; break;
        case LT_SECTION_ZCHG_PREDEC_SIZE: sec.zchg_predec_size = offset; break;
        case LT_SECTION_ZTIME_TABLE_SIZE: sec.ztime_table_size = offset; break;
        case LT_SECTION_ZSYNC_SIZE: sec.zsync_size = offset; break;
        default: break;
        }
    }
    return sec.facname_off != 0 && sec.zfacname_size != 0;
}

static void apply_timescale_byte(vcd_t* vcd, uint8_t ts)
{
    vcd->timescale.scale = 1;
    const char* u = "ns";
    if (ts >= 15)
        u = "fs";
    else if (ts >= 12)
        u = "ps";
    else if (ts >= 9)
        u = "ns";
    else if (ts >= 6)
        u = "us";
    else if (ts >= 3)
        u = "ms";
    else if (ts >= 0)
        u = "s";
    strncpy(vcd->timescale.unit, u, VCD_TIME_UNIT_SIZE - 1);
    vcd->timescale.unit[VCD_TIME_UNIT_SIZE - 1] = '\0';
}

static char mvl_from_cmd_nibble(uint8_t cmd)
{
    static const char map[] = {'0', '1', 'x', 'z', 'h', 'u', 'w', 'l', '-', '-', '-', '-', '-', '-', '-', '-'};
    return map[cmd & 0xf];
}

static char lxt1_flash_char(uint8_t cmd)
{
    switch (cmd & 0x0fu) {
    case 0x3:
        return '0';
    case 0x4:
        return '1';
    case 0x5:
        return 'z';
    case 0x6:
        return 'x';
    case 0x7:
        return 'h';
    case 0x8:
        return 'u';
    case 0x9:
        return 'w';
    case 0xa:
        return 'l';
    case 0xb:
        return '-';
    default:
        return mvl_from_cmd_nibble(cmd);
    }
}

static void lxt1_flash_to_string(const Lxt1Fac& fac, uint8_t cmd, char* out, size_t out_len)
{
    if (!out || out_len == 0)
        return;
    const char ch = lxt1_flash_char(cmd);
    unsigned width = (fac.flags & LT_SYM_F_INTEGER) ? 32u : fac.len;
    if (width == 0)
        width = 1;
    if (width == 1) {
        out[0] = ch;
        out[1] = '\0';
        return;
    }
    std::string s(width, ch);
    strncpy(out, s.c_str(), out_len - 1);
    out[out_len - 1] = '\0';
}

static void bits_to_string(uint8_t byte_val, unsigned width, char* out, size_t out_len)
{
    if (!out || out_len == 0)
        return;
    if (width == 0)
        width = 1;
    out[0] = '\0';
    std::string s;
    s.reserve(width);
    for (int i = static_cast<int>(width) - 1; i >= 0; --i) {
        const int bit = (byte_val >> i) & 1;
        s.push_back(bit ? '1' : '0');
    }
    strncpy(out, s.c_str(), out_len - 1);
    out[out_len - 1] = '\0';
}

static bool load_facnames(
    const std::vector<uint8_t>& file,
    const Lxt1Sections& sec,
    std::vector<Lxt1Fac>& facs)
{
    const uint32_t numfacs = rd_u32(file, sec.facname_off);
    const uint32_t total_mem = rd_u32(file, sec.facname_off + 4);
    if (numfacs == 0 || total_mem == 0)
        return false;

    const uint8_t* zsrc = file.data() + sec.facname_off + 8;
    const size_t zlen = sec.zfacname_size;
    if (sec.facname_off + 8 + zlen > file.size())
        return false;

    std::vector<uint8_t> raw;
    if (!zlib_inflate(zsrc, zlen, raw, total_mem))
        return false;

    facs.resize(numfacs);
    size_t pos = 0;
    for (uint32_t i = 0; i < numfacs; ++i) {
        if (pos + 2 > raw.size())
            return false;
        const uint16_t prefix_len = rd_u16(raw, pos);
        pos += 2;
        const size_t start = pos;
        while (pos < raw.size() && raw[pos] != 0)
            pos++;
        if (pos >= raw.size())
            return false;
        std::string tail(reinterpret_cast<const char*>(raw.data() + start), pos - start);
        pos++; /* skip NUL */
        if (i == 0) {
            facs[i].name = tail;
        } else {
            const std::string& prev = facs[i - 1].name;
            if (prefix_len <= prev.size())
                facs[i].name = prev.substr(0, prefix_len) + tail;
            else
                facs[i].name = tail;
        }
    }
    return true;
}

static bool load_geometry(
    const std::vector<uint8_t>& file,
    const Lxt1Sections& sec,
    std::vector<Lxt1Fac>& facs)
{
    if (!sec.facgeom_off || !sec.zfacgeom_size)
        return false;

    const uint8_t* zsrc = file.data() + sec.facgeom_off;
    const size_t zlen = sec.zfacgeom_size;
    if (sec.facgeom_off + zlen > file.size())
        return false;

    std::vector<uint8_t> raw;
    if (!zlib_inflate(zsrc, zlen, raw, 0))
        return false;

    size_t pos = 0;
    for (size_t i = 0; i < facs.size(); ++i) {
        if (pos + 16 > raw.size())
            return false;
        const uint32_t a = rd_u32(raw, pos);
        pos += 4;
        const uint32_t b = rd_u32(raw, pos);
        pos += 4;
        const uint32_t c = rd_u32(raw, pos);
        pos += 4;
        const uint32_t flags = rd_u32(raw, pos);
        pos += 4;
        if (flags & LT_SYM_F_ALIAS) {
            facs[i].alias_root = static_cast<int>(a);
            facs[i].msb = static_cast<int32_t>(b);
            facs[i].lsb = static_cast<int32_t>(c);
            facs[i].flags = flags;
            // len will be resolved in second pass below
            facs[i].len = 0;
        } else {
            facs[i].rows = a;
            facs[i].msb = static_cast<int32_t>(b);
            facs[i].lsb = static_cast<int32_t>(c);
            facs[i].flags = flags;
            facs[i].len = (flags & LT_SYM_F_INTEGER) ? 32u : (b >= static_cast<int32_t>(c) ? static_cast<uint32_t>(b - c + 1) : 1u);
            if (facs[i].len == 0)
                facs[i].len = 1;
        }
    }
    // Second pass: resolve alias lengths (root may appear after alias in file)
    for (size_t i = 0; i < facs.size(); ++i) {
        if (facs[i].flags & LT_SYM_F_ALIAS) {
            const int root = facs[i].alias_root;
            if (root >= 0 && static_cast<size_t>(root) < facs.size() && facs[static_cast<size_t>(root)].len > 0)
                facs[i].len = facs[static_cast<size_t>(root)].len;
            else
                facs[i].len = 1; // fallback: prevent zero-length causing parse stall
        }
    }
    return true;
}

static bool load_time_markers(
    const std::vector<uint8_t>& file,
    const Lxt1Sections& sec,
    std::vector<std::pair<size_t, uint64_t>>& markers)
{
    if (!sec.time_table_off)
        return false;

    const uint32_t cycles = rd_u32(file, sec.time_table_off);
    if (cycles == 0)
        return false;

    std::vector<uint8_t> raw;
    if (sec.ztime_table_size != 0) {
        const uint8_t* zsrc = file.data() + sec.time_table_off + 4;
        if (sec.time_table_off + 4 + sec.ztime_table_size > file.size())
            return false;
        if (!zlib_inflate(zsrc, sec.ztime_table_size, raw, 0))
            return false;
    } else {
        const size_t need = sec.time_table_off + 4 + 8 + cycles * 8u;
        if (need > file.size())
            return false;
        raw.assign(file.begin() + sec.time_table_off + 4, file.begin() + need);
    }

    if (raw.size() < 8 + cycles * 8u)
        return false;

    const uint64_t min_time = rd_u32(raw, 0);
    (void)rd_u32(raw, 4); /* max_time */

    markers.clear();
    markers.reserve(cycles);
    size_t pos = 0;
    uint64_t cur_time = min_time;
    const size_t pos_base = 8;
    const size_t time_base = pos_base + cycles * 4u;
    for (uint32_t i = 0; i < cycles; ++i) {
        pos += rd_u32(raw, pos_base + i * 4u);
        cur_time += rd_u32(raw, time_base + i * 4u);
        markers.emplace_back(pos, cur_time);
    }
    return !markers.empty();
}

static uint64_t lxt1_time_at_offset(
    size_t offset,
    const std::vector<std::pair<size_t, uint64_t>>& time_markers)
{
    uint64_t cur = 0;
    for (const auto& m : time_markers) {
        if (m.first > offset)
            break;
        cur = m.second;
    }
    return cur;
}

static bool lxt1_skip_change_payload(
    const std::vector<uint8_t>& buf,
    size_t& offs,
    const Lxt1Fac& fac,
    uint8_t cmd,
    bool cmdkill)
{
    if (cmdkill) {
        if (fac.flags & LT_SYM_F_DOUBLE) {
            offs += 8;
        } else {
            while (offs < buf.size() && rd_u8(buf, offs) != 0)
                offs++;
            if (offs < buf.size())
                offs++;
        }
        return offs <= buf.size();
    }

    switch (cmd) {
    case 0x0: {
        const unsigned modlen = (fac.flags & LT_SYM_F_INTEGER) ? 32u : fac.len;
        offs += (modlen + 7u) / 8u;
        break;
    }
    case 0x1:
        offs += (fac.len + 3u) / 4u;
        break;
    case 0x2:
        offs += (fac.len + 1u) / 2u;
        break;
    case 0x3:
    case 0x4:
    case 0x5:
    case 0x6:
    case 0x7:
    case 0x8:
    case 0x9:
    case 0xa:
    case 0xb:
        break;
    case 0xc:
    case 0xd:
    case 0xe:
    case 0xf:
        offs += (cmd & 3u) + 1u;
        break;
    default:
        return false;
    }
    return offs <= buf.size();
}

static bool lxt1_append_decoded_change(
    signal_t* sig,
    const Lxt1Fac& fac,
    uint8_t cmd,
    bool cmdkill,
    const std::vector<uint8_t>& buf,
    size_t val_start,
    uint64_t cur_time)
{
    if (!sig)
        return true;

    char valbuf[VCD_SIGNAL_SIZE];
    if (!cmdkill && cmd >= 0x8 && cmd <= 0xb) {
        valbuf[0] = mvl_from_cmd_nibble(cmd);
        valbuf[1] = '\0';
        vcd_signal_append_change(sig, static_cast<timestamp_t>(cur_time), valbuf);
    } else if (!cmdkill && cmd >= 0x3 && cmd <= 0x7) {
        lxt1_flash_to_string(fac, cmd, valbuf, sizeof(valbuf));
        vcd_signal_append_change(sig, static_cast<timestamp_t>(cur_time), valbuf);
    } else if (!cmdkill && cmd == 0x0 && fac.len <= 8) {
        const uint8_t byte_val = rd_u8(buf, val_start);
        bits_to_string(byte_val, fac.len, valbuf, sizeof(valbuf));
        vcd_signal_append_change(sig, static_cast<timestamp_t>(cur_time), valbuf);
    } else if (!cmdkill && cmd == 0x0) {
        std::string bits;
        const unsigned nbytes = (fac.len + 7u) / 8u;
        for (unsigned i = 0; i < nbytes && val_start + i < buf.size(); ++i) {
            const uint8_t byte_val = rd_u8(buf, val_start + i);
            for (int bit = 7; bit >= 0; --bit) {
                if (bits.size() >= fac.len)
                    break;
                bits.push_back((byte_val & (1 << bit)) ? '1' : '0');
            }
        }
        while (bits.size() < fac.len)
            bits.push_back('0');
        strncpy(valbuf, bits.c_str(), sizeof(valbuf) - 1);
        valbuf[sizeof(valbuf) - 1] = '\0';
        vcd_signal_append_change(sig, static_cast<timestamp_t>(cur_time), valbuf);
    }
    return true;
}

struct Lxt1HeapEntry {
    size_t offset = 0;
    uint32_t facidx = 0;
};

struct Lxt1HeapCompare {
    bool operator()(const Lxt1HeapEntry& a, const Lxt1HeapEntry& b) const
    {
        return a.offset < b.offset;
    }
};

static bool load_sync_offsets(
    const std::vector<uint8_t>& file,
    const Lxt1Sections& sec,
    size_t numfacs,
    std::vector<uint32_t>& sync)
{
    sync.assign(numfacs, 0);
    if (!sec.sync_off)
        return false;

    std::vector<uint8_t> raw;
    if (sec.zsync_size != 0) {
        if (sec.sync_off + sec.zsync_size > file.size())
            return false;
        if (!zlib_inflate(file.data() + sec.sync_off, sec.zsync_size, raw, numfacs * 4u))
            return false;
    } else {
        const size_t need = sec.sync_off + numfacs * 4u;
        if (need > file.size())
            return false;
        raw.assign(file.begin() + sec.sync_off, file.begin() + need);
    }

    if (raw.size() < numfacs * 4u)
        return false;
    for (size_t i = 0; i < numfacs; ++i)
        sync[i] = rd_u32(raw, i * 4u);
    return true;
}

static bool apply_interlaced_changes(
    const std::vector<uint8_t>& file,
    const std::vector<Lxt1Fac>& facs,
    std::vector<signal_t*>& sig_by_fac,
    const std::vector<uint32_t>& sync,
    const std::vector<std::pair<size_t, uint64_t>>& time_markers)
{
    std::priority_queue<Lxt1HeapEntry, std::vector<Lxt1HeapEntry>, Lxt1HeapCompare> heap;
    size_t steps = 0;
    const size_t max_steps = facs.size() * 1024u + 64u;
    for (size_t i = 0; i < facs.size(); ++i) {
        if (i >= sync.size() || sync[i] == 0)
            continue;
        heap.push(Lxt1HeapEntry{sync[i], static_cast<uint32_t>(i)});
    }

    while (!heap.empty()) {
        if (++steps > max_steps)
            return false;
        Lxt1HeapEntry entry = heap.top();
        heap.pop();
        if (entry.offset == 0 || entry.facidx >= facs.size())
            continue;

        const Lxt1Fac& fac = facs[entry.facidx];
        if (entry.offset >= file.size())
            return false;

        const uint8_t cmd_byte = rd_u8(file, entry.offset);
        const unsigned numbytes = (cmd_byte >> 4) & 3u;
        const unsigned delta_len = numbytes + 1u;
        if (entry.offset + 1 + delta_len > file.size())
            return false;

        uint32_t delta = 0;
        switch (numbytes) {
        case 0:
            delta = rd_u8(file, entry.offset + 1);
            break;
        case 1:
            delta = rd_u16(file, entry.offset + 1);
            break;
        case 2:
            delta = rd_u24(file, entry.offset + 1);
            break;
        case 3:
            delta = rd_u32(file, entry.offset + 1);
            break;
        }

        size_t prev_offset = 0;
        if (entry.offset >= delta + 2u)
            prev_offset = entry.offset - delta - 2u;

        const bool cmdkill = (fac.flags & (LT_SYM_F_DOUBLE | LT_SYM_F_STRING)) != 0;
        const uint8_t cmd = cmdkill ? 0 : static_cast<uint8_t>(cmd_byte & 0x0fu);
        size_t offs = entry.offset + 1 + delta_len;

        if (fac.rows > 1) {
            if (fac.rows >= 256u * 65536u)
                offs += 4;
            else if (fac.rows >= 65536u)
                offs += 3;
            else if (fac.rows >= 256u)
                offs += 2;
            else
                offs += 1;
        }

        const size_t val_start = offs;
        if (!lxt1_skip_change_payload(file, offs, fac, cmd, cmdkill))
            return false;

        const uint64_t cur_time = lxt1_time_at_offset(entry.offset, time_markers);
        signal_t* sig = sig_by_fac[entry.facidx];
        if (!sig)
            continue;
        if (!lxt1_append_decoded_change(sig, fac, cmd, cmdkill, file, val_start, cur_time)) {
            return false;
        }

        if (prev_offset != 0 && prev_offset < entry.offset)
            heap.push(Lxt1HeapEntry{prev_offset, entry.facidx});
    }
    return true;
}

static bool apply_linear_changes(
    const std::vector<uint8_t>& chg,
    const std::vector<Lxt1Fac>& facs,
    std::vector<signal_t*>& sig_by_fac,
    const std::vector<std::pair<size_t, uint64_t>>& time_markers)
{
    if (chg.size() < 4)
        return false;

    const size_t vlen = chg.size();
    size_t offs = 4;
    uint64_t cur_time = 0;
    size_t marker_idx = 0;

    unsigned numfacs_bytes = 0;
    if (facs.size() >= 256u * 65536u)
        numfacs_bytes = 3;
    else if (facs.size() >= 65536u)
        numfacs_bytes = 2;
    else if (facs.size() >= 256u)
        numfacs_bytes = 1;

    char valbuf[VCD_SIGNAL_SIZE];

    while (offs < vlen) {
        while (marker_idx < time_markers.size() && offs >= time_markers[marker_idx].first) {
            cur_time = time_markers[marker_idx].second;
            marker_idx++;
        }

        uint32_t facidx = 0;
        switch (numfacs_bytes & 3u) {
        case 0: facidx = rd_u8(chg, offs); break;
        case 1: facidx = rd_u16(chg, offs); break;
        case 2: facidx = rd_u24(chg, offs); break;
        case 3: facidx = rd_u32(chg, offs); break;
        }
        offs += numfacs_bytes + 1;
        if (offs > vlen)
            return false;
        if (facidx >= facs.size())
            return false;

        const Lxt1Fac& fac = facs[facidx];
        const bool cmdkill = (fac.flags & (LT_SYM_F_DOUBLE | LT_SYM_F_STRING)) != 0;
        uint8_t cmd = 0;
        if (!cmdkill) {
            if (offs >= vlen)
                return false;
            cmd = rd_u8(chg, offs);
            if (cmd > 0x0f)
                return false;
            offs++;
        }

        const size_t val_start = offs;
        if (!cmdkill) {
            switch (cmd) {
            case 0x0: {
                const unsigned modlen = (fac.flags & LT_SYM_F_INTEGER) ? 32u : fac.len;
                offs += (modlen + 7u) / 8u;
                break;
            }
            case 0x1:
                offs += (fac.len + 3u) / 4u;
                break;
            case 0x2:
                offs += (fac.len + 1u) / 2u;
                break;
            case 0x3:
            case 0x4:
            case 0x5:
            case 0x6:
            case 0x7:
            case 0x8:
            case 0x9:
            case 0xa:
            case 0xb:
                break;
            case 0xc:
            case 0xd:
            case 0xe:
            case 0xf:
                offs += (cmd & 3u) + 1u;
                break;
            default:
                return false;
            }
        } else if (fac.flags & LT_SYM_F_DOUBLE) {
            offs += 8;
        } else {
            while (offs < vlen && rd_u8(chg, offs) != 0)
                offs++;
            if (offs < vlen)
                offs++;
        }

        if (offs > vlen)
            return false;

        signal_t* sig = sig_by_fac[facidx];
        if (!sig)
            continue;

        if (!cmdkill && cmd >= 0x3 && cmd <= 0xb) {
            lxt1_flash_to_string(fac, cmd, valbuf, sizeof(valbuf));
            vcd_signal_append_change(sig, static_cast<timestamp_t>(cur_time), valbuf);
        } else if (!cmdkill && cmd == 0x0 && fac.len <= 8) {
            const uint8_t byte_val = rd_u8(chg, val_start);
            bits_to_string(byte_val, fac.len, valbuf, sizeof(valbuf));
            vcd_signal_append_change(sig, static_cast<timestamp_t>(cur_time), valbuf);
        } else if (!cmdkill && cmd == 0x0) {
            std::string bits;
            const unsigned nbytes = (fac.len + 7u) / 8u;
            for (unsigned i = 0; i < nbytes && val_start + i < chg.size(); ++i) {
                uint8_t byte_val = rd_u8(chg, val_start + i);
                for (int bit = 7; bit >= 0; --bit) {
                    if (bits.size() >= fac.len)
                        break;
                    bits.push_back((byte_val & (1 << bit)) ? '1' : '0');
                }
            }
            while (bits.size() < fac.len)
                bits.push_back('0');
            strncpy(valbuf, bits.c_str(), sizeof(valbuf) - 1);
            valbuf[sizeof(valbuf) - 1] = '\0';
            vcd_signal_append_change(sig, static_cast<timestamp_t>(cur_time), valbuf);
        }

    }
    return true;
}

#endif /* BEAR2WAVE_WITH_LXT2 */

} // namespace

extern "C" vcd_t* lxt1_read_to_vcd(const char* utf8_path)
{
    if (!utf8_path || !utf8_path[0])
        return nullptr;

#ifndef BEAR2WAVE_WITH_LXT2
    fprintf(stderr,
        "[LXT1] Bear2Wave built without BEAR2WAVE_WITH_LXT2. See docs/TRACE_FORMATS_BUILD.md\n");
    return nullptr;
#else
    if (trace_lxt_probe_file(utf8_path) != BEAR2WAVE_LXT_VARIANT_LXT1) {
        fprintf(stderr, "[LXT1] Not a legacy LXT (v1) file: %s\n", utf8_path);
        return nullptr;
    }
    fprintf(stderr, "[LXT1] open: %s\n", utf8_path);

    FILE* f = fopen(utf8_path, "rb");
    if (!f) {
        fprintf(stderr, "[LXT1] Cannot open: %s\n", utf8_path);
        return nullptr;
    }
    fseek(f, 0, SEEK_END);
    const long flen = ftell(f);
    fseek(f, 0, SEEK_SET);
    if (flen <= 0) {
        fclose(f);
        return nullptr;
    }
    std::vector<uint8_t> file(static_cast<size_t>(flen));
    if (fread(file.data(), 1, file.size(), f) != file.size()) {
        fclose(f);
        return nullptr;
    }
    fclose(f);

    Lxt1Sections sec {};
    if (!parse_section_index(file, sec)) {
        fprintf(stderr, "[LXT1] Invalid section index: %s\n", utf8_path);
        return nullptr;
    }

    std::vector<Lxt1Fac> facs;
    if (!load_facnames(file, sec, facs) || !load_geometry(file, sec, facs)) {
        fprintf(stderr, "[LXT1] Failed to read hierarchy: %s\n", utf8_path);
        return nullptr;
    }

    vcd_t* vcd = vcd_alloc_empty();
    if (!vcd)
        return nullptr;
    strncpy(vcd->version, "Bear2Wave LXT1 loader", VCD_VERSION_SIZE - 1);
    vcd->trace_backend = VCD_TRACE_BACKEND_NONE;

    if (sec.timescale_off != 0 && sec.timescale_off < file.size())
        apply_timescale_byte(vcd, rd_u8(file, sec.timescale_off));

    std::vector<signal_t*> sig_by_fac(facs.size(), nullptr);
    for (size_t i = 0; i < facs.size(); ++i) {
        if (facs[i].flags & LT_SYM_F_ALIAS)
            continue;

        auto* node = static_cast<signal_node_t*>(calloc(1, sizeof(signal_node_t)));
        if (!node) {
            vcd_free(vcd);
            return nullptr;
        }
        signal_t* sig = &node->signal;
        char mod[VCD_SIGNAL_SIZE];
        char nm[VCD_NAME_SIZE];
        split_lxt_name(facs[i].name.c_str(), mod, nm);
        strncpy(sig->module_path, mod, VCD_SIGNAL_SIZE - 1);
        strncpy(sig->name, nm, VCD_NAME_SIZE - 1);
        snprintf(sig->full_name, VCD_SIGNAL_SIZE, "%s.%s", mod, nm);
        snprintf(sig->signal_id, VCD_NAME_SIZE, "lxt1%zu", i);
        sig->size = facs[i].len;
        sig->fst_var_type = -1;
        append_signal_node(vcd, node);
        sig_by_fac[i] = sig;
    }

    for (size_t i = 0; i < facs.size(); ++i) {
        if (!(facs[i].flags & LT_SYM_F_ALIAS))
            continue;
        const int root = facs[i].alias_root;
        if (root < 0 || static_cast<size_t>(root) >= sig_by_fac.size() || !sig_by_fac[static_cast<size_t>(root)])
            continue;

        auto* node = static_cast<signal_node_t*>(calloc(1, sizeof(signal_node_t)));
        if (!node) {
            vcd_free(vcd);
            return nullptr;
        }
        signal_t* alias_sig = &node->signal;
        signal_t* root_sig = sig_by_fac[static_cast<size_t>(root)];

        char mod[VCD_SIGNAL_SIZE];
        char nm[VCD_NAME_SIZE];
        split_lxt_name(facs[i].name.c_str(), mod, nm);
        strncpy(alias_sig->module_path, mod, VCD_SIGNAL_SIZE - 1);
        strncpy(alias_sig->name, nm, VCD_NAME_SIZE - 1);
        snprintf(alias_sig->full_name, VCD_SIGNAL_SIZE, "%s.%s", mod, nm);
        snprintf(alias_sig->signal_id, VCD_NAME_SIZE, "lxt1%zu", i);
        alias_sig->size = root_sig->size;
        alias_sig->fst_var_type = root_sig->fst_var_type;
        alias_sig->trace_alias_source = root_sig;

        append_signal_node(vcd, node);
        sig_by_fac[i] = alias_sig;
    }

    if (sec.sync_off != 0) {
        if (sec.zchg_size != 0) {
            vcd_free(vcd);
            fprintf(stderr,
                "[LXT1] Interlaced LXT with compressed change block not yet supported: %s\n",
                utf8_path);
            return nullptr;
        }
        std::vector<uint32_t> sync;
        std::vector<std::pair<size_t, uint64_t>> time_markers;
        if (!load_sync_offsets(file, sec, facs.size(), sync)
            || !load_time_markers(file, sec, time_markers)) {
            vcd_free(vcd);
            fprintf(stderr, "[LXT1] Failed to read interlaced tables: %s\n", utf8_path);
            return nullptr;
        }

        bool any_sync = false;
        for (uint32_t off : sync) {
            if (off != 0) {
                any_sync = true;
                break;
            }
        }
        if (!any_sync) {
            vcd_free(vcd);
            fprintf(stderr,
                "[LXT1] Interlaced LXT has empty sync table (no value changes). "
                "Regenerate with vcd2lxt on Linux/WSL or fix LXT v1 writer.\n");
            return nullptr;
        }

        if (!apply_interlaced_changes(file, facs, sig_by_fac, sync, time_markers)) {
            vcd_free(vcd);
            fprintf(stderr, "[LXT1] Failed to decode interlaced changes: %s\n", utf8_path);
            return nullptr;
        }

        uint64_t tmax = 0;
        for (signal_node_t* n = vcd->signals_head; n; n = n->next) {
            vcd_sort_signal_value_changes(&n->signal);
            if (n->signal.changes_count > 0) {
                const uint64_t ts = trace_vc_timestamp(&n->signal, n->signal.changes_count - 1);
                tmax = std::max(tmax, ts);
            }
        }
        vcd->trace_max_timestamp = tmax;
        fprintf(stderr, "[LXT1] Interlaced LXT loaded: %zu signals, max_ts=%llu\n",
            vcd->signals_count, (unsigned long long)tmax);
        return vcd;
    }

    if (sec.change_off == 0 || sec.zchg_predec_size == 0 || sec.zchg_size == 0) {
        vcd_free(vcd);
        fprintf(stderr,
            "[LXT1] Missing compressed change section. Use vcd2lxt -chgpack -linear or Bear2Wave gen-lxt.\n");
        return nullptr;
    }
    if (sec.time_table_off == 0) {
        vcd_free(vcd);
        fprintf(stderr,
            "[LXT1] Missing time table (required for linear LXT). Use vcd2lxt -linear or Bear2Wave gen-lxt.\n");
        return nullptr;
    }

    if (sec.change_off + sec.zchg_size > file.size()) {
        vcd_free(vcd);
        fprintf(stderr, "[LXT1] Truncated change section: %s\n", utf8_path);
        return nullptr;
    }
    std::vector<uint8_t> chg;
    if (!load_change_section(file, sec, chg)) {
        vcd_free(vcd);
        fprintf(stderr, "[LXT1] Failed to decompress changes: %s\n", utf8_path);
        return nullptr;
    }

    std::vector<std::pair<size_t, uint64_t>> time_markers;
    if (!load_time_markers(file, sec, time_markers)) {
        vcd_free(vcd);
        fprintf(stderr, "[LXT1] Failed to read time table: %s\n", utf8_path);
        return nullptr;
    }

    if (!apply_linear_changes(chg, facs, sig_by_fac, time_markers)) {
        vcd_free(vcd);
        fprintf(stderr,
            "[LXT1] Unsupported or corrupt linear change encoding in %s "
            "(try vcd2lxt2 / FST, or regenerate with tools gen-lxt)\n",
            utf8_path);
        return nullptr;
    }

    uint64_t tmax = 0;
    for (signal_node_t* n = vcd->signals_head; n; n = n->next) {
        vcd_sort_signal_value_changes(&n->signal);
        if (n->signal.changes_count > 0) {
            const uint64_t ts = trace_vc_timestamp(&n->signal, n->signal.changes_count - 1);
            tmax = std::max(tmax, ts);
        }
    }
    vcd->trace_max_timestamp = tmax;
    return vcd;
#endif
}
