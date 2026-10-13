#pragma once
#include <filesystem>
#include <string>

namespace dps {
// Application behaviour, stored independently of sound profiles.
struct Preferences {
    bool startWithWindows=false,startMinimised=false,closeExits=false,autoReconnect=true;
    int dspAtStart=0; // 0 = last session, 1 = on, 2 = off
    int themeMode=0; // 0 = system, 1 = dark, 2 = light
    std::string virtualInputId;
    bool operator==(const Preferences&) const = default;
};
Preferences loadPreferences(const std::filesystem::path& path);
void savePreferences(const std::filesystem::path& path,const Preferences& preferences);
}
