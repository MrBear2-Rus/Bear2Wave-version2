#pragma once

#include <string>
#include <wx/string.h>

struct Bear2WaveConfig {
    std::string api_key;
    std::string model = "deepseek-chat";
    std::string api_host = "api.deepseek.com";
    std::string api_path = "/v1/chat/completions";
    float temperature = 0.4f;
    bool use_ollama = false;

    static std::wstring ConfigDir();
    void ApplyOllamaPreset();
    static std::wstring ConfigPath();

    bool Load();
    bool Save() const;

    wxString ApiKeyWx() const;
    void SetApiKeyWx(const wxString& key);
};
