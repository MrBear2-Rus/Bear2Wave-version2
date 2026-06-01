#pragma once

#include <wx/wx.h>
#include <wx/listbox.h>
#include <wx/splitter.h>
#include <wx/stdpaths.h>
#include <wx/filename.h>
#include <wx/file.h>
#include <wx/html/htmlwin.h>

#include "core/HelpCatalog.h"
#include "core/HelpHtmlSanitize.h"
#include "core/MarkdownToHtml.h"

#include <vector>

class HelpDialog : public wxDialog
{
public:
    enum class Tab {
        Contents,
        Shortcuts,
        Environment,
    };

    static void Show(wxWindow* parent, Tab initial = Tab::Contents)
    {
        HelpDialog dlg(parent, initial);
        dlg.ShowModal();
    }

private:
    struct Section {
        wxString title;
        wxString html;
    };

    wxListBox* m_sectionList = nullptr;
    wxHtmlWindow* m_html = nullptr;
    std::vector<Section> m_sections;
    int m_initialSelect = 0;
    bool m_ignoreListEvents = false;

    HelpDialog(wxWindow* parent, Tab initial)
        : wxDialog(parent, wxID_ANY, "Bear2Wave Help", wxDefaultPosition, wxSize(980, 740),
              wxDEFAULT_DIALOG_STYLE | wxRESIZE_BORDER)
    {
        auto* root = new wxBoxSizer(wxVERTICAL);

        auto* banner = new wxPanel(this, wxID_ANY);
        banner->SetBackgroundColour(wxColour(30, 58, 95));
        auto* bannerSizer = new wxBoxSizer(wxVERTICAL);
        auto* title = new wxStaticText(banner, wxID_ANY, "Bear2Wave Help");
        title->SetForegroundColour(*wxWHITE);
        title->SetFont(wxFont(12, wxFONTFAMILY_DEFAULT, wxFONTSTYLE_NORMAL, wxFONTWEIGHT_BOLD));
        auto* subtitle = new wxStaticText(banner, wxID_ANY, "Select a topic on the left");
        subtitle->SetForegroundColour(wxColour(148, 163, 184));
        bannerSizer->Add(title, 0, wxALL, 8);
        bannerSizer->Add(subtitle, 0, wxLEFT | wxRIGHT | wxBOTTOM, 8);
        banner->SetSizer(bannerSizer);
        root->Add(banner, 0, wxEXPAND);

        auto* splitter = new wxSplitterWindow(this, wxID_ANY, wxDefaultPosition, wxDefaultSize,
            wxSP_LIVE_UPDATE | wxSP_3D);
        m_sectionList = new wxListBox(splitter, wxID_ANY, wxDefaultPosition, wxSize(200, -1));
        m_html = new wxHtmlWindow(splitter, wxID_ANY, wxDefaultPosition, wxDefaultSize,
            wxHW_SCROLLBAR_AUTO);
        m_html->SetBorders(12);

        splitter->SplitVertically(m_sectionList, m_html, 220);
        splitter->SetMinimumPaneSize(120);
        root->Add(splitter, 1, wxEXPAND | wxALL, 8);

        auto* closeRow = new wxBoxSizer(wxHORIZONTAL);
        closeRow->AddStretchSpacer();
        closeRow->Add(new wxButton(this, wxID_CLOSE, "Close"), 0, wxALL, 4);
        root->Add(closeRow, 0, wxEXPAND | wxRIGHT | wxBOTTOM, 8);

        SetSizer(root);
        CentreOnParent();

        m_sectionList->Bind(wxEVT_LISTBOX, [this](wxCommandEvent& e) {
            if (m_ignoreListEvents)
                return;
            ShowSection(e.GetSelection());
        });
        Bind(wxEVT_BUTTON, [this](wxCommandEvent& e) {
            if (e.GetId() == wxID_CLOSE)
                EndModal(wxID_CLOSE);
        }, wxID_CLOSE);

        LoadAllSections(initial);

        if (!m_sections.empty()) {
            CallAfter([this]() {
                if (!m_sectionList || m_sections.empty())
                    return;
                m_ignoreListEvents = true;
                const int idx = wxMin(m_initialSelect, (int)m_sections.size() - 1);
                if (m_sectionList->GetCount() > 0)
                    m_sectionList->SetSelection(idx);
                m_ignoreListEvents = false;
                ShowSection(idx);
            });
        }
    }

