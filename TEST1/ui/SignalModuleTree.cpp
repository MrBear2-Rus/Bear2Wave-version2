#include "ui/SignalModuleTree.h"

#include "core/waveform_perf.h"

#include <algorithm>
#include <cstring>
#include <functional>
#include <unordered_map>
#include <unordered_set>

SignalModuleTree::SignalModuleTree(TraceDocument& document)
    : m_document(document)
{
}

void SignalModuleTree::Attach(wxTreeListCtrl* tree, ModuleTreeLazyCtrl* lazy, wxListCtrl* legacyList)
{
    m_tree = tree;
    m_lazy = lazy;
    m_legacyList = legacyList;
    m_list = nullptr;
}

void SignalModuleTree::Attach(wxTreeListCtrl* tree, ModuleSignalListView* list)
{
    m_tree = tree;
    m_list = list;
    m_lazy = nullptr;
    m_legacyList = nullptr;
}

void SignalModuleTree::RebuildFromVcd(vcd_t* vcd)
{
    m_document.BuildModuleIndexFromVcd(vcd);
    RebuildModuleTreeFromIndex();
}

void SignalModuleTree::RebuildFromExistingIndex()
{
    RebuildModuleTreeFromIndex();
}

void SignalModuleTree::SetSstFilter(const std::string& filterText)
{
    m_sstFilterText = filterText;
    m_sstFilter = sst_filter_parse(filterText);
    RebuildModuleTreeFromIndex();
}

size_t SignalModuleTree::CountMatchingSignals() const
{
    if (m_sstFilter.empty)
        return 0;

    std::unordered_set<signal_t*> seen;
    size_t n = 0;
    for (const auto& kv : m_document.ModuleIndex()) {
        for (signal_t* sig : kv.second) {
            if (!sig || seen.count(sig))
                continue;
            if (sst_filter_match(m_sstFilter, sig)) {
                seen.insert(sig);
                ++n;
            }
        }
    }
    return n;
}

void SignalModuleTree::AddModulePathAndAncestors(std::unordered_set<std::string>& out,
    const std::string& modulePathIn)
{
    std::string path = TraceDocument::NormalizeModulePathKey(modulePathIn.c_str());
    if (path == "$root")
        path.clear();
    out.insert(path);
    while (!path.empty()) {
        const size_t dot = path.rfind('.');
        if (dot == std::string::npos)
            break;
        path = path.substr(0, dot);
        out.insert(path);
    }
}

std::vector<std::string> SignalModuleTree::ModulePathsVisibleUnderFilter() const
{
    std::vector<std::string> paths;
    if (m_sstFilter.empty) {
        paths.reserve(m_document.ModuleIndex().size());
        for (const auto& kv : m_document.ModuleIndex())
            paths.push_back(kv.first);
        return paths;
    }

    for (const auto& kv : m_document.ModuleIndex()) {
        for (signal_t* sig : kv.second) {
            if (sig && sst_filter_match(m_sstFilter, sig)) {
                paths.push_back(kv.first);
                break;
            }
        }
    }
    std::sort(paths.begin(), paths.end());
    return paths;
}

void SignalModuleTree::FilterSignalRows(std::vector<signal_t*>& rows) const
{
    if (m_sstFilter.empty)
        return;
    rows.erase(
        std::remove_if(rows.begin(), rows.end(),
            [this](signal_t* sig) { return !sig || !sst_filter_match(m_sstFilter, sig); }),
        rows.end());
}

void SignalModuleTree::RebuildModuleTreeFromIndex()
{
    if (!m_tree)
        return;

    if (m_lazy) {
        const std::vector<std::string> paths = ModulePathsVisibleUnderFilter();
        m_lazy->RebuildFromModulePaths(paths);
        FillSignalListAllFromModules();
        return;
    }

    PopulateTreeFromIndex();
}

