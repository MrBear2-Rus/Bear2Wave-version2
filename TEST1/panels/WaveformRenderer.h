#pragma once

#include "vcd.h"

#include <cstdint>
#include <string>
#include <vector>

class WaveformPanel;

namespace WaveformRenderer {

enum class WaveTraceKind {
    DigitalScalar,
    BusBits,
    RealAnalog,
    TextString,
    TransactionEvent
};

struct DrawSegment {
    int x1 = 0;
    int x2 = 0;
    int y = 0;
    char value = '0';
    std::string text;
    WaveTraceKind traceKind = WaveTraceKind::DigitalScalar;
};

/** GTKWave-style trace transforms (shared with WaveformPanel). */
inline constexpr int TR_INVERT = 1;
inline constexpr int TR_REVERSE_BITS = 2;
inline constexpr int TR_RIGHT_JUSTIFY = 4;
inline constexpr int TR_POPCNT = 8;
inline constexpr int TR_RANGE_FILL = 16;
inline constexpr int TR_TRANSLATE_PROC = 32;

bool ParseTraceDouble(const char* s, double* out);
WaveTraceKind ClassifyTraceKind(const signal_t* sig);
char ParseVcdValue(const char* v);

/** ns/us/ms/s style label for ruler (UTF-8). */
std::string FormatTimeLabel(double t);

/** Background cache build (was WaveformPanel::BuildDrawCacheAsync). */
void BuildCacheAsync(WaveformPanel& panel);

} // namespace WaveformRenderer
