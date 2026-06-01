#pragma once

#include <algorithm>
#include <cmath>

/** GTKWave-style snap of visible time range to 1/2/5/10 × 10^n. */
inline long long SnapPow10Range(long long rawRange)
{
    if (rawRange <= 0)
        return 10;
    const double raw = (double)rawRange;
    const double expv = floor(log10(raw));
    const double base = raw / pow(10.0, expv);
    double niceBase = 1.0;
    if (base < 1.5)
        niceBase = 1.0;
    else if (base < 3.0)
        niceBase = 2.0;
    else if (base < 7.0)
        niceBase = 5.0;
    else
        niceBase = 10.0;
    const long long snapped = (long long)llround(niceBase * pow(10.0, expv));
    return std::max(10LL, snapped);
}
