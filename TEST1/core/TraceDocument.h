#pragma once

#include "csv.h"
#include "vcd.h"

#include <wx/string.h>

#include <map>
#include <string>
#include <vector>

class WaveformPanel;

/** Trace path, vcd_t lifecycle, CSV-owned signals, module index (E5-1). */
class TraceDocument {
public:
    ~TraceDocument();

    void Clear();

    /** Close trace + CSV heap + module index + path. */
    void ResetForNewFile();

    void SetTracePath(const wxString& path) { m_tracePath = path; }
    const wxString& TracePath() const { return m_tracePath; }

    const wxString& LastOpenError() const { return m_lastOpenError; }

    vcd_t* Vcd() const { return m_vcd; }

    const std::map<std::string, std::vector<signal_t*>>& ModuleIndex() const { return m_moduleToSignals; }
    std::map<std::string, std::vector<signal_t*>>& ModuleIndex() { return m_moduleToSignals; }

    const std::vector<signal_t*>& CsvDisplayedSignals() const { return m_displayedSignals; }

    /** Load VCD/FST/VZT/LXT/GHW via trace_loader; owns returned vcd_t. */
    bool OpenTrace(const wxString& path, wxString& errOut, wxString* warnOut = nullptr);

    void CloseTrace();

    void ReleaseCsvHeap();
    void BuildModuleIndexFromVcd(vcd_t* vcd);
    void ClearModuleIndex() { m_moduleToSignals.clear(); }

    void LoadFromCsv(CSVParser& parser, WaveformPanel& panel);

    static std::string NormalizeModulePathKey(const char* mp);

private:
    wxString m_tracePath;
    wxString m_lastOpenError;
    vcd_t* m_vcd = nullptr;
    std::map<std::string, std::vector<signal_t*>> m_moduleToSignals;
    std::vector<signal_t*> m_csvHeapSignals;
    std::vector<signal_t*> m_displayedSignals;
};
