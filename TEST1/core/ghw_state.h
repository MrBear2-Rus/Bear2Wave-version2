#pragma once

#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/** Map IEEE 1164 / VCD 9-state first char to waveform level (0/1/x/z). */
char bear2wave_nine_state_waveform_char(char c);

/** Copy display label for cursor/labels (preserves U/W/L/H when single char). */
void bear2wave_nine_state_display_label(const char* value, char* buf, size_t buf_len);

#ifdef __cplusplus
}
#endif
