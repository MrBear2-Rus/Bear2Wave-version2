#include "core/module_tree_lazy.h"

#include <algorithm>

const std::vector<std::string> ModuleTreeLazyIndex::kEmpty;

void ModuleTreeLazyIndex::Clear()
{
    m_children.clear();
}

void ModuleTreeLazyIndex::BuildFromModulePaths(const std::vector<std::string>& modulePaths)
{
    Clear();

    auto add_child = [&](const std::string& parent, const std::string& child) {
        if (child.empty())
            return;
        auto& v = m_children[parent];
        if (std::find(v.begin(), v.end(), child) == v.end())
            v.push_back(child);
    };

    for (const std::string& raw : modulePaths) {
        std::string path = raw;
        if (path == "$root")
            path.clear();

        std::string full;
        size_t start = 0;
        while (start < path.size()) {
            const size_t dot = path.find('.', start);
            const std::string part = (dot == std::string::npos) ? path.substr(start)
                                                                : path.substr(start, dot - start);
            start = (dot == std::string::npos) ? path.size() : dot + 1;
            if (part.empty())
                continue;
            add_child(full, part);
            if (!full.empty())
                full += ".";
            full += part;
        }
    }

    for (auto& kv : m_children)
        std::sort(kv.second.begin(), kv.second.end());
}

const std::vector<std::string>& ModuleTreeLazyIndex::DirectChildren(const std::string& parentPath) const
{
    const auto it = m_children.find(parentPath);
    return it == m_children.end() ? kEmpty : it->second;
}

bool ModuleTreeLazyIndex::HasChildren(const std::string& parentPath) const
{
    return !DirectChildren(parentPath).empty();
}
