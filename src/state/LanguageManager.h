#pragma once
#include <string>
#include <vector>

namespace LanguageManager {
    extern std::string g_currentLanguage;
    extern std::vector<std::pair<std::string, std::string>> g_availableLanguages;

    void Init();
    void SaveConfig();
    void LoadConfig();
    void LoadLanguage(const std::string& langCode);
    void ScanLanguages();
    const char* GetText(const std::string& key);
}
