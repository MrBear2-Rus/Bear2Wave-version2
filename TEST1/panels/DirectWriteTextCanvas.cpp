#include "panels/DirectWriteTextCanvas.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif

#include <d2d1.h>
#include <dwrite.h>
#include <wincodec.h>

#ifdef DrawText
#undef DrawText
#endif

#include <wx/settings.h>

#include <algorithm>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>

#pragma comment(lib, "d2d1.lib")
#pragma comment(lib, "dwrite.lib")
#pragma comment(lib, "windowscodecs.lib")

namespace {

template <typename T>
struct ComRelease {
    void operator()(T* p) const
    {
        if (p)
            p->Release();
    }
};

template <typename T>
using ComPtr = std::unique_ptr<T, ComRelease<T>>;

static D2D1_COLOR_F WxToD2D(const wxColour& c, float alphaScale = 1.f)
{
    unsigned char a = c.Alpha();
    if (a == 0 && c.IsOk())
        a = 255;
    return D2D1::ColorF(c.Red() / 255.f, c.Green() / 255.f, c.Blue() / 255.f,
        (a / 255.f) * alphaScale);
}

static std::wstring ToWide(const wxString& s)
{
    return std::wstring(s.wc_str());
}

} // namespace

struct DirectWriteTextCanvas::Impl {
    ComPtr<ID2D1Factory> d2dFactory;
    ComPtr<IWICImagingFactory> wicFactory;
    ComPtr<IWICBitmap> wicBitmap;
    ComPtr<ID2D1RenderTarget> rt;
    ComPtr<IDWriteFactory> dwFactory;
    ComPtr<IDWriteTextFormat> textFormat;
    ComPtr<ID2D1SolidColorBrush> textBrush;
    ComPtr<ID2D1SolidColorBrush> fillBrush;
    ComPtr<ID2D1SolidColorBrush> strokeBrush;
};

bool DirectWriteTextEnabled()
{
    if (const char* v = std::getenv("BEAR2WAVE_DIRECTWRITE")) {
        if (v[0] == '0' && v[1] == '\0')
            return false;
    }
    return true;
}

std::unique_ptr<DirectWriteTextCanvas> DirectWriteTextCanvas::TryCreate(int width, int height)
{
    auto canvas = std::unique_ptr<DirectWriteTextCanvas>(new DirectWriteTextCanvas());
    if (!canvas->Initialize(width, height))
        return nullptr;
    return canvas;
}

DirectWriteTextCanvas::~DirectWriteTextCanvas()
{
    FlushDrawSession();
}

bool DirectWriteTextCanvas::Initialize(int width, int height)
{
    if (width <= 0 || height <= 0 || width > 8192 || height > 8192)
        return false;

    m_width = width;
    m_height = height;
    m_wxFont = wxSystemSettings::GetFont(wxSYS_DEFAULT_GUI_FONT);
    m_impl = std::make_unique<Impl>();

    ID2D1Factory* d2dRaw = nullptr;
    if (FAILED(D2D1CreateFactory(D2D1_FACTORY_TYPE_SINGLE_THREADED, &d2dRaw)) || !d2dRaw)
        return false;
    m_impl->d2dFactory.reset(d2dRaw);

    IWICImagingFactory* wicRaw = nullptr;
    if (FAILED(CoCreateInstance(CLSID_WICImagingFactory, nullptr, CLSCTX_INPROC_SERVER,
            IID_IWICImagingFactory, reinterpret_cast<void**>(&wicRaw)))
        || !wicRaw)
        return false;
    m_impl->wicFactory.reset(wicRaw);

    IWICBitmap* bmpRaw = nullptr;
    if (FAILED(m_impl->wicFactory->CreateBitmap(static_cast<UINT>(m_width), static_cast<UINT>(m_height),
            GUID_WICPixelFormat32bppPBGRA, WICBitmapCacheOnLoad, &bmpRaw))
        || !bmpRaw)
        return false;
    m_impl->wicBitmap.reset(bmpRaw);

    const D2D1_RENDER_TARGET_PROPERTIES rtProps = D2D1::RenderTargetProperties(
        D2D1_RENDER_TARGET_TYPE_DEFAULT,
        D2D1::PixelFormat(DXGI_FORMAT_B8G8R8A8_UNORM, D2D1_ALPHA_MODE_PREMULTIPLIED));
    ID2D1RenderTarget* rtRaw = nullptr;
    if (FAILED(m_impl->d2dFactory->CreateWicBitmapRenderTarget(m_impl->wicBitmap.get(), rtProps, &rtRaw))
        || !rtRaw)
        return false;
    m_impl->rt.reset(rtRaw);

    // ClearType requires an opaque background; GRAYSCALE produces correct alpha on transparent WIC bitmaps.
    m_impl->rt->SetTextAntialiasMode(D2D1_TEXT_ANTIALIAS_MODE_GRAYSCALE);
    m_impl->rt->SetAntialiasMode(D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);

    IDWriteFactory* dwRaw = nullptr;
    if (FAILED(DWriteCreateFactory(DWRITE_FACTORY_TYPE_SHARED, __uuidof(IDWriteFactory),
            reinterpret_cast<IUnknown**>(&dwRaw)))
        || !dwRaw)
        return false;
    m_impl->dwFactory.reset(dwRaw);

    UpdateTextFormat();
    UpdateBrushes();
    EnsureDrawSession();
    m_impl->rt->Clear(D2D1::ColorF(0, 0, 0, 0));
    return true;
}

