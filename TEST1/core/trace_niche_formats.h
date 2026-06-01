#pragma once

#include <stdio.h>

#ifdef __cplusplus
extern "C" {
#endif

/** How Bear2Wave handles E5 niche extensions. */
typedef enum TraceNicheDisposition {
    TRACE_NICHE_NONE = 0,
    TRACE_NICHE_REJECTED, /**< e.g. .saif — not a waveform viewer format */
    TRACE_NICHE_DEFERRED  /**< reserved for future niche formats */
} TraceNicheDisposition;

TraceNicheDisposition trace_niche_disposition(const char* ext);

/** User-facing message when disposition != NONE; nullptr if none. */
const char* trace_niche_user_message(const char* ext);

/** Documentation pointer appended to open-failure hints. */
const char* trace_niche_doc_hint(const char* ext);

void trace_niche_print_capabilities(FILE* out);

#ifdef __cplusplus
}
#endif
