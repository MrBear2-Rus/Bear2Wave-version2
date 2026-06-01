#pragma once

#include <cstdint>
#include <memory>
#include <vector>

#include <wx/colour.h>
#include <wx/font.h>
#include <wx/gdicmn.h>
#include <wx/string.h>

/** Abstraction for off-screen text/label rendering (wx GDI or DirectWrite). */
class WaveformTextCanvas {
public:
    virtual ~WaveformTextCanvas() = default;

    virtual int Width() const = 0;
    virtual int Height() const = 0;

    virtual void SetTextForeground(const wxColour& colour) = 0;
    virtual void SetFont(const wxFont& font) = 0;
    virtual wxFont GetFont() const = 0;
    virtual wxSize GetTextExtent(const wxString& text) = 0;
    virtual void DrawTextAt(const wxString& text, int x, int y) = 0;

    virtual void SetFillColour(const wxColour& colour) = 0;
    virtual void SetStrokeColour(const wxColour& colour, int width = 1) = 0;
    virtual void DrawRectangle(int x, int y, int w, int h) = 0;

    virtual void PushClipRect(const wxRect& rect) = 0;
    virtual void PopClip() = 0;

    /** RGBA 8-bit straight alpha, row-major, for GL upload. */
    virtual bool ExportRgba(std::vector<std::uint8_t>& rgba) = 0;
};

#if defined(_WIN32)
bool DirectWriteTextEnabled();
#endif

std::unique_ptr<WaveformTextCanvas> CreateTextCanvas(int width, int height);
/** wx GDI white-key fallback (always available). */
std::unique_ptr<WaveformTextCanvas> CreateWxTextCanvas(int width, int height);
/** True if any pixel has meaningful alpha (post-export sanity check). */
bool TextRgbaHasVisiblePixels(const std::vector<std::uint8_t>& rgba);
