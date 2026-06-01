#include "core/waveform_hardcopy.h"

#include "panels/WaveformPanel.hpp"
#include "waveform_constants.h"

#include <wx/bitmap.h>
#include <wx/dcmemory.h>
#include <wx/filedlg.h>
#include <wx/image.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <fstream>
#include <mutex>
#include <string>
#include <vector>

namespace {

struct PageDims {
    int w;
    int h;
};

PageDims PageDimensions(WaveformHardcopy::PageSize ps)
{
    switch (ps) {
    case WaveformHardcopy::PageSize::A4:
        return {595, 842};
    case WaveformHardcopy::PageSize::Legal:
        return {612, 1008};
    case WaveformHardcopy::PageSize::Letter:
    default:
        return {612, 792};
    }
}

std::string PsEscape(const std::string& s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        if (c == '(' || c == ')' || c == '\\')
            out.push_back('\\');
        out.push_back(c);
    }
    return out;
}

std::string MifEscape(const std::string& s)
{
    std::string out;
    for (char c : s) {
        if (c == '`')
            out += "``";
        else if (c == '\\')
            out += "\\\\";
        else
            out.push_back(c);
    }
    return out;
}

void PageSizeToMifUnits(WaveformHardcopy::PageSize ps, double* wIn, double* hIn)
{
    const PageDims d = PageDimensions(ps);
    *wIn = d.w / 72.0;
    *hIn = d.h / 72.0;
}

bool WritePostScript(const WaveformPanel& panel, const wxString& path,
                     const WaveformHardcopy::Options& opts, wxString* errOut)
{
    const wxSize client = panel.GetClientSize();
    if (client.x <= 0 || client.y <= 0) {
        if (errOut)
            *errOut = "waveform panel has no size";
        return false;
    }

    const PageDims page = PageDimensions(opts.pageSize);
    const double scaleX = (double)page.w / (double)client.x;
    const double scaleY = (double)page.h / (double)client.y;
    const double scale = std::min(scaleX, scaleY);

    std::ofstream ofs(path.ToUTF8().data(), std::ios::out | std::ios::trunc);
    if (!ofs) {
        if (errOut)
            *errOut = "cannot open output file";
        return false;
    }

    const int outW = (int)(client.x * scale);
    const int outH = (int)(client.y * scale);

    ofs << "%!PS-Adobe-3.0 EPSF-3.0\n";
    ofs << "%%Creator: Bear2Wave\n";
    ofs << "%%BoundingBox: 0 0 " << outW << " " << outH << "\n";
    ofs << "%%Pages: 1\n";
    ofs << "%%EndComments\n";
    ofs << "%%BeginProlog\n";
    ofs << "/drawline { newpath moveto lineto stroke } bind def\n";
    ofs << "/drawrect { newpath moveto 4 {rlineto} repeat closepath fill } bind def\n";
    ofs << "%%EndProlog\n";
    ofs << "%%Page: 1 1\n";
    ofs << scale << " " << scale << " scale\n";
    ofs << "0 " << client.y << " translate\n";
    ofs << "1 -1 scale\n";

    auto toPsY = [&](int y) { return y; };

    ofs << "0.98 0.99 1 setrgbcolor\n";
    ofs << "0 0 " << client.x << " " << client.y << " drawrect\n";

    const int viewW = std::max(1, client.x - LEFT_MARGIN - WAVE_PADDING);
    const double tScale = (double)viewW / (double)panel.m_displayTimeRange;

    std::lock_guard<std::mutex> lock(panel.m_cacheMutex);
    const int safeCount = std::min((int)panel.m_displayedSignals2.size(),
                                   (int)panel.m_cachedSegments.size());
    const int scrollPx = panel.SignalRowScrollOffsetPx();
    const int firstRow = panel.m_signalScrollRow;
    const int lastRow = std::min(safeCount, firstRow + panel.VisibleSignalRowCount());

    ofs << "0.17 0.24 0.31 setrgbcolor\n";
    ofs << "1 setlinewidth\n";

    for (int i = firstRow; i < lastRow; ++i) {
        signal_t* sig = panel.m_displayedSignals2[i];
        if (!sig || i >= (int)panel.m_cachedSegments.size())
            continue;
        const auto& segments = panel.m_cachedSegments[i];
        for (const auto& seg : segments) {
            const int y = seg.y - scrollPx;
            if (seg.traceKind == WaveformPanel::WaveTraceKind::RealAnalog) {
                ofs << seg.x1 << " " << toPsY(y) << " moveto "
                    << seg.x2 << " " << toPsY(y) << " drawline\n";
            } else if (seg.traceKind == WaveformPanel::WaveTraceKind::BusBits
                       || seg.traceKind == WaveformPanel::WaveTraceKind::TextString
                       || seg.traceKind == WaveformPanel::WaveTraceKind::TransactionEvent) {
                ofs << "0.78 0.90 1 setrgbcolor\n";
                ofs << seg.x1 << " " << toPsY(y - 9) << " " << (seg.x2 - seg.x1) << " 18 drawrect\n";
                ofs << "0.17 0.24 0.31 setrgbcolor\n";
            } else {
                const int yCurr = (seg.value == '0') ? (y + 15) : (y - 15);
                ofs << seg.x1 << " " << toPsY(yCurr) << " moveto "
                    << seg.x2 << " " << toPsY(yCurr) << " drawline\n";
            }
        }
        if (opts.layout == WaveformHardcopy::Layout::Full) {
            ofs << "/Helvetica findfont 9 scalefont setfont\n";
            ofs << "0.3 0.3 0.3 setrgbcolor\n";
            const wxString name = panel.TraceRowDisplayName(sig);
            ofs << "10 " << toPsY(panel.RowYBase(i) - 8) << " moveto ("
                << PsEscape(name.ToUTF8().data()) << ") show\n";
        }
    }

    const auto drawVLine = [&](int x, double r, double g, double b, double w) {
        ofs << r << " " << g << " " << b << " setrgbcolor\n";
        ofs << w << " setlinewidth\n";
        ofs << x << " 0 moveto " << x << " " << client.y << " drawline\n";
    };

    drawVLine(panel.TimeToX(panel.m_currentTimestamp, tScale), 0.26, 0.52, 0.96, 2.0);
    if (panel.m_hasBaseline && panel.m_baselineTimestamp >= 0)
        drawVLine(panel.TimeToX(panel.m_baselineTimestamp, tScale), 1.0, 1.0, 1.0, 2.0);

    for (const auto& mk : panel.m_markers)
        drawVLine(panel.TimeToX(mk.timestamp, tScale), 0.0, 0.7, 0.0, 1.5);

    ofs << "showpage\n";
    ofs << "%%EOF\n";
    return true;
}

