#include "ui/ModuleTreeLazyCtrl.h"

intptr_t ModuleTreeLazyCtrl::ItemKey(wxTreeListItem item)
{
    return reinterpret_cast<intptr_t>(item.GetID());
}

void ModuleTreeLazyCtrl::SetItemPath(wxTreeListItem item, const std::string& path)
{
    if (item.IsOk())
        m_itemPaths[ItemKey(item)] = path;
}

std::string ModuleTreeLazyCtrl::GetItemPath(wxTreeListItem item) const
{
    const auto it = m_itemPaths.find(ItemKey(item));
    return it != m_itemPaths.end() ? it->second : std::string();
}

void ModuleTreeLazyCtrl::Attach(wxTreeListCtrl* tree)
{
    m_tree = tree;
}

void ModuleTreeLazyCtrl::SetOnModulePathSelected(std::function<void(const std::string& modulePath)> fn)
{
    m_onSelect = std::move(fn);
}

void ModuleTreeLazyCtrl::RebuildFromModulePaths(const std::vector<std::string>& modulePaths)
{
    if (!m_tree)
        return;

    m_index.BuildFromModulePaths(modulePaths);
    m_itemPaths.clear();
    m_tree->DeleteAllItems();

    const wxTreeListItem rootId = m_tree->AppendItem(m_tree->GetRootItem(), "VCD Modules");
    SetItemPath(rootId, "");
    PopulateDirectChildren(rootId, "");
    m_tree->Expand(rootId);

    if (m_onSelect)
        m_onSelect("");
}

bool ModuleTreeLazyCtrl::IsModulesRoot(wxTreeListItem item) const
{
    return m_tree && item.IsOk() && m_tree->GetItemText(item, 0) == "VCD Modules";
}

bool ModuleTreeLazyCtrl::IsPlaceholderChild(wxTreeListItem item) const
{
    return m_tree && item.IsOk() && m_tree->GetItemText(item, 0) == kPlaceholderLabel;
}

void ModuleTreeLazyCtrl::PopulateDirectChildren(wxTreeListItem parent, const std::string& parentPath)
{
    if (!m_tree || !parent.IsOk())
        return;

    wxTreeListItem child = m_tree->GetFirstChild(parent);
    while (child.IsOk()) {
        wxTreeListItem next = m_tree->GetNextSibling(child);
        m_itemPaths.erase(ItemKey(child));
        m_tree->DeleteItem(child);
        child = next;
    }

    for (const std::string& segment : m_index.DirectChildren(parentPath)) {
        std::string full = parentPath;
        if (!full.empty())
            full += ".";
        full += segment;

        const wxTreeListItem item = m_tree->AppendItem(parent, wxString::FromUTF8(segment));
        SetItemPath(item, full);

        if (m_index.HasChildren(full)) {
            const wxTreeListItem ph = m_tree->AppendItem(item, kPlaceholderLabel);
            SetItemPath(ph, full + "\x01"); /* marker, not a real path */
        }
    }
}

wxTreeListItem ModuleTreeLazyCtrl::FindDirectChild(wxTreeListItem parent, const std::string& segment) const
{
    if (!m_tree || !parent.IsOk())
        return wxTreeListItem();

    wxTreeListItem child = m_tree->GetFirstChild(parent);
    while (child.IsOk()) {
        if (IsPlaceholderChild(child)) {
            child = m_tree->GetNextSibling(child);
            continue;
        }
        if (m_tree->GetItemText(child, 0) == wxString::FromUTF8(segment))
            return child;
        child = m_tree->GetNextSibling(child);
    }
    return wxTreeListItem();
}

void ModuleTreeLazyCtrl::OnTreeExpanding(wxTreeListEvent& event)
{
    const wxTreeListItem item = event.GetItem();
    if (!item.IsOk() || IsModulesRoot(item))
        return;

    wxTreeListItem first = m_tree->GetFirstChild(item);
    if (!first.IsOk() || !IsPlaceholderChild(first))
        return;

    const std::string stored = GetItemPath(item);
    const std::string parentPath = (!stored.empty() && stored.back() != '\x01') ? stored : PathOf(item);
    PopulateDirectChildren(item, parentPath);
}

wxTreeListItem ModuleTreeLazyCtrl::ModulesRoot() const
{
    if (!m_tree)
        return wxTreeListItem();
    const wxTreeListItem root = m_tree->GetRootItem();
    wxTreeListItem child = m_tree->GetFirstChild(root);
    while (child.IsOk()) {
        if (IsModulesRoot(child))
            return child;
        child = m_tree->GetNextSibling(child);
    }
    return wxTreeListItem();
}

std::string ModuleTreeLazyCtrl::PathOf(wxTreeListItem item) const
{
    const std::string stored = GetItemPath(item);
    if (!stored.empty() && stored.back() != '\x01')
        return stored;

    if (!m_tree || !item.IsOk())
        return {};

    wxString path;
    while (item.IsOk()) {
        const wxString name = m_tree->GetItemText(item, 0);
        if (name == "VCD Modules")
            break;
        if (name == kPlaceholderLabel) {
            item = m_tree->GetItemParent(item);
            continue;
        }
        path = path.IsEmpty() ? name : name + "." + path;
        item = m_tree->GetItemParent(item);
    }
    return path.ToStdString();
}

wxTreeListItem ModuleTreeLazyCtrl::EnsurePath(const std::string& modulePathIn)
{
    wxTreeListItem modules = ModulesRoot();
    if (!modules.IsOk())
        return wxTreeListItem();

    std::string path = modulePathIn;
    while (!path.empty() && (path.back() == '.' || path.back() == '/'))
        path.pop_back();
    if (path.empty() || path == "$root")
        return modules;

    wxTreeListItem cur = modules;
    std::string built;
    size_t start = 0;
    while (start < path.size()) {
        const size_t dot = path.find('.', start);
        const std::string part = (dot == std::string::npos) ? path.substr(start) : path.substr(start, dot - start);
        start = (dot == std::string::npos) ? path.size() : dot + 1;
        if (part.empty())
            continue;

        wxTreeListItem first = m_tree->GetFirstChild(cur);
        if (first.IsOk() && IsPlaceholderChild(first))
            PopulateDirectChildren(cur, built);

        wxTreeListItem next = FindDirectChild(cur, part);
        if (!next.IsOk()) {
            PopulateDirectChildren(cur, built);
            next = FindDirectChild(cur, part);
        }
        if (!next.IsOk())
            return wxTreeListItem();

        cur = next;
        if (!built.empty())
            built += ".";
        built += part;
        m_tree->Expand(cur);
    }
    return cur;
}

void ModuleTreeLazyCtrl::CollectExpandedPaths(std::vector<std::string>& out) const
{
    const wxTreeListItem modules = ModulesRoot();
    if (!m_tree || !modules.IsOk())
        return;

    std::function<void(wxTreeListItem)> walk;
    walk = [&](wxTreeListItem it) {
        if (!it.IsOk())
            return;
        if (it != modules && m_tree->IsExpanded(it) && !IsPlaceholderChild(it)) {
            const std::string p = PathOf(it);
            if (!p.empty() && std::find(out.begin(), out.end(), p) == out.end())
                out.push_back(p);
        }
        wxTreeListItem ch = m_tree->GetFirstChild(it);
        while (ch.IsOk()) {
            if (!IsPlaceholderChild(ch))
                walk(ch);
            ch = m_tree->GetNextSibling(ch);
        }
    };
    walk(modules);
}