void SignalModuleTree::PopulateTreeFromIndex()
{
    if (!m_tree)
        return;

    m_tree->DeleteAllItems();
    const wxTreeListItem rootId = m_tree->AppendItem(m_tree->GetRootItem(), "VCD Modules");
    std::unordered_map<std::string, wxTreeListItem> pathMap;
    pathMap[""] = rootId;

    std::unordered_set<std::string> visible;
    if (!m_sstFilter.empty) {
        for (const auto& kv : m_document.ModuleIndex()) {
            for (signal_t* sig : kv.second) {
                if (sig && sst_filter_match(m_sstFilter, sig)) {
                    AddModulePathAndAncestors(visible, kv.first);
                    break;
                }
            }
        }
    }

    for (const auto& pair : m_document.ModuleIndex()) {
        if (!m_sstFilter.empty && visible.count(pair.first) == 0)
            continue;

        const std::string& modulePath = TraceDocument::NormalizeModulePathKey(pair.first.c_str());
        std::string fullPath;
        wxTreeListItem parent = rootId;
        size_t start = 0;
        while (start < modulePath.size()) {
            const size_t dot = modulePath.find('.', start);
            const std::string part = (dot == std::string::npos) ? modulePath.substr(start)
                                                                : modulePath.substr(start, dot - start);
            start = (dot == std::string::npos) ? modulePath.size() : dot + 1;
            if (part.empty())
                continue;
            if (!fullPath.empty())
                fullPath += ".";
            fullPath += part;

            if (pathMap.count(fullPath)) {
                parent = pathMap[fullPath];
                continue;
            }
            const wxTreeListItem item = m_tree->AppendItem(parent, wxString::FromUTF8(part));
            pathMap[fullPath] = item;
            parent = item;
        }
    }

    FillSignalListAllFromModules();
    m_tree->Expand(rootId);
}

void SignalModuleTree::FillLegacyList(const std::vector<signal_t*>& rows, bool showFullName, size_t totalHint)
{
    if (!m_legacyList)
        return;

    m_legacyList->DeleteAllItems();
    const int maxRows = WaveformPerf::MaxSignalListRows();
    int shown = 0;
    const size_t total = totalHint > 0 ? totalHint : rows.size();

    for (signal_t* sig : rows) {
        if (!sig)
            continue;
        if (shown >= maxRows) {
            long idx = m_legacyList->InsertItem(m_legacyList->GetItemCount(), wxT("…"));
            m_legacyList->SetItem(
                idx,
                1,
                wxString::Format(
                    wxT("(%zu signals total — showing first %d; narrow module or use search)"),
                    total,
                    maxRows));
            break;
        }
        long idx = m_legacyList->InsertItem(m_legacyList->GetItemCount(), "wire");
        if (showFullName && sig->full_name[0])
            m_legacyList->SetItem(idx, 1, wxString::FromUTF8(sig->full_name));
        else
            m_legacyList->SetItem(idx, 1, wxString::FromUTF8(sig->name));
        m_legacyList->SetItemPtrData(idx, (wxUIntPtr)sig);
        ++shown;
    }
}

void SignalModuleTree::FillSignalListForModulePath(const std::string& modulePath)
{
    auto& index = m_document.ModuleIndex();
    auto it = index.find(modulePath);
    if (it == index.end() && modulePath == "$root") {
        it = index.find("");
        if (it == index.end())
            it = index.find("$root");
    }

    if (m_list) {
        if (it == index.end()) {
            m_list->Clear();
            return;
        }
        std::vector<signal_t*> rows;
        rows.reserve(it->second.size());
        for (signal_t* sig : it->second) {
            if (sig)
                rows.push_back(sig);
        }
        FilterSignalRows(rows);
        m_list->SetSignals(rows, false);
        return;
    }

    if (!m_legacyList)
        return;

    if (it == index.end()) {
        m_legacyList->DeleteAllItems();
        return;
    }

    std::vector<signal_t*> rows;
    rows.reserve(it->second.size());
    for (signal_t* sig : it->second) {
        if (sig)
            rows.push_back(sig);
    }
    FilterSignalRows(rows);
    FillLegacyList(rows, false, rows.size());
}

