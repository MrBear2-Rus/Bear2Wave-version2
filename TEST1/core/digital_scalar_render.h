#pragma once

#include <wx/colour.h>
#include <wx/gdicmn.h>

/** GTKWave-style 0/1/X/Z digital scalar layout and colours. */
namespace DigitalScalarRender {

inline bool IsFourState(char v)
{
    return v == '0' || v == '1' || v == 'x' || v == 'z';
}

inline bool IsFilledBlock(char v) { return v == 'x'; }

inline int LevelY(char v, int yHigh, int yLow, int yMid)
{
    switch (v) {
    case '0':
        return yLow;
    case '1':
        return yHigh;
    case 'z':
    case 'x':
        return yMid;
    default:
        return yLow;
    }
}

inline wxColour StateLineColour(char v, const wxColour& signalColour)
{
    switch (v) {
    case 'x':
        return wxColour(180, 40, 40);
    case 'z':
        return wxColour(210, 110, 0);
    default:
        return signalColour;
    }
}

inline wxColour StateFillColour(char v)
{
    if (v == 'x')
        return wxColour(255, 170, 170);
    return wxColour();
}

inline wxRect SegmentHitRect(char v, int x1, int x2, int yHigh, int yLow, int yMid, int scrollPx)
{
    const int bandH = yLow - yHigh;
    if (IsFilledBlock(v))
        return wxRect(x1, yHigh - scrollPx, std::max(1, x2 - x1), std::max(1, bandH));
    const int y = LevelY(v, yHigh, yLow, yMid) - scrollPx;
    return wxRect(x1, y - 6, std::max(1, x2 - x1), 12);
}

} // namespace DigitalScalarRender
