#pragma once

#include "core/TraceDocument.h"
#include "core/sst_filter.h"
#include "ui/ModuleSignalListView.hpp"
#include "ui/ModuleTreeLazyCtrl.h"

#include <wx/listctrl.h>
#include <wx/treelist.h>

#include <functional>
#include <string>
#include <unordered_set>
#include <vector>

/** Module hierarchy + signal list (E5-1); lazy tree via ModuleTreeLazyCtrl (P4-6). */
class SignalModuleTree {
public:
    explicit SignalModuleTree(TraceDocument& document);

    /** Lazy module tree + wxListCtrl signal list (MainFrame default). */
    void Attach(wxTreeListCtrl* tree, ModuleTreeLazyCtrl* lazy, wxListCtrl* legacyList);

    /** Eager tree + virtual list (alternate / tests). */
    void Attach(wxTreeListCtrl* tree, ModuleSignalListView* list);

    void RebuildFromVcd(vcd_t* vcd);
    void RebuildFromExistingIndex();

    void OnTreeSelect(wxTreeListEvent& event);
    void OnModuleRightClick(wxContextMenuEvent& event, wxMenu* moduleMenu);

    wxTreeListItem RightClickItem() const { return m_rightClickModuleItem; }

    void FillSignalListForModulePath(const std::string& modulePath);
    void FillSignalListAllFromModules();

    void GetAllSignalsInModuleTree(const std::string& rootPath, std::vector<signal_t*>& outSignals) const;

    void OpenScopeForSignal(signal_t* sig, const std::function<void(const wxString&)>& setStatus);

    void ApplyOpenPaths(const std::vector<std::string>& paths);
    void CollectExpandedPaths(std::vector<std::string>& out) const;

    /** Apply static SST filter (GTKWave-style); empty text restores full tree. */
    void SetSstFilter(const std::string& filterText);
    const std::string& SstFilterText() const { return m_sstFilterText; }
    const SstFilterSpec& SstFilter() const { return m_sstFilter; }
    size_t CountMatchingSignals() const;

    wxTreeListItem ModulesRoot() const;
    wxTreeListItem EnsureModulePath(const std::string& modulePathIn);

    std::string GetFullPath(wxTreeListItem item) const;

private:
    static std::string ModulePathFromSignal(const signal_t* sig);
    static std::string TrimModulePathForTree(std::string p);

    wxTreeListItem FindVcdModulesRoot() const;
    wxTreeListItem FindTreeItemByModulePath(const std::string& modulePathIn) const;
    void ExpandTreeToItem(wxTreeListItem item) const;
    void PopulateTreeFromIndex();

    void FillLegacyList(const std::vector<signal_t*>& rows, bool showFullName, size_t totalHint);

    void RebuildModuleTreeFromIndex();
    void FilterSignalRows(std::vector<signal_t*>& rows) const;
    std::vector<std::string> ModulePathsVisibleUnderFilter() const;
    static void AddModulePathAndAncestors(std::unordered_set<std::string>& out, const std::string& modulePath);

    TraceDocument& m_document;
    std::string m_sstFilterText;
    SstFilterSpec m_sstFilter;
    wxTreeListCtrl* m_tree = nullptr;
    ModuleTreeLazyCtrl* m_lazy = nullptr;
    wxListCtrl* m_legacyList = nullptr;
    ModuleSignalListView* m_list = nullptr;
    wxTreeListItem m_rightClickModuleItem;
};
