#pragma once

#include <wx/string.h>

/** Shared wxFileDialog filters for trace / waveform open dialogs. */
namespace Bear2WaveTraceUi {

inline wxString OpenTraceDialogFilter()
{
    return wxT("All traces (*.vcd;*.evcd;*.fst;*.fzt;*.vzt;*.lxt;*.lxt2;*.ghw;*.csv;*.vpd;*.wlf;*.fsdb;*.shm;*.trn;*.aet;*.aet2;*.ae2)|")
           wxT("*.vcd;*.evcd;*.fst;*.fzt;*.vzt;*.lxt;*.lxt2;*.ghw;*.csv;*.vpd;*.wlf;*.fsdb;*.shm;*.trn;*.aet;*.aet2;*.ae2|")
           wxT("VCD (*.vcd;*.evcd)|*.vcd;*.evcd|")
           wxT("FST (*.fst;*.fzt)|*.fst;*.fzt|")
           wxT("VZT (*.vzt)|*.vzt|")
           wxT("LXT (*.lxt;*.lxt2)|*.lxt;*.lxt2|")
           wxT("GHW (*.ghw)|*.ghw|")
           wxT("CSV (*.csv)|*.csv|")
           wxT("VPD (*.vpd)|*.vpd|")
           wxT("WLF (*.wlf)|*.wlf|")
           wxT("FSDB (*.fsdb)|*.fsdb|")
           wxT("SHM/TRN (*.shm;*.trn)|*.shm;*.trn|")
           wxT("AET/AET2 (*.aet;*.aet2;*.ae2)|*.aet;*.aet2;*.ae2|")
           wxT("All files (*.*)|*.*");
}

inline wxString ProjectStartDialogFilter()
{
    return wxT("Waveform / Project (*.vcd;*.evcd;*.fst;*.fzt;*.vzt;*.lxt;*.lxt2;*.ghw;*.csv;*.vpd;*.wlf;*.fsdb;*.shm;*.trn;*.aet;*.aet2;*.ae2;*.bwv;*.b2w)|")
           wxT("*.vcd;*.evcd;*.fst;*.fzt;*.vzt;*.lxt;*.lxt2;*.ghw;*.csv;*.vpd;*.wlf;*.fsdb;*.shm;*.trn;*.aet;*.aet2;*.ae2;*.bwv;*.b2w|")
           wxT("All files (*.*)|*.*");
}

inline bool IsTraceExtension(const wxString& extLower)
{
    return extLower == wxT("vcd") || extLower == wxT("evcd") || extLower == wxT("fst") || extLower == wxT("fzt")
        || extLower == wxT("vzt") || extLower == wxT("lxt") || extLower == wxT("lxt2")
        || extLower == wxT("ghw") || extLower == wxT("vpd") || extLower == wxT("wlf") || extLower == wxT("fsdb")
        || extLower == wxT("shm") || extLower == wxT("trn")
        || extLower == wxT("aet") || extLower == wxT("aet2") || extLower == wxT("ae2");
}

inline bool IsWaveformOpenExtension(const wxString& extLower)
{
    return IsTraceExtension(extLower) || extLower == wxT("csv");
}

} // namespace Bear2WaveTraceUi