void SignalModuleTree::FillSignalListAllFromModules()
{
    std::vector<signal_t*> flat;
    std::unordered_set<signal_t*> seen;
    for (const auto& kv : m_document.ModuleIndex()) {
        for (signal_t* sig : kv.second) {
            if (!sig || seen.count(sig))
                continue;
            seen.insert(sig);
            flat.push_back(sig);
        }
    }
    std::sort(flat.begin(), flat.end(), [](const signal_t* a, const signal_t* b) {
        return std::strcmp(a->full_name, b->full_name) < 0;
    });
    FilterSignalRows(flat);

    if (m_list) {
        m_list->SetSignals(flat, true);
        return;
    }

    if (m_legacyList) {
        const int maxRows = WaveformPerf::MaxSignalListRows();
        if ((int)flat.size() > maxRows) {
            std::vector<signal_t*> head(flat.begin(), flat.begin() + maxRows);
            FillLegacyList(head, true, flat.size());
            return;
        }
        FillLegacyList(flat, true, flat.size());
    }
}

void SignalModuleTree::OnTreeSelect(wxTreeListEvent& event)
{
    const wxTreeListItem item = event.GetItem();
    if (!item.IsOk())
        return;

    std::string modulePath;
    if (m_lazy)
        modulePath = m_lazy->PathOf(item);
    else
        modulePath = GetFullPath(item);

    if (modulePath.empty())
        FillSignalListAllFromModules();
    else
        FillSignalListForModulePath(modulePath);
}

std::string SignalModuleTree::GetFullPath(wxTreeListItem item) const
{
    if (!m_tree)
        return {};
    if (m_lazy) {
        const std::string p = m_lazy->PathOf(item);
        if (!p.empty())
            return p;
    }
    wxString path;
    while (item.IsOk()) {
        const wxString name = m_tree->GetItemText(item, 0);
        if (name == "VCD Modules")
            break;
        path = path.IsEmpty() ? name : name + "." + path;
        item = m_tree->GetItemParent(item);
    }
    return path.ToStdString();
}

void SignalModuleTree::OnModuleRightClick(wxContextMenuEvent& event, wxMenu* moduleMenu)
{
    if (!m_tree || !moduleMenu)
        return;

    wxPoint pt = event.GetPosition();
    if (pt == wxDefaultPosition)
        pt = wxGetMousePosition();
    pt = m_tree->ScreenToClient(pt);

    const wxTreeListItem item = m_tree->GetSelection();
    if (!item.IsOk())
        return;

    m_tree->Select(item);
    m_rightClickModuleItem = item;
    m_tree->PopupMenu(moduleMenu, pt);
}

void SignalModuleTree::GetAllSignalsInModuleTree(const std::string& rootPath,
    std::vector<signal_t*>& outSignals) const
{
    for (const auto& pair : m_document.ModuleIndex()) {
        const std::string& path = pair.first;
        if (path.find(rootPath) == 0)
            outSignals.insert(outSignals.end(), pair.second.begin(), pair.second.end());
    }
}

std::string SignalModuleTree::ModulePathFromSignal(const signal_t* sig)
{
    if (!sig)
        return "$root";
    std::string mp = TraceDocument::NormalizeModulePathKey(sig->module_path);
    if (mp != "$root" && !mp.empty())
        return mp;
    if (!sig->full_name[0])
        return "$root";
    const std::string fn = sig->full_name;
    const size_t dot = fn.rfind('.');
    if (dot == std::string::npos)
        return "$root";
    return fn.substr(0, dot);
}

wxTreeListItem SignalModuleTree::ModulesRoot() const
{
    if (m_lazy)
        return m_lazy->ModulesRoot();
    return FindVcdModulesRoot();
}

wxTreeListItem SignalModuleTree::EnsureModulePath(const std::string& modulePathIn)
{
    if (m_lazy)
        return m_lazy->EnsurePath(TrimModulePathForTree(modulePathIn));
    return FindTreeItemByModulePath(modulePathIn);
}

wxTreeListItem SignalModuleTree::FindVcdModulesRoot() const
{
    if (!m_tree)
        return wxTreeListItem();
    const wxTreeListItem root = m_tree->GetRootItem();
    wxTreeListItem child = m_tree->GetFirstChild(root);
    while (child.IsOk()) {
        if (m_tree->GetItemText(child, 0) == "VCD Modules")
            return child;
        child = m_tree->GetNextSibling(child);
    }
    return wxTreeListItem();
}

std::string SignalModuleTree::TrimModulePathForTree(std::string p)
{
    while (!p.empty() && (p.back() == '.' || p.back() == '/'))
        p.pop_back();
    return p;
}

