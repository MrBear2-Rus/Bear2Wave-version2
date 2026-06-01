#include "core/ghw_state.h"

#include <cctype>
#include <cstring>

char bear2wave_nine_state_waveform_char(char c)
{
    const unsigned char u = static_cast<unsigned char>(c);
    c = static_cast<char>(tolower(u));
    switch (c) {
    case '0':
    case 'l':
        return '0';
    case '1':
    case 'h':
        return '1';
    case 'x':
    case 'u':
    case 'w':
    case 'd':
        return 'x';
    case 'z':
        return 'z';
    default:
        return '0';
    }
}

void bear2wave_nine_state_display_label(const char* value, char* buf, size_t buf_len)
{
    if (!buf || buf_len == 0)
        return;
    buf[0] = '\0';
    if (!value || !value[0])
        return;

    if (value[1] == '\0') {
        const char c = value[0];
        if (c == '0' || c == '1' || c == 'x' || c == 'X' || c == 'z' || c == 'Z'
            || c == 'u' || c == 'U' || c == 'w' || c == 'W' || c == 'l' || c == 'L'
            || c == 'h' || c == 'H' || c == 'd' || c == 'D' || c == '-')
        {
            buf[0] = c;
            buf[1] = '\0';
            return;
        }
    }

    strncpy(buf, value, buf_len - 1);
    buf[buf_len - 1] = '\0';
}
