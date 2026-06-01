#pragma once

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <sstream>
#include <string>
#include <vector>

/** Radix / value formatting (GTKWave-style), shared by waveform panel and session files. */
namespace WaveformRadix {

enum class Radix {
    Binary = 0,
    Octal = 1,
    Decimal = 2,
    Hex = 4,
    Signed = 5,
    Ascii = 6,
    Real = 7
};

inline int ToGtkwaveCode(Radix r)
{
    switch (r) {
    case Radix::Binary: return 0;
    case Radix::Octal: return 1;
    case Radix::Decimal: return 2;
    case Radix::Hex: return 4;
    case Radix::Signed: return 5;
    case Radix::Ascii: return 6;
    case Radix::Real: return 7;
    }
    return 0;
}

inline Radix FromGtkwaveCode(int code)
{
    switch (code) {
    case 0: return Radix::Binary;
    case 1: return Radix::Octal;
    case 2: return Radix::Decimal;
    case 4: return Radix::Hex;
    case 5: return Radix::Signed;
    case 6: return Radix::Ascii;
    case 7: return Radix::Real;
    default: return Radix::Binary;
    }
}

/** GTKWave writes "@NNN" where NNN = radix * 100 (e.g. @200 = decimal). */
inline Radix FromGtkwaveAtLine(const std::string& line)
{
    if (line.empty() || line[0] != '@')
        return Radix::Binary;
    int v = 0;
    try {
        v = std::stoi(line.substr(1));
    } catch (...) {
        return Radix::Binary;
    }
    return FromGtkwaveCode(v / 100);
}

inline const char* RadixName(Radix r)
{
    switch (r) {
    case Radix::Binary: return "binary";
    case Radix::Octal: return "octal";
    case Radix::Decimal: return "decimal";
    case Radix::Hex: return "hex";
    case Radix::Signed: return "signed";
    case Radix::Ascii: return "ascii";
    case Radix::Real: return "real";
    }
    return "binary";
}

inline bool ParseRadixName(const std::string& s, Radix& out)
{
    std::string k = s;
    for (char& c : k)
        c = (char)tolower((unsigned char)c);
    if (k == "bin" || k == "binary" || k == "b")
        out = Radix::Binary;
    else if (k == "oct" || k == "octal" || k == "o")
        out = Radix::Octal;
    else if (k == "dec" || k == "decimal" || k == "d")
        out = Radix::Decimal;
    else if (k == "hex" || k == "hexadecimal" || k == "h")
        out = Radix::Hex;
    else if (k == "signed" || k == "s")
        out = Radix::Signed;
    else if (k == "ascii" || k == "a")
        out = Radix::Ascii;
    else if (k == "real" || k == "r" || k == "float")
        out = Radix::Real;
    else
        return false;
    return true;
}

struct DecodedValue {
    bool valid = false;
    bool hasNon01 = false;
    int width = 0;
    std::string bits;
    std::string raw;
};

inline bool Is01(char c)
{
    return c == '0' || c == '1';
}

inline char NormalizeScalar(char c)
{
    c = (char)tolower((unsigned char)c);
    if (c == 'l') return '0';
    if (c == 'h') return '1';
    if (c == 'u' || c == 'w' || c == 'd') return 'x';
    if (c == '0' || c == '1' || c == 'x' || c == 'z')
        return c;
    return 'x';
}

inline bool BitsHaveNon01(const std::string& b)
{
    for (char c : b) {
        if (!Is01(c))
            return true;
    }
    return false;
}

inline std::string StripVcdPrefix(const char* v)
{
    if (!v || !v[0])
        return "";
    const unsigned char c0 = (unsigned char)v[0];
    if (c0 == 'b' || c0 == 'B' || c0 == 'r' || c0 == 'R' || c0 == 'o' || c0 == 'O' || c0 == 'd' || c0 == 'D'
        || c0 == 'h' || c0 == 'H')
        return std::string(v + 1);
    return std::string(v);
}

inline bool ParseUInt(const std::string& s, int base, unsigned long long& out)
{
    if (s.empty())
        return false;
    try {
        size_t pos = 0;
        out = std::stoull(s, &pos, base);
        return pos == s.size();
    } catch (...) {
        return false;
    }
}

/** Expand VCD literal to a bit string (MSB first); preserves x/z in vector form. */
inline DecodedValue DecodeVcdLiteral(const char* v, int signalWidth = 0)
{
    DecodedValue d;
    if (!v || !v[0])
        return d;
    d.raw = v;
    d.valid = true;

    const unsigned char c0 = (unsigned char)v[0];
    if (c0 == 'b' || c0 == 'B' || c0 == 'r' || c0 == 'R') {
        d.bits = v + 1;
        d.width = signalWidth > 0 ? signalWidth : (int)d.bits.size();
        d.hasNon01 = BitsHaveNon01(d.bits);
        return d;
    }
    if (c0 == 'o' || c0 == 'O') {
        unsigned long long n = 0;
        if (!ParseUInt(v + 1, 8, n)) {
            d.valid = false;
            return d;
        }
        std::ostringstream oss;
        for (int i = 0; i < 21; ++i) {
            if (n == 0 && i > 0)
                break;
            oss << ((n & 1ULL) ? '1' : '0');
            n >>= 1;
        }
        std::string rev = oss.str();
        d.bits.assign(rev.rbegin(), rev.rend());
        while (d.bits.size() < 3)
            d.bits.insert(d.bits.begin(), '0');
        d.width = signalWidth > 0 ? signalWidth : (int)d.bits.size();
        return d;
    }
    if (c0 == 'd' || c0 == 'D') {
        unsigned long long n = 0;
        if (!ParseUInt(v + 1, 10, n)) {
            d.valid = false;
            return d;
        }
        std::ostringstream oss;
        do {
            oss << ((n & 1ULL) ? '1' : '0');
            n >>= 1;
        } while (n);
        std::string rev = oss.str();
        if (rev.empty())
            rev = "0";
        d.bits.assign(rev.rbegin(), rev.rend());
        d.width = signalWidth > 0 ? signalWidth : (int)d.bits.size();
        return d;
    }
    if (c0 == 'h' || c0 == 'H') {
        unsigned long long n = 0;
        if (!ParseUInt(v + 1, 16, n)) {
            d.valid = false;
            return d;
        }
        std::ostringstream oss;
        do {
            oss << ((n & 1ULL) ? '1' : '0');
            n >>= 1;
        } while (n);
        std::string rev = oss.str();
        if (rev.empty())
            rev = "0";
        d.bits.assign(rev.rbegin(), rev.rend());
        d.width = signalWidth > 0 ? signalWidth : (int)d.bits.size();
        return d;
    }

  /* scalar or bare vector */
    if (v[1] == '\0') {
        char sc = NormalizeScalar(v[0]);
        d.bits = sc;
        d.width = 1;
        d.hasNon01 = !Is01(sc);
        return d;
    }

    d.bits = StripVcdPrefix(v);
    if (d.bits == v)
        d.bits = v;
    d.width = signalWidth > 0 ? signalWidth : (int)d.bits.size();
    d.hasNon01 = BitsHaveNon01(d.bits);
    return d;
}

inline void PadBitsLeft(std::string& bits, int width)
{
    if (width <= (int)bits.size())
        return;
    bits.insert(bits.begin(), (size_t)(width - bits.size()), '0');
}

inline unsigned long long BitsToU64(const std::string& bits)
{
    unsigned long long n = 0;
    for (char c : bits) {
        if (!Is01(c))
            return 0;
        n = (n << 1) | (c == '1' ? 1ULL : 0ULL);
    }
    return n;
}

inline long long BitsToS64(const std::string& bits)
{
    if (bits.empty())
        return 0;
    if (BitsHaveNon01(bits))
        return 0;
    unsigned long long u = BitsToU64(bits);
    const size_t w = bits.size();
    if (w == 0)
        return 0;
    const unsigned long long sign = 1ULL << (w - 1);
    if (u & sign)
        return (long long)(u - (1ULL << w));
    return (long long)u;
}

inline std::string FormatHexU64(unsigned long long v, int widthBits, bool upper = false)
{
    const int nibbles = std::max(1, (widthBits + 3) / 4);
    std::ostringstream oss;
    oss << (upper ? std::uppercase : std::nouppercase) << std::hex << v;
    std::string h = oss.str();
    if ((int)h.size() < nibbles)
        h.insert(h.begin(), (size_t)(nibbles - h.size()), '0');
    return h;
}

inline std::string FormatOctalU64(unsigned long long v, int widthBits)
{
    std::ostringstream oss;
    oss << std::oct << v;
    std::string o = oss.str();
    const int need = std::max(1, (widthBits + 2) / 3);
    if ((int)o.size() < need)
        o.insert(o.begin(), (size_t)(need - o.size()), '0');
    return o;
}

inline std::string FormatRealFromBits(const std::string& bits)
{
    if (BitsHaveNon01(bits))
        return "X";
    if (bits.size() == 32) {
        const uint32_t u = (uint32_t)BitsToU64(bits);
        float f = 0.f;
        std::memcpy(&f, &u, sizeof(f));
        std::ostringstream oss;
        oss.precision(8);
        oss << f;
        return oss.str();
    }
    if (bits.size() == 64) {
        const uint64_t u = BitsToU64(bits);
        double d = 0.0;
        std::memcpy(&d, &u, sizeof(d));
        std::ostringstream oss;
        oss.precision(16);
        oss << d;
        return oss.str();
    }
    std::ostringstream oss;
    oss << (double)BitsToU64(bits);
    return oss.str();
}

inline std::string FormatAsciiFromBits(const std::string& bits)
{
    if (bits.empty() || (bits.size() % 8) != 0 || BitsHaveNon01(bits))
        return bits;
    std::string out;
    for (size_t i = 0; i < bits.size(); i += 8) {
        unsigned char c = 0;
        for (int b = 0; b < 8; ++b)
            c = (unsigned char)((c << 1) | (bits[i + (size_t)b] == '1' ? 1 : 0));
        out.push_back((c >= 32 && c < 127) ? (char)c : '.');
    }
    return out;
}

inline std::string FormatValue(const char* vcdValue, Radix radix, int signalWidth = 0)
{
    DecodedValue d = DecodeVcdLiteral(vcdValue, signalWidth);
    if (!d.valid)
        return vcdValue ? std::string(vcdValue) : std::string();

    if (d.hasNon01 && radix != Radix::Binary)
        return d.bits.empty() ? d.raw : d.bits;

    int w = d.width > 0 ? d.width : (int)d.bits.size();
    if (w <= 0)
        w = 1;
    std::string bits = d.bits;
    PadBitsLeft(bits, w);

    switch (radix) {
    case Radix::Binary:
        return bits;
    case Radix::Octal:
        if (BitsHaveNon01(bits))
            return bits;
        return FormatOctalU64(BitsToU64(bits), w);
    case Radix::Decimal:
        if (BitsHaveNon01(bits))
            return bits;
        return std::to_string(BitsToU64(bits));
    case Radix::Hex:
        if (BitsHaveNon01(bits))
            return bits;
        return FormatHexU64(BitsToU64(bits), w, true);
    case Radix::Signed:
        if (BitsHaveNon01(bits))
            return bits;
        return std::to_string(BitsToS64(bits));
    case Radix::Ascii:
        return FormatAsciiFromBits(bits);
    case Radix::Real:
        return FormatRealFromBits(bits);
    }
    return bits;
}

} // namespace WaveformRadix