bool WriteMif(const WaveformPanel& panel, const wxString& path,
              const WaveformHardcopy::Options& opts, wxString* errOut)
{
    const wxSize client = panel.GetClientSize();
    if (client.x <= 0 || client.y <= 0) {
        if (errOut)
            *errOut = "waveform panel has no size";
        return false;
    }

    double pageWIn = 0, pageHIn = 0;
    PageSizeToMifUnits(opts.pageSize, &pageWIn, &pageHIn);
    const double sx = pageWIn / (double)client.x;
    const double sy = pageHIn / (double)client.y;
    const double s = std::min(sx, sy);

    std::ofstream ofs(path.ToUTF8().data(), std::ios::out | std::ios::trunc);
    if (!ofs) {
        if (errOut)
            *errOut = "cannot open output file";
        return false;
    }

    ofs << "<MIFFile 7.00>\n# Generated by Bear2Wave\n<Units U/in>\n";
    ofs << "<PageWidth " << pageWIn << "in>\n";
    ofs << "<PageHeight " << pageHIn << "in>\n";
    ofs << "<Page\n <PageType BodyPage>\n <PageBackground `Default'>\n>\n";
    ofs << "<TextRect\n <RectRect 0.0in 0.0in " << pageWIn << "in " << pageHIn << "in>\n>\n";

    const int viewW = std::max(1, client.x - LEFT_MARGIN - WAVE_PADDING);
    const double tScale = (double)viewW / (double)panel.m_displayTimeRange;

    auto yIn = [&](int yPx) { return (double)yPx * s; };
    auto xIn = [&](int xPx) { return (double)xPx * s; };

    std::lock_guard<std::mutex> lock(panel.m_cacheMutex);
    const int safeCount = std::min((int)panel.m_displayedSignals2.size(),
                                   (int)panel.m_cachedSegments.size());
    const int scrollPx = panel.SignalRowScrollOffsetPx();
    const int firstRow = panel.m_signalScrollRow;
    const int lastRow = std::min(safeCount, firstRow + panel.VisibleSignalRowCount());

    for (int i = firstRow; i < lastRow; ++i) {
        signal_t* sig = panel.m_displayedSignals2[i];
        if (!sig || i >= (int)panel.m_cachedSegments.size())
            continue;
        if (opts.layout == WaveformHardcopy::Layout::Full) {
            const wxString name = panel.TraceRowDisplayName(sig);
            ofs << "<ParaLine\n <Unique 1>\n <Font `'Courier'>\n <FontSize 9pt>\n"
                << " <String `" << MifEscape(name.ToUTF8().data()) << "'>\n"
                << " <RectRect " << xIn(10) << "in " << yIn(panel.RowYBase(i) - 8)
                << "in 0.1in 0.1in>\n>\n";
        }
        for (const auto& seg : panel.m_cachedSegments[i]) {
            const int y = seg.y - scrollPx;
            int yLine = y;
            if (seg.traceKind != WaveformPanel::WaveTraceKind::RealAnalog
                && seg.traceKind != WaveformPanel::WaveTraceKind::BusBits
                && seg.traceKind != WaveformPanel::WaveTraceKind::TextString
                && seg.traceKind != WaveformPanel::WaveTraceKind::TransactionEvent)
                yLine = (seg.value == '0') ? (y + 15) : (y - 15);

            ofs << "<PolyLine\n <NumPoints 2>\n";
            ofs << " <Point " << xIn(seg.x1) << "in " << yIn(yLine) << "in>\n";
            ofs << " <Point " << xIn(seg.x2) << "in " << yIn(yLine) << "in>\n";
            ofs << " <LineWidth 0.5pt>\n>\n";
        }
    }

    const auto vLine = [&](int xPx, const char* colorTag) {
        ofs << "<PolyLine\n <NumPoints 2>\n";
        ofs << " <Point " << xIn(xPx) << "in 0in>\n";
        ofs << " <Point " << xIn(xPx) << "in " << yIn(client.y) << "in>\n";
        ofs << " <LineWidth 1.0pt>\n <Color `" << colorTag << "'>\n>\n";
    };

    vLine(panel.TimeToX(panel.m_currentTimestamp, tScale), "Blue");
    if (panel.m_hasBaseline && panel.m_baselineTimestamp >= 0)
        vLine(panel.TimeToX(panel.m_baselineTimestamp, tScale), "White");

    ofs << "<MIFFile 7.00>\n";
    return true;
}

} // namespace

