#pragma once

#include "core/module_tree_lazy.h"

#include <wx/treelist.h>

#include <functional>
#include <string>
#include <cstdint>
#include <map>
#include <vector>

/** Lazy module tree: populate direct children on expand (P4-6). */
class ModuleTreeLazyCtrl {
public:
    static constexpr const char* kPlaceholderLabel = "\xE2\x80\xA6"; /* UTF-8 "…" */

    void Attach(wxTreeListCtrl* tree);
    void SetOnModulePathSelected(std::function<void(const std::string& modulePath)> fn);

    void RebuildFromModulePaths(const std::vector<std::string>& modulePaths);

    void OnTreeExpanding(wxTreeListEvent& event);

    wxTreeListItem ModulesRoot() const;
    wxTreeListItem EnsurePath(const std::string& modulePath);
    std::string PathOf(wxTreeListItem item) const;

    void CollectExpandedPaths(std::vector<std::string>& out) const;

private:
    bool IsModulesRoot(wxTreeListItem item) const;
    bool IsPlaceholderChild(wxTreeListItem item) const;
    void PopulateDirectChildren(wxTreeListItem parent, const std::string& parentPath);
    wxTreeListItem FindDirectChild(wxTreeListItem parent, const std::string& segment) const;

    wxTreeListCtrl* m_tree = nullptr;
    ModuleTreeLazyIndex m_index;
    std::map<intptr_t, std::string> m_itemPaths;

    static intptr_t ItemKey(wxTreeListItem item);
    void SetItemPath(wxTreeListItem item, const std::string& path);
    std::string GetItemPath(wxTreeListItem item) const;
    std::function<void(const std::string&)> m_onSelect;
};
