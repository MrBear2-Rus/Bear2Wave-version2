#include "trace_loader.h"

#include "core/waveform_perf.h"

#include "fst_loader.h"

#include "ghw_loader.h"

#include "lxt2_loader.h"

#include "vzt_loader.h"



#include <algorithm>

#include <cctype>

#include <cstdio>

#include <cstring>

#include <filesystem>
#include <cstdio>



static void trace_set_err(char* err_buf, size_t err_buf_len, const char* msg)

{

    if (!err_buf || err_buf_len == 0)

        return;

    strncpy(err_buf, msg ? msg : "", err_buf_len - 1);

    err_buf[err_buf_len - 1] = '\0';

}



const char* trace_format_extension(const char* path)

{

    static char extbuf[16];

    extbuf[0] = '\0';

    if (!path)

        return extbuf;

    const char* dot = strrchr(path, '.');

    if (!dot || !dot[1])

        return extbuf;

    size_t i = 0;

    for (const char* p = dot + 1; *p && i + 1 < sizeof(extbuf); ++p)

        extbuf[i++] = (char)tolower((unsigned char)*p);

    extbuf[i] = '\0';

    return extbuf;

}



int trace_format_supported(const char* path)

{

    const char* ext = trace_format_extension(path);

    return strcmp(ext, "vcd") == 0 || strcmp(ext, "fst") == 0 || strcmp(ext, "vzt") == 0

        || strcmp(ext, "lxt") == 0 || strcmp(ext, "lxt2") == 0 || strcmp(ext, "ghw") == 0

        || strcmp(ext, "csv") == 0;

}



int trace_uses_lazy_backend(const vcd_t* vcd)

{

    if (!vcd)

        return 0;

    return vcd->trace_backend != VCD_TRACE_BACKEND_NONE;

}



void trace_compute_padded_range(

    uint64_t view_t0,

    uint64_t view_t1,

    uint64_t file_max,

    double margin_ratio,

    uint64_t* out_t0,

    uint64_t* out_t1)

{

    if (!out_t0 || !out_t1)

        return;



    if (view_t1 < view_t0) {

        const uint64_t tmp = view_t0;

        view_t0 = view_t1;

        view_t1 = tmp;

    }



    uint64_t span = view_t1 - view_t0;

    if (span == 0)

        span = 1;



    const uint64_t margin = static_cast<uint64_t>(static_cast<double>(span) * margin_ratio);

    uint64_t t0 = (view_t0 > margin) ? (view_t0 - margin) : 0;

    uint64_t t1 = view_t1 + margin;

    if (file_max > 0 && t1 > file_max)

        t1 = file_max;



    *out_t0 = t0;

    *out_t1 = t1;

}



int trace_load_signals(vcd_t* vcd, signal_t** sigs, size_t count, uint64_t t0, uint64_t t1)

{

    if (!vcd || !sigs || count == 0)

        return -1;



    switch (vcd->trace_backend) {

    case VCD_TRACE_BACKEND_FST_LAZY:

        return fst_load_signals(vcd, sigs, count, t0, t1);

    case VCD_TRACE_BACKEND_VZT_LAZY:

        return vzt_load_signals(vcd, sigs, count, t0, t1);

    case VCD_TRACE_BACKEND_LXT2_LAZY:

        return lxt2_load_signals(vcd, sigs, count, t0, t1);

    case VCD_TRACE_BACKEND_GHW_LAZY:

        return ghw_load_signals(vcd, sigs, count, t0, t1);

    default:

        return 0;

    }

}



void trace_loader_request_cancel(vcd_t* vcd)

{

    if (!vcd)

        return;

    switch (vcd->trace_backend) {

    case VCD_TRACE_BACKEND_FST_LAZY:

        fst_loader_request_cancel(vcd);

        break;

    case VCD_TRACE_BACKEND_VZT_LAZY:

        vzt_loader_request_cancel(vcd);

        break;

    case VCD_TRACE_BACKEND_LXT2_LAZY:

        lxt2_loader_request_cancel(vcd);

        break;

    default:

        break;

    }

}



