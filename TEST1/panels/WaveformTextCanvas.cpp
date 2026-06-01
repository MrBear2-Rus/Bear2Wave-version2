#include "panels/WaveformTextCanvas.h"

#include <wx/bitmap.h>
#include <wx/dc.h>
#include <wx/dcmemory.h>
#include <wx/image.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <vector>

namespace {

class WxMemoryTextCanvas : public WaveformTextCanvas {
public:
    explicit WxMemoryTextCanvas(int width, int height)
        : m_width(width)
        , m_height(height)
    {
        if (m_width <= 0 || m_height <= 0 || m_width > 8192 || m_height > 8192) {
            m_width = 1;
            m_height = 1;
        }
        m_bitmap = new wxBitmap(m_width, m_height, 24);
        m_dc = new wxMemoryDC(*m_bitmap);
        m_dc->SetBackground(*wxWHITE_BRUSH);
        m_dc->Clear();
        m_dc->SetBackgroundMode(wxTRANSPARENT);
    }

    ~WxMemoryTextCanvas() override
    {
        if (m_dc) {
            m_dc->SelectObject(wxNullBitmap);
            delete m_dc;
            m_dc = nullptr;
        }
        delete m_bitmap;
        m_bitmap = nullptr;
    }

    int Width() const override { return m_width; }
    int Height() const override { return m_height; }

    void SetTextForeground(const wxColour& colour) override { m_dc->SetTextForeground(colour); }
    void SetFont(const wxFont& font) override { m_dc->SetFont(font); }
    wxFont GetFont() const override { return m_dc->GetFont(); }
    wxSize GetTextExtent(const wxString& text) override { return m_dc->GetTextExtent(text); }
    void DrawTextAt(const wxString& text, int x, int y) override { m_dc->DrawText(text, x, y); }

    void SetFillColour(const wxColour& colour) override { m_dc->SetBrush(wxBrush(colour)); }
    void SetStrokeColour(const wxColour& colour, int width) override { m_dc->SetPen(wxPen(colour, width)); }
    void DrawRectangle(int x, int y, int w, int h) override { m_dc->DrawRectangle(x, y, w, h); }

    void PushClipRect(const wxRect& rect) override { m_dc->SetClippingRegion(rect); }
    void PopClip() override { m_dc->DestroyClippingRegion(); }

    bool ExportRgba(std::vector<std::uint8_t>& rgba) override
    {
        if (!m_bitmap)
            return false;
        wxImage img = m_bitmap->ConvertToImage();
        if (!img.IsOk())
            return false;
        const int w = img.GetWidth();
        const int h = img.GetHeight();
        const unsigned char* rgb = img.GetData();
        if (!rgb || w <= 0 || h <= 0)
            return false;

        rgba.assign(static_cast<size_t>(w) * h * 4, 0);
        for (int y = 0; y < h; ++y) {
            for (int x = 0; x < w; ++x) {
                const int src = (y * w + x) * 3;
                const size_t dst = static_cast<size_t>((y * w + x) * 4);
                const unsigned char r = rgb[src + 0];
                const unsigned char g = rgb[src + 1];
                const unsigned char b = rgb[src + 2];
                rgba[dst + 0] = r;
                rgba[dst + 1] = g;
                rgba[dst + 2] = b;
                const unsigned char maxc = std::max(r, std::max(g, b));
                const unsigned char minc = std::min(r, std::min(g, b));
                const unsigned chroma = static_cast<unsigned>(maxc) - minc;
                const unsigned avg =
                    (static_cast<unsigned>(r) + static_cast<unsigned>(g) + static_cast<unsigned>(b)) / 3u;
                unsigned char a;
                if (chroma <= 10u) {
                    a = static_cast<unsigned char>(255u - avg);
                } else if (avg <= 180u) {
                    a = static_cast<unsigned char>(255u - avg);
                } else {
                    a = 255u;
                }
                rgba[dst + 3] = a;
            }
        }
        return true;
    }

private:
    int m_width = 0;
    int m_height = 0;
    wxBitmap* m_bitmap = nullptr;
    wxMemoryDC* m_dc = nullptr;
};

} // namespace

#if defined(_WIN32)
#include "panels/DirectWriteTextCanvas.h"
#endif

std::unique_ptr<WaveformTextCanvas> CreateWxTextCanvas(int width, int height)
{
    return std::make_unique<WxMemoryTextCanvas>(width, height);
}

bool TextRgbaHasVisiblePixels(const std::vector<std::uint8_t>& rgba)
{
    for (size_t i = 3; i < rgba.size(); i += 4) {
        if (rgba[i] > 8)
            return true;
    }
    return false;
}

std::unique_ptr<WaveformTextCanvas> CreateTextCanvas(int width, int height)
{
#if defined(_WIN32)
    if (DirectWriteTextEnabled()) {
        if (auto dw = DirectWriteTextCanvas::TryCreate(width, height))
            return dw;
    }
#endif
    return CreateWxTextCanvas(width, height);
}
