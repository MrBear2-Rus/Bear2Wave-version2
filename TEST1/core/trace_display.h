#pragma once

#include "vcd.h"

enum class Bear2waveTraceKind {
    DigitalScalar,
    BusBits,
    RealAnalog,
    TextString,
    TransactionEvent
};

Bear2waveTraceKind bear2wave_classify_trace_kind(const signal_t* sig);

bool bear2wave_parse_trace_double(const char* s, double* out);
