#include "core/trace_niche_formats.h"

#include <cstring>

TraceNicheDisposition trace_niche_disposition(const char* ext)
{
    if (!ext || !ext[0])
        return TRACE_NICHE_NONE;

    if (!strcmp(ext, "saif"))
        return TRACE_NICHE_REJECTED;

    return TRACE_NICHE_NONE;
}

const char* trace_niche_user_message(const char* ext)
{
    if (!ext || !ext[0])
        return nullptr;

    if (!strcmp(ext, "saif"))
        return "SAIF is a power-activity format, not a time-domain waveform. "
               "Use a power analysis tool or convert to VCD/FST first.";

    return nullptr;
}

const char* trace_niche_doc_hint(const char* ext)
{
    if (!ext || !ext[0])
        return nullptr;

    if (!strcmp(ext, "saif"))
        return "\n\nSee docs/NICHE_FORMATS.md.";

    return nullptr;
}

void trace_niche_print_capabilities(FILE* out)
{
    if (!out)
        return;

    fprintf(out, "  shm trn         : external converter (simvisdbutil / shm2vcd wrapper)\n");
    fprintf(out, "  saif            : rejected (power format — not opened here)\n");
    fprintf(out, "  aet aet2 ae2    : external converter (aet2vcd / IBM SIMARAMA)\n");
    fprintf(out, "  fsdb native     : deferred (use fsdb2vcd via E4)\n");
    fprintf(out, "  b2wtrace.json   : deferred (schema in docs/B2W_TRACE_JSON_SCHEMA.md)\n");
}
