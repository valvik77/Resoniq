#include "dps/preferences.hpp"
#include <fstream>
#include <iostream>
#include <stdexcept>
#ifdef _WIN32
#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
namespace {
void check(bool condition,const char* message) {if(!condition) throw std::runtime_error(message);}
std::string contents(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);
    return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
}
int main() {
    const auto path=std::filesystem::temp_directory_path()/"resoniq-preferences-test.txt";
    try {
        std::filesystem::remove(path);
        check(dps::loadPreferences(path)==dps::Preferences{},"Missing preferences do not default");
        for(int modes=0;modes<9;++modes) {
            dps::Preferences preferences;
            preferences.startWithWindows=true;preferences.startMinimised=true;
            preferences.closeExits=true;preferences.autoReconnect=false;
            preferences.dspAtStart=modes/3;preferences.themeMode=modes%3;
            preferences.virtualInputId="{saved-virtual-endpoint}";
            dps::savePreferences(path,preferences);
            check(dps::loadPreferences(path)==preferences,"Preferences roundtrip failed");
        }
        const auto previous=contents(path);
        auto invalid=dps::Preferences{};invalid.themeMode=3;
        bool failed=false;
        try {dps::savePreferences(path,invalid);} catch(const std::exception&) {failed=true;}
        check(failed&&contents(path)==previous,"Invalid save destroyed preferences");
#ifdef _WIN32
        HANDLE locked=CreateFileW(path.c_str(),GENERIC_READ,FILE_SHARE_READ,nullptr,OPEN_EXISTING,0,nullptr);
        check(locked!=INVALID_HANDLE_VALUE,"Cannot lock test preferences");
        failed=false;
        try {dps::savePreferences(path,{});} catch(const std::exception&) {failed=true;}
        CloseHandle(locked);
        check(failed&&contents(path)==previous,"Failed replacement destroyed preferences");
        auto pending=path;pending+=".pending-0";
        check(!std::filesystem::exists(pending),"Failed save left a temporary directory");
#endif
        {std::ofstream output(path);output<<"# comment\nfuture text value\ntheme_mode 9\ndsp_at_start -1\n";}
        const auto clamped=dps::loadPreferences(path);
        check(clamped.themeMode==2&&clamped.dspAtStart==0,"Legacy mode clamping changed");
        for(const char* malformed:{"theme_mode text\n","theme_mode 1 trailing\n","theme_mode 1\ntheme_mode 2\n"}) {
            {std::ofstream output(path);output<<malformed;}
            failed=false;try {(void)dps::loadPreferences(path);}catch(const std::exception&) {failed=true;}
            check(failed,"Malformed preferences accepted");
        }
        std::filesystem::remove(path);
        std::cout<<"PASS: preferences roundtrip, defaults, forward compatibility, malformed input and failed replacement\n";
    } catch(const std::exception& error) {
        std::filesystem::remove(path);std::cerr<<error.what()<<'\n';return 1;
    }
}