    void ShowSection(int index)
    {
        if (index < 0 || index >= (int)m_sections.size() || !m_html)
            return;
        const wxString page = HelpHtmlSanitize::Prepare(m_sections[(size_t)index].html);
        if (page.empty()) {
            m_html->SetPage("<html><body><p>(empty help page)</p></body></html>");
            return;
        }
        m_html->SetPage(page);
    }

    static wxString FindDirectoryNamed(const wxString& leaf)
    {
        wxFileName fn(wxStandardPaths::Get().GetExecutablePath());
        for (int i = 0; i < 10; ++i) {
            const wxString dir = fn.GetPath() + wxFileName::GetPathSeparator() + leaf;
            if (wxFileName::DirExists(dir))
                return dir;
            if (!fn.DirExists())
                break;
            fn.RemoveLastDir();
        }
        return wxEmptyString;
    }

    static wxString ReadTextFile(const wxString& path)
    {
        wxFile f;
        if (!f.Open(path, wxFile::read))
            return wxEmptyString;
        wxString data;
        f.ReadAll(&data);
        return data;
    }

    static wxString LoadSectionHtml(const wxString& helpDir, const wxString& docsDir,
        const HelpSectionEntry& entry)
    {
        if (!helpDir.empty() && entry.htmlFile) {
            const wxString htmlPath = helpDir + wxFileName::GetPathSeparator()
                + wxString::FromUTF8(entry.htmlFile);
            if (wxFileName::FileExists(htmlPath)) {
                wxString html = ReadTextFile(htmlPath);
                if (!html.empty())
                    return html;
            }
        }

        if (entry.markdownFile && !docsDir.empty()) {
            const wxString mdPath = docsDir + wxFileName::GetPathSeparator()
                + wxString::FromUTF8(entry.markdownFile);
            const wxString md = ReadTextFile(mdPath);
            if (!md.empty())
                return MarkdownToHtml::Convert(md);
        }
        return wxEmptyString;
    }

    void PopulateSectionList()
    {
        if (!m_sectionList)
            return;
        m_ignoreListEvents = true;
        m_sectionList->Clear();
        for (const Section& s : m_sections)
            m_sectionList->Append(s.title);
        m_ignoreListEvents = false;
    }

    void LoadAllSections(Tab initial)
    {
        m_sections.clear();
        m_initialSelect = 0;

        const wxString docsDir = FindDirectoryNamed("docs");
        const wxString docsHelpDir = docsDir.empty()
            ? wxString()
            : docsDir + wxFileName::GetPathSeparator() + "help";
        const wxString effectiveHelp = wxFileName::DirExists(docsHelpDir)
            ? docsHelpDir
            : FindDirectoryNamed("help");

        for (const HelpSectionEntry& entry : DefaultHelpCatalog()) {
            wxString html = LoadSectionHtml(effectiveHelp, docsDir, entry);
            if (html.empty() && entry.markdownFile == nullptr)
                continue;
            if (html.empty())
                html = MarkdownToHtml::Convert(MissingPageMarkdown(entry.title));

            Section sec;
            sec.title = wxString::FromUTF8(entry.title);
            sec.html = std::move(html);
            m_sections.push_back(std::move(sec));
        }

        if (m_sections.empty()) {
            Section sec;
            sec.title = "Overview";
            sec.html = MarkdownToHtml::Convert(MissingPageMarkdown("Help"));
            m_sections.push_back(std::move(sec));
        }

        PopulateSectionList();

        if (initial == Tab::Shortcuts) {
            for (size_t i = 0; i < m_sections.size(); ++i) {
                if (m_sections[i].title.Contains("快捷键")) {
                    m_initialSelect = (int)i;
                    break;
                }
            }
        } else if (initial == Tab::Environment) {
            for (size_t i = 0; i < m_sections.size(); ++i) {
                if (m_sections[i].title.Contains("环境变量")) {
                    m_initialSelect = (int)i;
                    break;
                }
            }
        }
    }

    static wxString MissingPageMarkdown(const char* topic)
    {
        return wxString::Format(
            wxString::FromUTF8(
                "## %s\n\n"
                "HTML help file not found.\n\n"
                "Expected: `docs/help/*.html`\n\n"
                "Run from source tree or install the release package with `docs/help/`."),
            wxString::FromUTF8(topic));
    }
};
