#pragma once



#include "core/trace_external_convert.h"
#include "core/trace_filter_config.h"

#include <wx/button.h>

#include <wx/checkbox.h>

#include <wx/dialog.h>

#include <wx/filedlg.h>

#include <wx/msgdlg.h>

#include <wx/sizer.h>

#include <wx/stattext.h>

#include <wx/textctrl.h>

#include <vector>



/** Edit -> External Tool Paths (E4-5). */

class ExternalToolsSettingsDialog : public wxDialog

{

public:

    explicit ExternalToolsSettingsDialog(wxWindow* parent)

        : wxDialog(parent, wxID_ANY, wxT("External Tool Paths"), wxDefaultPosition, wxSize(680, 520),

            wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)

    {

        m_cfg = trace_external_load_config();



        auto* root = new wxBoxSizer(wxVERTICAL);

        root->Add(new wxStaticText(this, wxID_ANY,

            wxT("Leave blank to auto-detect from PATH and EDA install dirs (VCS_HOME, VERDI_HOME, MODELTECH, CDS_INST_DIR, SIMARAMA_BASE, …).\n"
                "GTKWave readers (FST/LXT/VZT/GHW) are built-in — VPD/WLF/FSDB/SHM/AET need external tools here.")),

            0, wxALL, 10);



        m_vpdPath = addPathRow(root, wxT("vpd2vcd:"), m_cfg.vpd2vcd_path, TraceExternalKind::Vpd);

        m_wlfPath = addPathRow(root, wxT("wlf2vcd:"), m_cfg.wlf2vcd_path, TraceExternalKind::Wlf);

        m_fsdbPath = addPathRow(root, wxT("fsdb2vcd:"), m_cfg.fsdb2vcd_path, TraceExternalKind::Fsdb);

        m_shmPath = addPathRow(root, wxT("shm2vcd:"), m_cfg.shm2vcd_path, TraceExternalKind::Shm);

        m_aetPath = addPathRow(root, wxT("aet2vcd:"), m_cfg.aet2vcd_path, TraceExternalKind::Aet);

        root->Add(new wxStaticText(this, wxID_ANY,
            wxT("Filter processes (Translate / Transaction — FP-0/FP-1):")),
            0, wxLEFT | wxRIGHT | wxTOP, 10);

        m_translateProcPath = addPlainPathRow(root, wxT("translate_proc:"), m_cfg.translate_proc_path);
        m_transactionProcPath = addPlainPathRow(root, wxT("transaction_proc:"), m_cfg.transaction_proc_path);

        auto* timeoutRow = new wxBoxSizer(wxHORIZONTAL);
        timeoutRow->Add(new wxStaticText(this, wxID_ANY, wxT("filter_timeout_ms:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);
        m_filterTimeout = new wxTextCtrl(this, wxID_ANY, wxString::Format("%d", m_cfg.filter_process_timeout_ms));
        timeoutRow->Add(m_filterTimeout, 0, wxEXPAND);
        root->Add(timeoutRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);

        auto* detectRow = new wxBoxSizer(wxHORIZONTAL);

        auto* detectBtn = new wxButton(this, wxID_ANY, wxT("Auto-detect all"));

        detectBtn->Bind(wxEVT_BUTTON, [this](wxCommandEvent&) { autoDetectAll(); });

        detectRow->Add(detectBtn, 0, wxRIGHT, 8);

        detectRow->Add(new wxStaticText(this, wxID_ANY,

            wxT("Scans PATH, exe/tools/, VCS_HOME, VERDI_HOME, CDS_INST_DIR, SIMARAMA_BASE, BEAR2WAVE_EXT_SEARCH_DIRS")),

            1, wxALIGN_CENTER_VERTICAL);

        root->Add(detectRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);



        auto* cacheRow = new wxBoxSizer(wxHORIZONTAL);

        m_cacheCheck = new wxCheckBox(this, wxID_ANY, wxT("Enable conversion cache"));

        m_cacheCheck->SetValue(m_cfg.cache_enabled != 0);

        cacheRow->Add(m_cacheCheck, 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 12);

        cacheRow->Add(new wxStaticText(this, wxID_ANY, wxT("Cache dir:")), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 6);

        m_cacheDir = new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(m_cfg.cache_dir.c_str()));

        cacheRow->Add(m_cacheDir, 1, wxEXPAND);

        root->Add(cacheRow, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 10);



        auto* btns = CreateStdDialogButtonSizer(wxOK | wxCANCEL);

        root->Add(btns, 0, wxEXPAND | wxALL, 10);

        SetSizerAndFit(root);

        SetMinSize(GetSize());

        CentreOnParent();



        autoDetectAll(false);

        Bind(wxEVT_BUTTON, &ExternalToolsSettingsDialog::OnOk, this, wxID_OK);

    }



private:

    wxTextCtrl* addPathRow(wxBoxSizer* root, const wxString& label, const std::string& initial, TraceExternalKind kind)

    {

        auto* row = new wxBoxSizer(wxHORIZONTAL);

        row->Add(new wxStaticText(this, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);



        auto* path = new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(initial.c_str()));

        row->Add(path, 1, wxEXPAND | wxRIGHT, 6);



        auto* browse = new wxButton(this, wxID_ANY, wxT("Browse..."));

        browse->Bind(wxEVT_BUTTON, [this, path](wxCommandEvent&) {

            wxFileDialog dlg(this, wxT("Select converter executable"), wxEmptyString, wxEmptyString,

                wxT("Programs (*.exe;*.cmd;*.bat)|*.exe;*.cmd;*.bat|All files (*.*)|*.*"),

                wxFD_OPEN | wxFD_FILE_MUST_EXIST);

            if (dlg.ShowModal() == wxID_OK)

                path->SetValue(dlg.GetPath());

        });

        row->Add(browse, 0, wxALIGN_CENTER_VERTICAL);



        root->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);



        auto* hint = new wxStaticText(this, wxID_ANY, wxEmptyString);

        hint->SetForegroundColour(wxColour(90, 90, 90));

        root->Add(hint, 0, wxLEFT | wxRIGHT | wxBOTTOM, 10);

        m_hints.push_back({kind, hint, path});

        return path;

    }

    wxTextCtrl* addPlainPathRow(wxBoxSizer* root, const wxString& label, const std::string& initial)
    {
        auto* row = new wxBoxSizer(wxHORIZONTAL);
        row->Add(new wxStaticText(this, wxID_ANY, label), 0, wxALIGN_CENTER_VERTICAL | wxRIGHT, 8);

        auto* path = new wxTextCtrl(this, wxID_ANY, wxString::FromUTF8(initial.c_str()));
        row->Add(path, 1, wxEXPAND | wxRIGHT, 6);

        auto* browse = new wxButton(this, wxID_ANY, wxT("Browse..."));
        browse->Bind(wxEVT_BUTTON, [this, path](wxCommandEvent&) {
            wxFileDialog dlg(this, wxT("Select filter process executable"), wxEmptyString, wxEmptyString,
                wxT("Programs (*.exe;*.cmd;*.bat)|*.exe;*.cmd;*.bat|All files (*.*)|*.*"),
                wxFD_OPEN | wxFD_FILE_MUST_EXIST);
            if (dlg.ShowModal() == wxID_OK)
                path->SetValue(dlg.GetPath());
        });
        row->Add(browse, 0, wxALIGN_CENTER_VERTICAL);

        root->Add(row, 0, wxEXPAND | wxLEFT | wxRIGHT | wxBOTTOM, 6);
        return path;
    }



    void autoDetectAll(bool fillEmptyOnly = true)

    {

        TraceExternalConfig probe = m_cfg;

        trace_external_apply_probed_paths(&probe);

        TraceFilterProcessConfig fprobe = trace_filter_from_external(probe);
        trace_filter_apply_probed_paths(&fprobe);

        for (const auto& row : m_hints) {

            std::string detected;

            switch (row.kind) {

            case TraceExternalKind::Vpd:

                detected = probe.vpd2vcd_path;

                break;

            case TraceExternalKind::Wlf:

                detected = probe.wlf2vcd_path;

                break;

            case TraceExternalKind::Fsdb:

                detected = probe.fsdb2vcd_path;

                break;

            case TraceExternalKind::Shm:

                detected = probe.shm2vcd_path;

                break;

            case TraceExternalKind::Aet:

                detected = probe.aet2vcd_path;

                break;

            default:

                break;

            }



            if (detected.empty()) {

                row.hint->SetLabel(wxT("Auto-detect: (not found)"));

            } else {

                row.hint->SetLabel(wxT("Auto-detect: ") + wxString::FromUTF8(detected.c_str()));

                if (!fillEmptyOnly || row.path->GetValue().IsEmpty())

                    row.path->SetValue(wxString::FromUTF8(detected.c_str()));

            }

        }

        if (!fprobe.translate_proc_path.empty()) {
            if (!fillEmptyOnly || m_translateProcPath->GetValue().IsEmpty())
                m_translateProcPath->SetValue(wxString::FromUTF8(fprobe.translate_proc_path.c_str()));
        }
        if (!fprobe.transaction_proc_path.empty()) {
            if (!fillEmptyOnly || m_transactionProcPath->GetValue().IsEmpty())
                m_transactionProcPath->SetValue(wxString::FromUTF8(fprobe.transaction_proc_path.c_str()));
        }

    }



    void OnOk(wxCommandEvent&)

    {

        m_cfg.vpd2vcd_path = m_vpdPath->GetValue().utf8_string();

        m_cfg.wlf2vcd_path = m_wlfPath->GetValue().utf8_string();

        m_cfg.fsdb2vcd_path = m_fsdbPath->GetValue().utf8_string();

        m_cfg.shm2vcd_path = m_shmPath->GetValue().utf8_string();

        m_cfg.aet2vcd_path = m_aetPath->GetValue().utf8_string();

        m_cfg.translate_proc_path = m_translateProcPath->GetValue().utf8_string();
        m_cfg.transaction_proc_path = m_transactionProcPath->GetValue().utf8_string();
        long timeout_ms = m_cfg.filter_process_timeout_ms;
        if (m_filterTimeout->GetValue().ToLong(&timeout_ms) && timeout_ms > 0)
            m_cfg.filter_process_timeout_ms = (int)timeout_ms;

        m_cfg.cache_enabled = m_cacheCheck->IsChecked() ? 1 : 0;

        m_cfg.cache_dir = m_cacheDir->GetValue().utf8_string();

        if (trace_external_save_config(m_cfg) != 0) {

            wxMessageBox(wxT("Failed to save external tool settings."), wxT("Error"), wxOK | wxICON_ERROR, this);

            return;

        }

        EndModal(wxID_OK);

    }



    struct HintRow {

        TraceExternalKind kind;

        wxStaticText* hint;

        wxTextCtrl* path;

    };



    TraceExternalConfig m_cfg;

    wxTextCtrl* m_vpdPath = nullptr;

    wxTextCtrl* m_wlfPath = nullptr;

    wxTextCtrl* m_fsdbPath = nullptr;

    wxTextCtrl* m_shmPath = nullptr;

    wxTextCtrl* m_aetPath = nullptr;

    wxTextCtrl* m_translateProcPath = nullptr;

    wxTextCtrl* m_transactionProcPath = nullptr;

    wxTextCtrl* m_filterTimeout = nullptr;

    wxCheckBox* m_cacheCheck = nullptr;

    wxTextCtrl* m_cacheDir = nullptr;

    std::vector<HintRow> m_hints;

};