namespace WaveformHardcopy {

bool WriteHardcopyFile(const WaveformPanel& panel, const wxString& path, const Options& opts, wxString* errOut)
{
    if (opts.format == Format::Mif)
        return WriteMif(panel, path, opts, errOut);
    return WritePostScript(panel, path, opts, errOut);
}

bool WritePngFile(WaveformPanel& panel, const wxString& path, wxString* errOut)
{
    panel.RequestDrawCacheRebuild(true);
    panel.Refresh();
    panel.Update();

    const wxSize sz = panel.GetClientSize();
    if (sz.x <= 0 || sz.y <= 0) {
        if (errOut)
            *errOut = "waveform panel has no size";
        return false;
    }

    wxBitmap bmp(sz.x, sz.y, 24);
    wxMemoryDC memDC(bmp);
    memDC.SetBackground(wxBrush(wxColour(252, 253, 255)));
    memDC.Clear();

    wxClientDC clientDC(const_cast<WaveformPanel*>(&panel));
    if (!memDC.Blit(0, 0, sz.x, sz.y, &clientDC, 0, 0)) {
        if (errOut)
            *errOut = "failed to capture waveform window";
        return false;
    }

    wxImage img = bmp.ConvertToImage();
    if (!img.SaveFile(path, wxBITMAP_TYPE_PNG)) {
        if (errOut)
            *errOut = "failed to write PNG file";
        return false;
    }
    return true;
}

} // namespace WaveformHardcopy
