#include "ui/MainFrameDiagnostics.hpp"
#include "ui/MainFrame.hpp"

#include "core/trace_backend.h"
#include "core/waveform_perf.h"
#include "panels/WaveformPanel.hpp"
#include "trace_loader.h"

#include <wx/datetime.h>
#include <wx/filedlg.h>
#include <wx/filename.h>
#include <wx/stdpaths.h>

#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>

#if defined(_WIN32)
#include <stdlib.h>
extern "C" char** _environ;
#endif

namespace {

static void AppendEnvBlock(std::ostringstream& out, const char* prefix)
{
    const size_t plen = strlen(prefix);
    out << "\n--- Environment (" << prefix << "*) ---\n";
#if defined(_WIN32)
    for (char** p = _environ; p && *p; ++p) {
        if (strncmp(*p, prefix, plen) == 0)
            out << *p << '\n';
    }
#else
    extern char** environ;
    for (char** p = environ; p && *p; ++p) {
        if (strncmp(*p, prefix, plen) == 0)
            out << *p << '\n';
    }
#endif
}

static const char* BackendName(TraceBackend::Kind k)
{
    switch (k) {
    case TraceBackend::Kind::FstLazy: return "FST_LAZY";
    case TraceBackend::Kind::VztLazy: return "VZT_LAZY";
    case TraceBackend::Kind::Lxt2Lazy: return "LXT2_LAZY";
    case TraceBackend::Kind::GhwLazy: return "GHW_LAZY";
    case TraceBackend::Kind::VcdLazy: return "VCD_LAZY";
    case TraceBackend::Kind::VcdFull: return "VCD_FULL";
    default: return "NONE";
    }
}

} // namespace

namespace MainFrameDiagnostics {

bool ExportToFile(MyFrame& frame, const wxString& path)
{
    std::ostringstream out;
    out << "Bear2Wave diagnostic bundle\n";
    out << "Generated: " << wxDateTime::Now().FormatISOCombined(' ').ToUTF8() << "\n";

    wxString version = "unknown";
    {
        const wxArrayString candidates = {
            wxFileName::DirName(wxGetCwd()).GetPath() + wxFileName::GetPathSeparator() + "VERSION.txt",
            wxStandardPaths::Get().GetExecutablePath() + wxFileName::GetPathSeparator() + "VERSION.txt",
        };
        for (const wxString& verPath : candidates) {
            if (!wxFile::Exists(verPath))
                continue;
            wxFile f(verPath);
            if (!f.IsOpened())
                continue;
            wxString v;
            if (f.ReadAll(&v)) {
                version = v.Trim();
                break;
            }
        }
    }
    out << "Version: " << version.ToUTF8() << "\n";
    out << "Executable: " << wxStandardPaths::Get().GetExecutablePath().ToUTF8() << "\n";

    out << "\n--- Trace ---\n";
    out << "Path: " << frame.m_tracePathLabel.ToUTF8() << "\n";

    WaveformPanel* panel = frame.m_wavePanel;
    vcd_t* vcd = panel ? panel->m_vcdData : nullptr;
    if (!vcd) {
        out << "Backend: (no trace loaded)\n";
    } else {
        const TraceBackend::Kind bk = TraceBackend::FromVcd(vcd);
        out << "Backend: " << BackendName(bk) << " lazy_io=" << (TraceBackend::UsesLazyIO(bk) ? "yes" : "no") << "\n";
        if (vcd->timescale.unit[0])
            out << "Timescale: " << vcd->timescale.scale << " " << vcd->timescale.unit << "\n";
        if (vcd->version[0])
            out << "Version string: " << vcd->version << "\n";
        if (panel) {
            out << "Signals in trace: " << panel->m_allSignals.size() << "\n";
            out << "Displayed rows: " << panel->m_displayedSignals2.size() << "\n";
            out << "Scroll row: " << panel->m_signalScrollRow << " / max " << panel->MaxSignalScrollRow() << "\n";
            out << "Time offset: " << panel->m_timeOffset << " range: " << panel->m_displayTimeRange
                << " maxTs: " << panel->m_maxTimestamp << "\n";
        }
    }

    out << "\n--- Key env defaults (effective) ---\n";
    out << "BEAR2WAVE_LOAD_MARGIN=" << WaveformPerf::TraceLoadMarginRatio() << "\n";
    out << "BEAR2WAVE_MAX_SEGMENTS=" << WaveformPerf::MaxDrawSegments() << "\n";
    out << "BEAR2WAVE_IDX_CACHE=" << WaveformPerf::IdxCacheEnabled() << "\n";
    out << "BEAR2WAVE_VCD_LAZY_MB=" << WaveformPerf::VcdLazyThresholdMb() << "\n";

    AppendEnvBlock(out, "BEAR2WAVE_");

    out << "\n--- Log file hint ---\n";
    out << "Set BEAR2WAVE_LOG_FILE=1 and BEAR2WAVE_LOG_LEVEL=debug\n";
#ifdef _WIN32
    out << "Default log: %APPDATA%\\Bear2Wave\\logs\\bear2wave.log\n";
#else
    out << "Default log: bear2wave.log (cwd)\n";
#endif

    std::ofstream f(path.ToStdString(), std::ios::trunc);
    if (!f)
        return false;
    const std::string blob = out.str();
    f.write(blob.data(), static_cast<std::streamsize>(blob.size()));
    return f.good();
}

void PromptExport(MyFrame& frame)
{
    wxString def = "bear2wave_diagnostics.txt";
    if (!frame.m_tracePathLabel.empty()) {
        wxFileName fn(frame.m_tracePathLabel);
        def = fn.GetName() + "_diagnostics.txt";
    }
    wxFileDialog dlg(
        &frame,
        "Export diagnostic bundle",
        wxEmptyString,
        def,
        "Text files (*.txt)|*.txt",
        wxFD_SAVE | wxFD_OVERWRITE_PROMPT);
    if (dlg.ShowModal() != wxID_OK)
        return;
    if (ExportToFile(frame, dlg.GetPath())) {
        wxMessageBox("Diagnostics written to:\n" + dlg.GetPath(), "Export diagnostics", wxOK | wxICON_INFORMATION);
    } else {
        wxMessageBox("Failed to write diagnostics file.", "Export diagnostics", wxOK | wxICON_ERROR);
    }
}

} // namespace MainFrameDiagnostics

void MyFrame::OnExportDiagnostics(wxCommandEvent&)
{
    MainFrameDiagnostics::PromptExport(*this);
}
