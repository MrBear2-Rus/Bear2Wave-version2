#include "core/ui_theme.h"

#include <cstdio>
#include <cstring>
#include <filesystem>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <shlobj.h>
#endif

namespace {

UiThemeId g_themeId = UiThemeId::Light;

UiThemeColors MakeLight()
{
    UiThemeColors t;
    t.panelBg = wxColour(252, 253, 255);
    t.plotBg = wxColour(252, 253, 255);
    t.rowStripe = wxColour(241, 244, 248);
    t.axisBand = wxColour(232, 236, 242);
    t.scrollBarBg = wxColour(240, 248, 255);
    t.signalNameText = wxColour(80, 80, 80);
    t.signalNameDim = wxColour(180, 180, 180);
    t.traceDefault = wxColour(44, 62, 80);
    t.selectedRow = wxColour(255, 252, 220);
    t.commentRow = wxColour(235, 245, 255);
    t.gridMajor = wxColour(200, 200, 200);
    t.gridMinor = wxColour(230, 230, 230);
    t.playhead = wxColour(255, 200, 0);
    t.baseline = wxColour(180, 180, 180);
    t.measureLine = wxColour(255, 0, 0);
    t.cursorValueBg = wxColour(255, 255, 220);
    t.markerLabelBg = wxColour(255, 255, 220);
    t.hoverHighlight = wxColour(255, 140, 0);
    t.textOverlayBg = wxColour(0, 0, 0); /* alpha cleared in upload */
    return t;
}

UiThemeColors MakeDark()
{
    UiThemeColors t;
    t.panelBg = wxColour(30, 32, 38);
    t.plotBg = wxColour(24, 26, 32);
    t.rowStripe = wxColour(32, 35, 42);
    t.axisBand = wxColour(40, 44, 52);
    t.scrollBarBg = wxColour(36, 40, 48);
    t.signalNameText = wxColour(210, 214, 220);
    t.signalNameDim = wxColour(120, 125, 135);
    t.traceDefault = wxColour(120, 180, 255);
    t.selectedRow = wxColour(55, 58, 68);
    t.commentRow = wxColour(42, 46, 56);
    t.gridMajor = wxColour(70, 74, 84);
    t.gridMinor = wxColour(50, 54, 62);
    t.playhead = wxColour(255, 200, 80);
    t.baseline = wxColour(160, 160, 170);
    t.measureLine = wxColour(255, 90, 90);
    t.cursorValueBg = wxColour(50, 54, 64);
    t.markerLabelBg = wxColour(50, 54, 64);
    t.hoverHighlight = wxColour(255, 160, 60);
    t.textOverlayBg = wxColour(0, 0, 0);
    return t;
}

UiThemeColors g_light = MakeLight();
UiThemeColors g_dark = MakeDark();

std::string ThemePrefPath()
{
#ifdef _WIN32
    wchar_t path[MAX_PATH] = {};
    if (FAILED(SHGetFolderPathW(nullptr, CSIDL_LOCAL_APPDATA, nullptr, 0, path)))
        return {};
    char narrow[MAX_PATH * 4] = {};
    WideCharToMultiByte(CP_UTF8, 0, path, -1, narrow, sizeof(narrow), nullptr, nullptr);
    return std::string(narrow) + "\\Bear2Wave\\ui.ini";
#else
    const char* home = getenv("HOME");
    if (!home)
        return {};
    return std::string(home) + "/.bear2wave/ui.ini";
#endif
}

} // namespace

UiThemeId CurrentThemeId()
{
    return g_themeId;
}

const UiThemeColors& CurrentTheme()
{
    return g_themeId == UiThemeId::Dark ? g_dark : g_light;
}

bool IsDarkTheme()
{
    return g_themeId == UiThemeId::Dark;
}

void SetTheme(UiThemeId id)
{
    g_themeId = id;
    SaveThemePreference();
}

void CurrentThemeGlClear(float out_rgba[4])
{
    const UiThemeColors& t = CurrentTheme();
    out_rgba[0] = t.plotBg.Red() / 255.0f;
    out_rgba[1] = t.plotBg.Green() / 255.0f;
    out_rgba[2] = t.plotBg.Blue() / 255.0f;
    out_rgba[3] = 1.0f;
}

void LoadThemePreference()
{
    const std::string path = ThemePrefPath();
    if (path.empty())
        return;
    FILE* f = fopen(path.c_str(), "r");
    if (!f)
        return;
    char line[64] = {};
    if (fgets(line, sizeof(line), f)) {
        if (strstr(line, "dark"))
            g_themeId = UiThemeId::Dark;
        else
            g_themeId = UiThemeId::Light;
    }
    fclose(f);
}

void SaveThemePreference()
{
    const std::string path = ThemePrefPath();
    if (path.empty())
        return;
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    FILE* f = fopen(path.c_str(), "w");
    if (!f)
        return;
    fprintf(f, "theme=%s\n", g_themeId == UiThemeId::Dark ? "dark" : "light");
    fclose(f);
}
