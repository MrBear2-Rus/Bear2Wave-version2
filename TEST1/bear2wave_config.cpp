#include "bear2wave_config.h"

#include <fstream>
#include <sstream>
#include <algorithm>
#include <cctype>

#ifdef _WIN32
#include <windows.h>
#include <shlobj.h>
#endif

static std::string trim(const std::string& s)
{
    size_t a = 0;
    while (a < s.size() && std::isspace((unsigned char)s[a]))
        ++a;
    size_t b = s.size();
    while (b > a && std::isspace((unsigned char)s[b - 1]))
        --b;
    return s.substr(a, b - a);
}

std::wstring Bear2WaveConfig::ConfigDir()
{
#ifdef _WIN32
    wchar_t path[MAX_PATH] = {};
    if (SUCCEEDED(SHGetFolderPathW(nullptr, CSIDL_APPDATA, nullptr, 0, path))) {
        std::wstring dir = path;
        dir += L"\\Bear2Wave";
        CreateDirectoryW(dir.c_str(), nullptr);
        return dir;
    }
#endif
    return L".";
}

std::wstring Bear2WaveConfig::ConfigPath()
{
    return ConfigDir() + L"\\config.ini";
}

static std::string wide_path_utf8(const std::wstring& wpath)
{
    if (wpath.empty())
        return {};
    const int n = WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1)
        return {};
    std::string out((size_t)n - 1, '\0');
    WideCharToMultiByte(CP_UTF8, 0, wpath.c_str(), -1, out.data(), n, nullptr, nullptr);
    return out;
}

bool Bear2WaveConfig::Load()
{
    const std::string path = wide_path_utf8(ConfigPath());
    std::ifstream in(path);
    if (!in.is_open())
        return false;

    std::string line;
    while (std::getline(in, line)) {
        line = trim(line);
        if (line.empty() || line[0] == '#' || line[0] == ';')
            continue;
        const size_t eq = line.find('=');
        if (eq == std::string::npos)
            continue;
        const std::string key = trim(line.substr(0, eq));
        const std::string val = trim(line.substr(eq + 1));
        if (key == "api_key")
            api_key = val;
        else if (key == "model")
            model = val;
        else if (key == "api_host")
            api_host = val;
        else if (key == "api_path")
            api_path = val;
        else if (key == "temperature") {
            try {
                temperature = std::stof(val);
            } catch (...) {
            }
        } else if (key == "use_ollama")
            use_ollama = (val == "1" || val == "true" || val == "yes");
    }
    if (use_ollama)
        ApplyOllamaPreset();
    return true;
}

void Bear2WaveConfig::ApplyOllamaPreset()
{
    api_host = "127.0.0.1:11434";
    api_path = "/v1/chat/completions";
    if (model.empty() || model == "deepseek-chat")
        model = "llama3.2";
    temperature = 0.3f;
    use_ollama = true;
}

bool Bear2WaveConfig::Save() const
{
    const std::string path = wide_path_utf8(ConfigPath());
    std::ofstream out(path);
    if (!out.is_open())
        return false;

    out << "# Bear2Wave AI settings (do not share this file)\n";
    out << "api_key=" << api_key << "\n";
    out << "model=" << model << "\n";
    out << "api_host=" << api_host << "\n";
    out << "api_path=" << api_path << "\n";
    out << "temperature=" << temperature << "\n";
    out << "use_ollama=" << (use_ollama ? "1" : "0") << "\n";
    return true;
}

wxString Bear2WaveConfig::ApiKeyWx() const
{
    return wxString::FromUTF8(api_key);
}

void Bear2WaveConfig::SetApiKeyWx(const wxString& key)
{
    api_key = key.utf8_string();
}
