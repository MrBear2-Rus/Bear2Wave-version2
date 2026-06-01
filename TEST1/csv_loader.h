#pragma once

#include "vcd.h"

#ifdef __cplusplus
extern "C" {
#endif

/** Load CSV (header row + time column) into vcd_t; caller frees with vcd_free. */
vcd_t* csv_read_to_vcd(const char* utf8_path, char* err_buf, size_t err_buf_len);

#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
class CSVParser;
/** Build vcd_t from an already-loaded CSVParser (C++ only). */
vcd_t* csv_vcd_from_parser(CSVParser& parser, char* err_buf, size_t err_buf_len);
#endif
