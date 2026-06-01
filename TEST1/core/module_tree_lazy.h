#pragma once

#include <string>
#include <unordered_map>
#include <vector>

/** Direct-child module segment index for lazy wx tree population (P4-6). */
class ModuleTreeLazyIndex {
public:
    void Clear();
    void BuildFromModulePaths(const std::vector<std::string>& modulePaths);

    const std::vector<std::string>& DirectChildren(const std::string& parentPath) const;
    bool HasChildren(const std::string& parentPath) const;

private:
    static const std::vector<std::string> kEmpty;
    std::unordered_map<std::string, std::vector<std::string>> m_children;
};
