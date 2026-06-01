#include "csv_loader.h"

#include "core/trace_var_types.h"
#include "csv.h"
#include "vcd.h"

#include <cstdio>
#include <cstring>
#include <string>

static void csv_set_err(char* err_buf, size_t err_buf_len, const char* msg)
{
    if (!err_buf || err_buf_len == 0)
        return;
    strncpy(err_buf, msg ? msg : "", err_buf_len - 1);
    err_buf[err_buf_len - 1] = '\0';
}

static void append_csv_signal(vcd_t* vcd, signal_node_t* node)
{
    if (!vcd->signals_head)
        vcd->signals_head = node;
    else {
        signal_node_t* p = vcd->signals_head;
        while (p->next)
            p = p->next;
        p->next = node;
    }
    vcd->signals_count++;
}

static vcd_t* csv_build_vcd_from_parser(CSVParser& parser, char* err_buf, size_t err_buf_len)
{
    if (!parser.HasHeaders() || parser.GetRowCount() == 0) {
        csv_set_err(err_buf, err_buf_len, "CSV: need header row and at least one data row");
        return nullptr;
    }

    const std::vector<std::string> headers = parser.GetHeaders();
    if (headers.size() < 2) {
        csv_set_err(err_buf, err_buf_len, "CSV: need time column plus at least one signal column");
        return nullptr;
    }

    vcd_t* vcd = vcd_alloc_empty();
    if (!vcd) {
        csv_set_err(err_buf, err_buf_len, "CSV: out of memory");
        return nullptr;
    }

    strncpy(vcd->version, "Bear2Wave CSV loader", VCD_VERSION_SIZE - 1);
    vcd->version[VCD_VERSION_SIZE - 1] = '\0';
    strncpy(vcd->timescale.unit, "ns", VCD_TIME_UNIT_SIZE - 1);
    vcd->timescale.unit[VCD_TIME_UNIT_SIZE - 1] = '\0';
    vcd->timescale.scale = 1;

    const std::string& timeHeader = headers[0];
    uint64_t max_ts = 0;

    for (size_t col = 1; col < headers.size(); ++col) {
        const std::string& signalName = headers[col];
        if (signalName.empty())
            continue;

        signal_node_t* node = static_cast<signal_node_t*>(calloc(1, sizeof(signal_node_t)));
        if (!node) {
            vcd_free(vcd);
            csv_set_err(err_buf, err_buf_len, "CSV: out of memory");
            return nullptr;
        }

        signal_t* sig = &node->signal;
        sig->fst_var_type = BEAR2WAVE_VT_VCD_ONLY;
        strncpy(sig->name, signalName.c_str(), VCD_NAME_SIZE - 1);
        sig->name[VCD_NAME_SIZE - 1] = '\0';
        strncpy(sig->module_path, "CSV", VCD_SIGNAL_SIZE - 1);
        sig->module_path[VCD_SIGNAL_SIZE - 1] = '\0';
        snprintf(sig->full_name, VCD_SIGNAL_SIZE, "CSV.%s", signalName.c_str());
        sig->full_name[VCD_SIGNAL_SIZE - 1] = '\0';
        snprintf(sig->signal_id, VCD_NAME_SIZE, "csv_%zu", col);
        sig->signal_id[VCD_NAME_SIZE - 1] = '\0';
        sig->size = 1;

        bool any = false;
        for (size_t row = 0; row < parser.GetRowCount(); ++row) {
            const std::string timeStr = parser.GetValue(row, timeHeader);
            const std::string valueStr = parser.GetValue(row, signalName);
            if (timeStr.empty() || valueStr.empty())
                continue;
            try {
                const long long tsll = std::stoll(timeStr);
                const timestamp_t ts = (tsll < 0) ? 0 : static_cast<timestamp_t>(tsll);
                if (vcd_signal_append_change(sig, ts, valueStr.c_str()) != 0)
                    break;
                any = true;
                if (static_cast<uint64_t>(ts) > max_ts)
                    max_ts = static_cast<uint64_t>(ts);
            } catch (...) {
            }
        }

        if (!any) {
            free(node);
            continue;
        }

        append_csv_signal(vcd, node);
    }

    if (vcd->signals_count == 0) {
        vcd_free(vcd);
        csv_set_err(err_buf, err_buf_len, "CSV: no signal columns with value changes");
        return nullptr;
    }

    vcd->trace_max_timestamp = max_ts > 0 ? max_ts : 1;
    return vcd;
}

vcd_t* csv_vcd_from_parser(CSVParser& parser, char* err_buf, size_t err_buf_len)
{
    return csv_build_vcd_from_parser(parser, err_buf, err_buf_len);
}

vcd_t* csv_read_to_vcd(const char* utf8_path, char* err_buf, size_t err_buf_len)
{
    if (!utf8_path || !utf8_path[0]) {
        csv_set_err(err_buf, err_buf_len, "CSV: empty path");
        return nullptr;
    }

    CSVParser parser;
    if (!parser.LoadFromFile(utf8_path)) {
        csv_set_err(err_buf, err_buf_len, "CSV: failed to read file");
        return nullptr;
    }

    return csv_build_vcd_from_parser(parser, err_buf, err_buf_len);
}