wxTreeListItem SignalModuleTree::FindTreeItemByModulePath(const std::string& modulePathIn) const
{
    const wxTreeListItem modules = FindVcdModulesRoot();
    if (!modules.IsOk())
        return wxTreeListItem();

    std::string path = TrimModulePathForTree(modulePathIn);
    if (path.empty() || path == "$root")
        return modules;

    wxTreeListItem cur = modules;
    size_t start = 0;
    while (start < path.size()) {
        const size_t dot = path.find('.', start);
        const std::string part = (dot == std::string::npos) ? path.substr(start) : path.substr(start, dot - start);
        start = (dot == std::string::npos) ? path.size() : dot + 1;
        if (part.empty())
            continue;

        wxTreeListItem found;
        wxTreeListItem child = m_tree->GetFirstChild(cur);
        while (child.IsOk()) {
            if (m_tree->GetItemText(child, 0) == wxString::FromUTF8(part)) {
                found = child;
                break;
            }
            child = m_tree->GetNextSibling(child);
        }
        if (!found.IsOk())
            return wxTreeListItem();
        cur = found;
    }
    return cur;
}

void SignalModuleTree::ExpandTreeToItem(wxTreeListItem item) const
{
    if (!m_tree || !item.IsOk())
        return;
    for (wxTreeListItem p = item; p.IsOk(); p = m_tree->GetItemParent(p))
        m_tree->Expand(p);
}

void SignalModuleTree::OpenScopeForSignal(signal_t* sig, const std::function<void(const wxString&)>& setStatus)
{
    if (!sig || !m_tree) {
        if (setStatus)
            setStatus("Open Scope: no signal or module tree.");
        return;
    }

    const std::string mp = ModulePathFromSignal(sig);
    const wxTreeListItem item = EnsureModulePath(mp);
    if (!item.IsOk()) {
        if (setStatus)
            setStatus("Open Scope: module not found in tree: " + wxString::FromUTF8(mp.c_str()));
        return;
    }

    ExpandTreeToItem(item);
    m_tree->Select(item);
    m_tree->EnsureVisible(item);

    const std::string path = m_lazy ? m_lazy->PathOf(item) : GetFullPath(item);
    FillSignalListForModulePath(path.empty() ? mp : path);

    const wxString sigLabel = sig->full_name[0] ? wxString::FromUTF8(sig->full_name)
                                                : wxString::FromUTF8(sig->name);
    if (setStatus)
        setStatus("Open Scope: " + wxString::FromUTF8(mp.c_str()) + " (" + sigLabel + ")");
}

void SignalModuleTree::ApplyOpenPaths(const std::vector<std::string>& paths)
{
    if (!m_tree || paths.empty())
        return;

    wxTreeListItem lastOk;
    for (const std::string& raw : paths) {
        const wxTreeListItem item = EnsureModulePath(raw);
        if (!item.IsOk())
            continue;
        ExpandTreeToItem(item);
        lastOk = item;
    }
    if (lastOk.IsOk()) {
        m_tree->Select(lastOk);
        m_tree->EnsureVisible(lastOk);
        const std::string path = m_lazy ? m_lazy->PathOf(lastOk) : GetFullPath(lastOk);
        FillSignalListForModulePath(path);
    }
}

void SignalModuleTree::CollectExpandedPaths(std::vector<std::string>& out) const
{
    if (m_lazy) {
        m_lazy->CollectExpandedPaths(out);
        return;
    }
    if (!m_tree)
        return;
    const wxTreeListItem modules = FindVcdModulesRoot();
    if (!modules.IsOk())
        return;

    std::function<void(wxTreeListItem)> walk;
    walk = [&](wxTreeListItem it) {
        if (!it.IsOk())
            return;
        if (it != modules && m_tree->IsExpanded(it)) {
            const std::string p = GetFullPath(it);
            if (!p.empty()) {
                bool dup = false;
                for (const std::string& e : out) {
                    if (e == p) {
                        dup = true;
                        break;
                    }
                }
                if (!dup)
                    out.push_back(p);
            }
        }
        wxTreeListItem ch = m_tree->GetFirstChild(it);
        while (ch.IsOk()) {
            walk(ch);
            ch = m_tree->GetNextSibling(ch);
        }
    };
    walk(modules);
}