void DirectWriteTextCanvas::EnsureDrawSession()
{
    if (!m_impl || !m_impl->rt || m_drawing)
        return;
    m_impl->rt->BeginDraw();
    m_drawing = true;
}

void DirectWriteTextCanvas::FlushDrawSession()
{
    if (!m_impl || !m_impl->rt || !m_drawing)
        return;
    m_impl->rt->EndDraw();
    m_drawing = false;
}

void DirectWriteTextCanvas::UpdateTextFormat()
{
    if (!m_impl || !m_impl->dwFactory)
        return;

    const wxString face = m_wxFont.GetFaceName();
    float size = static_cast<float>(m_wxFont.GetPixelSize().GetHeight());
    if (size <= 0.f) {
        const int pt = m_wxFont.GetPointSize();
        size = (pt > 0) ? static_cast<float>(pt) * 96.f / 72.f : 12.f;
    }
    size = std::max(8.f, size);
    const DWRITE_FONT_WEIGHT weight =
        m_wxFont.GetWeight() >= wxFONTWEIGHT_BOLD ? DWRITE_FONT_WEIGHT_BOLD : DWRITE_FONT_WEIGHT_NORMAL;
    const DWRITE_FONT_STYLE style =
        m_wxFont.GetStyle() == wxFONTSTYLE_ITALIC ? DWRITE_FONT_STYLE_ITALIC : DWRITE_FONT_STYLE_NORMAL;

    IDWriteTextFormat* fmtRaw = nullptr;
    HRESULT hrFmt = m_impl->dwFactory->CreateTextFormat(face.wc_str(), nullptr, weight, style,
        DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &fmtRaw);
    if (FAILED(hrFmt) || !fmtRaw) {
        if (FAILED(m_impl->dwFactory->CreateTextFormat(L"Segoe UI", nullptr, weight, style,
                DWRITE_FONT_STRETCH_NORMAL, size, L"en-us", &fmtRaw))
            || !fmtRaw)
            return;
    }
    m_impl->textFormat.reset(fmtRaw);
    if (m_impl->textFormat)
        m_impl->textFormat->SetWordWrapping(DWRITE_WORD_WRAPPING_NO_WRAP);
}

void DirectWriteTextCanvas::UpdateBrushes()
{
    if (!m_impl || !m_impl->rt)
        return;
    EnsureDrawSession();
    ID2D1SolidColorBrush* brushRaw = nullptr;
    if (SUCCEEDED(m_impl->rt->CreateSolidColorBrush(WxToD2D(m_textColour), &brushRaw)) && brushRaw)
        m_impl->textBrush.reset(brushRaw);
    brushRaw = nullptr;
    if (SUCCEEDED(m_impl->rt->CreateSolidColorBrush(WxToD2D(m_fillColour), &brushRaw)) && brushRaw)
        m_impl->fillBrush.reset(brushRaw);
    brushRaw = nullptr;
    if (SUCCEEDED(m_impl->rt->CreateSolidColorBrush(WxToD2D(m_strokeColour), &brushRaw)) && brushRaw)
        m_impl->strokeBrush.reset(brushRaw);
}

void DirectWriteTextCanvas::SetTextForeground(const wxColour& colour)
{
    m_textColour = colour;
    UpdateBrushes();
}

void DirectWriteTextCanvas::SetFont(const wxFont& font)
{
    m_wxFont = font;
    UpdateTextFormat();
}

wxSize DirectWriteTextCanvas::GetTextExtent(const wxString& text)
{
    if (!m_impl || !m_impl->dwFactory || !m_impl->textFormat)
        return wxSize(0, 0);
    const std::wstring ws = ToWide(text);
    IDWriteTextLayout* layoutRaw = nullptr;
    if (FAILED(m_impl->dwFactory->CreateTextLayout(ws.c_str(), static_cast<UINT32>(ws.size()),
            m_impl->textFormat.get(), 100000.f, 100000.f, &layoutRaw))
        || !layoutRaw)
        return wxSize(0, 0);
    ComPtr<IDWriteTextLayout> layout(layoutRaw);
    DWRITE_TEXT_METRICS metrics {};
    layout->GetMetrics(&metrics);
    return wxSize(static_cast<int>(metrics.width + 0.5f), static_cast<int>(metrics.height + 0.5f));
}

