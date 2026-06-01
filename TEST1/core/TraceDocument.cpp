#include "core/TraceDocument.h"
#include "csv_loader.h"
#include "core/trace_vc.h"

#include "panels/WaveformPanel.hpp"
#include "trace_loader.h"

#include <wx/filename.h>

#include <algorithm>
#include <cstring>

TraceDocument::~TraceDocument()
{
    ResetForNewFile();
}

void TraceDocument::Clear()
{
    ResetForNewFile();
}

void TraceDocument::ResetForNewFile()
{
    CloseTrace();
    ReleaseCsvHeap();
    m_tracePath.clear();
    m_moduleToSignals.clear();
    m_displayedSignals.clear();
    m_lastOpenError.clear();
}

void TraceDocument::CloseTrace()
{
    if (m_vcd) {
        vcd_free(m_vcd);
        m_vcd = nullptr;
    }
}

bool TraceDocument::OpenTrace(const wxString& path, wxString& errOut, wxString* warnOut)
{
    errOut.clear();
    m_lastOpenError.clear();
    if (warnOut)
        warnOut->clear();
    if (path.IsEmpty()) {
        errOut = "Empty trace path";
        m_lastOpenError = errOut;
        return false;
    }

    CloseTrace();

    char err[512] = {};
#if defined(__WXMSW__)
    wxString pathOpen(path);
    wxFileName fn(path);
    const wxString shortPath = fn.GetShortPath();
    if (!shortPath.empty())
        pathOpen = shortPath;
    const wxScopedCharBuffer mb(pathOpen.mb_str(wxConvLocal));
    const char* narrow = mb.data() ? mb.data() : "";
    vcd_t* data = trace_load_from_path(narrow, err, sizeof(err));
#else
    wxCharBuffer utf8 = path.ToUTF8();
    vcd_t* data = trace_load_from_path(utf8.data(), err, sizeof(err));
#endif

    if (!data) {
        errOut = wxString::FromUTF8(err);
        if (errOut.empty())
            errOut = "Unknown trace load error";
        m_lastOpenError = errOut;
        return false;
    }

    if (err[0] != '\0' && strncmp(err, "WARN:", 5) == 0 && warnOut)
        *warnOut = wxString::FromUTF8(err + 5);

    m_vcd = data;
    m_tracePath = path;
    BuildModuleIndexFromVcd(m_vcd);
    return true;
}

void TraceDocument::ReleaseCsvHeap()
{
    for (signal_t* s : m_csvHeapSignals) {
        if (!s)
            continue;
        signal_free_value_changes(s);
        delete s;
    }
    m_csvHeapSignals.clear();
    m_displayedSignals.clear();
}

std::string TraceDocument::NormalizeModulePathKey(const char* mp)
{
    if (!mp || !mp[0])
        return "$root";
    return std::string(mp);
}

void TraceDocument::BuildModuleIndexFromVcd(vcd_t* vcd)
{
    m_moduleToSignals.clear();
    if (!vcd)
        return;
    for (signal_node_t* node = vcd->signals_head; node; node = node->next) {
        signal_t* sig = &node->signal;
        const std::string modulePath = NormalizeModulePathKey(sig->module_path);
        m_moduleToSignals[modulePath].push_back(sig);
    }
}

void TraceDocument::LoadFromCsv(CSVParser& parser, WaveformPanel& panel)
{
    CloseTrace();
    ReleaseCsvHeap();

    char err[512] = {};
    vcd_t* vcd = csv_vcd_from_parser(parser, err, sizeof(err));
    if (!vcd)
        return;

    m_vcd = vcd;
    BuildModuleIndexFromVcd(vcd);
    panel.SetVcdData(vcd);
}