vcd_t* trace_load_from_path(const char* utf8_path, char* err_buf, size_t err_buf_len)

{

    if (!utf8_path || !utf8_path[0]) {

        trace_set_err(err_buf, err_buf_len, "empty path");

        return nullptr;

    }



    const char* ext = trace_format_extension(utf8_path);



    if (strcmp(ext, "vcd") == 0) {

        vcd_t* v = vcd_read_from_path(const_cast<char*>(utf8_path));

        if (!v) {

            trace_set_err(err_buf, err_buf_len, "VCD parse failed");

            return nullptr;

        }

        const int warn_mb = WaveformPerf::VcdWarnThresholdMb();

        if (warn_mb > 0) {

            std::error_code ec;

            const auto sz = std::filesystem::file_size(utf8_path, ec);

            if (!ec && sz > static_cast<uintmax_t>(warn_mb) * 1024u * 1024u) {

                const double mb = static_cast<double>(sz) / (1024.0 * 1024.0);

                char msg[512];

                snprintf(msg, sizeof(msg),

                    "WARN: VCD is %.1f MB (>%d MB). Large files load entirely into RAM. "

                    "For faster viewing, convert to FST: vcd2fst \"%s\" waves.fst",

                    mb, warn_mb, utf8_path);

                trace_set_err(err_buf, err_buf_len, msg);

            }

        }

        return v;

    }



    if (strcmp(ext, "fst") == 0) {

        vcd_t* v = fst_open_lazy(utf8_path);

        if (!v)

            trace_set_err(err_buf, err_buf_len, "FST open failed");

        return v;

    }



    if (strcmp(ext, "vzt") == 0) {

        vcd_t* v = vzt_open_lazy(utf8_path);

        if (!v)

            trace_set_err(err_buf, err_buf_len,

                "VZT open failed (enable BEAR2WAVE_WITH_VZT — see docs/TRACE_FORMATS_BUILD.md)");

        return v;

    }



    if (strcmp(ext, "lxt") == 0 || strcmp(ext, "lxt2") == 0) {

        vcd_t* v = lxt2_open_lazy(utf8_path);

        if (!v)

            trace_set_err(err_buf, err_buf_len,

                "LXT/LXT2 open failed (enable BEAR2WAVE_WITH_LXT2 — see docs/TRACE_FORMATS_BUILD.md)");

        return v;

    }



    if (strcmp(ext, "ghw") == 0) {

        vcd_t* v = ghw_open_lazy(utf8_path);

        if (!v)

            trace_set_err(err_buf, err_buf_len,

                "GHW open failed (enable BEAR2WAVE_WITH_GHW — see docs/TRACE_FORMATS_BUILD.md)");

        return v;

    }



    trace_set_err(err_buf, err_buf_len, "unsupported trace extension");

    return nullptr;

}



void vcd_trace_session_release(vcd_t* vcd)

{

    if (!vcd)

        return;



    switch (vcd->trace_backend) {

    case VCD_TRACE_BACKEND_FST_LAZY:

        if (vcd->trace_session)

            fst_trace_session_destroy(vcd->trace_session);

        break;

    case VCD_TRACE_BACKEND_VZT_LAZY:

        if (vcd->trace_session)

            vzt_trace_session_destroy(vcd->trace_session);

        break;

    case VCD_TRACE_BACKEND_LXT2_LAZY:

        if (vcd->trace_session)

            lxt2_trace_session_destroy(vcd->trace_session);

        break;

    case VCD_TRACE_BACKEND_GHW_LAZY:

        if (vcd->trace_session)

            ghw_trace_session_destroy(vcd->trace_session);

        break;

    default:

        break;

    }



    vcd->trace_session = nullptr;

    vcd->trace_backend = VCD_TRACE_BACKEND_NONE;

    vcd->trace_max_timestamp = 0;

}

