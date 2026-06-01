#pragma once

#include <wx/string.h>

#include <utility>
#include <vector>

struct HelpSectionEntry {
    const char* title;
    const char* htmlFile;      /**< under docs/help/ */
    const char* markdownFile;  /**< fallback under docs/ when html missing */
};

inline std::vector<HelpSectionEntry> DefaultHelpCatalog()
{
    return {
        {"概述与快速入门", "overview.html", "USER_GUIDE.md"},
        {"主界面布局", "interface.html", nullptr},
        {"波形交互", "waveform.html", nullptr},
        {"标记与测量", "markers.html", nullptr},
        {"编辑信号列表", "edit_signals.html", nullptr},
        {"会话文件 (.bwv)", "session.html", nullptr},
        {"Compare 对比", "compare.html", nullptr},
        {"AI 与本地分析", "ai.html", nullptr},
        {"文件格式", "formats.html", nullptr},
        {"性能与调优", "performance.html", nullptr},
        {"故障排除", "troubleshooting.html", nullptr},
        {"快捷键大全", "shortcuts.html", "SHORTCUTS.md"},
        {"环境变量", "environment.html", "ENVIRONMENT.md"},
        {"熊二的彩蛋 · 狗熊岭", "easter_egg.html", nullptr},
    };
}