void DirectWriteTextCanvas::DrawTextAt(const wxString& text, int x, int y)
{
    if (!m_impl || !m_impl->rt || !m_impl->dwFactory || !m_impl->textFormat || !m_impl->textBrush)
        return;
    EnsureDrawSession();
    const std::wstring ws = ToWide(text);
    IDWriteTextLayout* layoutRaw = nullptr;
    if (FAILED(m_impl->dwFactory->CreateTextLayout(ws.c_str(), static_cast<UINT32>(ws.size()),
            m_impl->textFormat.get(), 100000.f, 100000.f, &layoutRaw))
        || !layoutRaw)
        return;
    ComPtr<IDWriteTextLayout> layout(layoutRaw);
    const D2D1_POINT_2F origin = { static_cast<float>(x), static_cast<float>(y) };
    m_impl->rt->DrawTextLayout(origin, layout.get(), m_impl->textBrush.get());
}

void DirectWriteTextCanvas::SetFillColour(const wxColour& colour)
{
    m_fillColour = colour;
    UpdateBrushes();
}

void DirectWriteTextCanvas::SetStrokeColour(const wxColour& colour, int width)
{
    m_strokeColour = colour;
    m_strokeWidth = static_cast<float>(std::max(1, width));
    UpdateBrushes();
}

void DirectWriteTextCanvas::DrawRectangle(int x, int y, int w, int h)
{
    if (!m_impl || !m_impl->rt || w <= 0 || h <= 0)
        return;
    EnsureDrawSession();
    const D2D1_RECT_F rect = {
        static_cast<float>(x), static_cast<float>(y),
        static_cast<float>(x + w), static_cast<float>(y + h),
    };
    if (m_impl->fillBrush)
        m_impl->rt->FillRectangle(rect, m_impl->fillBrush.get());
    if (m_impl->strokeBrush && m_strokeWidth > 0.f) {
        m_impl->rt->DrawRectangle(rect, m_impl->strokeBrush.get(), m_strokeWidth);
    }
}

void DirectWriteTextCanvas::PushClipRect(const wxRect& rect)
{
    if (!m_impl || !m_impl->rt)
        return;
    EnsureDrawSession();
    const D2D1_RECT_F r = {
        static_cast<float>(rect.x), static_cast<float>(rect.y),
        static_cast<float>(rect.x + rect.width), static_cast<float>(rect.y + rect.height),
    };
    m_impl->rt->PushAxisAlignedClip(r, D2D1_ANTIALIAS_MODE_PER_PRIMITIVE);
    ++m_clipDepth;
}

void DirectWriteTextCanvas::PopClip()
{
    if (!m_impl || !m_impl->rt || m_clipDepth <= 0)
        return;
    m_impl->rt->PopAxisAlignedClip();
    --m_clipDepth;
}

bool DirectWriteTextCanvas::ExportRgba(std::vector<std::uint8_t>& rgba)
{
    if (!m_impl || !m_impl->wicBitmap)
        return false;
    if (!m_drawing)
        EnsureDrawSession();
    if (m_drawing) {
        const HRESULT hr = m_impl->rt->EndDraw();
        m_drawing = false;
        if (FAILED(hr))
            return false;
    }

    IWICBitmapLock* lockRaw = nullptr;
    WICRect wicRect = { 0, 0, m_width, m_height };
    if (FAILED(m_impl->wicBitmap->Lock(&wicRect, WICBitmapLockRead, &lockRaw)) || !lockRaw)
        return false;
    ComPtr<IWICBitmapLock> lock(lockRaw);

    UINT w = 0, h = 0;
    lock->GetSize(&w, &h);
    UINT stride = 0;
    lock->GetStride(&stride);
    UINT bufSize = 0;
    WICInProcPointer pb = nullptr;
    if (FAILED(lock->GetDataPointer(&bufSize, &pb)) || !pb || w == 0 || h == 0)
        return false;

    rgba.assign(static_cast<size_t>(w) * h * 4, 0);
    for (UINT y = 0; y < h; ++y) {
        const BYTE* row = pb + static_cast<size_t>(y) * stride;
        for (UINT x = 0; x < w; ++x) {
            const BYTE b = row[x * 4 + 0];
            const BYTE g = row[x * 4 + 1];
            const BYTE r = row[x * 4 + 2];
            const BYTE a = row[x * 4 + 3];
            const size_t dst = static_cast<size_t>((y * w + x) * 4);
            if (a == 0) {
                rgba[dst + 0] = rgba[dst + 1] = rgba[dst + 2] = rgba[dst + 3] = 0;
                continue;
            }
            rgba[dst + 0] = static_cast<std::uint8_t>((static_cast<unsigned>(r) * 255 + a / 2) / a);
            rgba[dst + 1] = static_cast<std::uint8_t>((static_cast<unsigned>(g) * 255 + a / 2) / a);
            rgba[dst + 2] = static_cast<std::uint8_t>((static_cast<unsigned>(b) * 255 + a / 2) / a);
            rgba[dst + 3] = a;
        }
    }
    return true;
}
