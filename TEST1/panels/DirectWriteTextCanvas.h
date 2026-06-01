#pragma once

#include "panels/WaveformTextCanvas.h"

#include <memory>

/** DirectWrite + Direct2D grayscale text on a transparent WIC bitmap. */
class DirectWriteTextCanvas : public WaveformTextCanvas {
public:
    static std::unique_ptr<DirectWriteTextCanvas> TryCreate(int width, int height);
    ~DirectWriteTextCanvas() override;

    int Width() const override { return m_width; }
    int Height() const override { return m_height; }

    void SetTextForeground(const wxColour& colour) override;
    void SetFont(const wxFont& font) override;
    wxFont GetFont() const override { return m_wxFont; }
    wxSize GetTextExtent(const wxString& text) override;
    void DrawTextAt(const wxString& text, int x, int y) override;

    void SetFillColour(const wxColour& colour) override;
    void SetStrokeColour(const wxColour& colour, int width = 1) override;
    void DrawRectangle(int x, int y, int w, int h) override;

    void PushClipRect(const wxRect& rect) override;
    void PopClip() override;

    bool ExportRgba(std::vector<std::uint8_t>& rgba) override;

private:
    DirectWriteTextCanvas() = default;
    bool Initialize(int width, int height);
    void EnsureDrawSession();
    void FlushDrawSession();
    void UpdateTextFormat();
    void UpdateBrushes();

    int m_width = 0;
    int m_height = 0;
    wxFont m_wxFont;
    wxColour m_textColour = *wxBLACK;
    wxColour m_fillColour = *wxWHITE;
    wxColour m_strokeColour = *wxBLACK;
    float m_strokeWidth = 1.f;
    bool m_drawing = false;
    int m_clipDepth = 0;

    struct Impl;
    std::unique_ptr<Impl> m_impl;
};

bool DirectWriteTextEnabled();
