#pragma once

#include <wx/wx.h>

class WaveformPanel;

namespace WaveformHardcopy {

enum class Format {
    PostScript,
    Mif,
};

enum class PageSize {
    Letter,
    A4,
    Legal,
};

enum class Layout {
    Full,
    Minimal,
};

struct Options {
    Format format = Format::PostScript;
    PageSize pageSize = PageSize::Letter;
    Layout layout = Layout::Full;
};

bool WriteHardcopyFile(const WaveformPanel& panel, const wxString& path, const Options& opts,
                       wxString* errOut = nullptr);
bool WritePngFile(WaveformPanel& panel, const wxString& path, wxString* errOut = nullptr);

} // namespace WaveformHardcopy
