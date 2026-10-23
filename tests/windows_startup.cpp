#include "../apps/windows_startup.hpp"
#include <iostream>

using resoniq::startup::Repair;
void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}

// The decision table on its own: no registry, no files, no machine state.
void policy() {
    using resoniq::startup::decide;
    const std::wstring installed=L"C:\\Program Files\\RESONIQ\\resoniq.exe";
    const std::wstring build=L"D:\\source\\build\\resoniq.exe";
    const auto entry=resoniq::startup::command(installed);
    check(decide(false,L"",installed,false,true)==Repair::None,"Disabled preference registers");
    check(decide(false,entry,installed,true,true)==Repair::None,"Disabled preference rewrites");
    check(decide(true,L"",installed,false,true)==Repair::Registered,"Missing entry not registered");
    check(decide(true,entry,installed,true,true)==Repair::None,"Valid entry rewritten");
    // Same file, different spelling: Windows does not care about case, and
    // neither should a repair that would otherwise run at every start.
    check(decide(true,resoniq::startup::command(L"C:\\PROGRAM FILES\\RESONIQ\\resoniq.exe"),installed,true,true)==Repair::None,
        "Case difference treated as a different application");
    // The tray flag is what keeps sign-in from opening a window.
    check(decide(true,L"\""+installed+L"\"",installed,true,true)==Repair::Retargeted,"Entry without the tray flag kept");
    check(decide(true,resoniq::startup::command(L"C:\\old\\resoniq.exe"),installed,false,true)==Repair::Retargeted,
        "Entry pointing nowhere not repaired");
    check(decide(true,resoniq::startup::command(build),installed,true,true)==Repair::Retargeted,
        "Installed copy cannot reclaim sign-in");
    check(decide(true,resoniq::startup::command(installed),build,true,false)==Repair::Foreign,
        "A build run from the source tree takes sign-in over");
}
void parsing() {
    using resoniq::startup::target;
    check(target(L"\"C:\\Program Files\\RESONIQ\\resoniq.exe\" --tray")==L"C:\\Program Files\\RESONIQ\\resoniq.exe",
        "Quoted path not read back");
    check(target(L"C:\\RESONIQ\\resoniq.exe --tray")==L"C:\\RESONIQ\\resoniq.exe","Unquoted path not read back");
    check(target(L"C:\\RESONIQ\\resoniq.exe")==L"C:\\RESONIQ\\resoniq.exe","Bare path not read back");
    check(target(L"").empty(),"Empty command invented a path");
}
// A disposable key: never change the user's real startup registration.
void runValue(const std::wstring& key) {
    using namespace resoniq::startup;
    const std::wstring installed=L"C:\\Program Files\\RESONIQ\\resoniq.exe";
    check(!matches(installed,key.c_str()),"Missing entry accepted");
    check(reconcile(false,installed,key.c_str())==Repair::None,"Disabled preference repaired");
    check(!matches(installed,key.c_str()),"Disabled preference left an entry");
    check(reconcile(true,installed,key.c_str())==Repair::Registered,"Missing entry not repaired");
    check(matches(installed,key.c_str()),"Quoted tray command not registered");
    check(reconcile(true,installed,key.c_str())==Repair::None,"Valid entry rewritten");
    apply(true,L"C:\\old\\resoniq.exe",key.c_str());
    check(!matches(installed,key.c_str()),"Stale executable accepted");
    check(reconcile(true,installed,key.c_str())==Repair::Retargeted,"Stale executable not repaired");
    check(matches(installed,key.c_str()),"Repair left the stale command");
    // An entry owned by a copy that is still on disk stays untouched when the
    // running application is not the installed one.
    wchar_t self[MAX_PATH]{};GetModuleFileNameW(nullptr,self,MAX_PATH);
    apply(true,self,key.c_str());
    check(reconcile(true,L"D:\\source\\build\\resoniq.exe",key.c_str())==Repair::Foreign,
        "A build run from the source tree hijacked the entry");
    check(matches(self,key.c_str()),"Foreign entry was rewritten anyway");
    apply(false,installed,key.c_str());
    check(!matches(installed,key.c_str()),"Disable left entry behind");
    apply(false,installed,key.c_str());
}
void shortcut(const std::filesystem::path& folder) {
    using namespace resoniq::startup;
    const std::wstring installed=L"C:\\Program Files\\RESONIQ\\resoniq.exe";
    const auto file=folder/shortcutName;
    check(registeredShortcut(folder).empty(),"Missing shortcut invented a target");
    check(reconcileShortcut(false,installed,folder)==Repair::None,"Disabled preference created a shortcut");
    check(!std::filesystem::exists(file),"Disabled preference left a shortcut");
    check(reconcileShortcut(true,installed,folder)==Repair::Registered,"Missing shortcut not created");
    check(std::filesystem::exists(file),"Shortcut was not written");
    check(sameText(registeredShortcut(folder),command(installed)),"Shortcut does not carry the quoted tray command");
    check(reconcileShortcut(true,installed,folder)==Repair::None,"Valid shortcut rewritten");
    applyShortcut(true,L"C:\\old\\resoniq.exe",folder);
    check(reconcileShortcut(true,installed,folder)==Repair::Retargeted,"Stale shortcut not repaired");
    check(sameText(registeredShortcut(folder),command(installed)),"Repair left the stale shortcut");
    applyShortcut(false,installed,folder);
    check(!std::filesystem::exists(file),"Disable left the shortcut behind");
    applyShortcut(false,installed,folder);
}
// Which copy sign-in should point at. Whether this machine has RESONIQ
// installed decides the answer, so both outcomes are accepted and each one is
// checked for what it has to guarantee.
void preference() {
    using namespace resoniq::startup;
    const std::wstring build=L"D:\\source\\build\\resoniq.exe";
    const auto installed=installedCopy();
    if(installed.empty()) {
        check(preferredCopy(build)==build,"Without an installation the running copy is not used");
        return;
    }
    check(std::filesystem::exists(std::filesystem::path(installed)),"Recorded installation does not exist");
    check(sameText(std::filesystem::path(installed).filename().wstring(),L"resoniq.exe"),
        "Recorded installation is not the application");
    check(preferredCopy(build)==installed,"A build run by hand would claim sign-in");
    check(installedLocation(installed),"Installed copy not recognised as installed");
}
int main() {
    const auto key=L"Software\\RESONIQ\\StartupTest-"+std::to_wstring(GetCurrentProcessId());
    const auto folder=std::filesystem::temp_directory_path()/
        (L"resoniq-startup-"+std::to_wstring(GetCurrentProcessId()));
    auto clean=[&]{
        RegDeleteKeyW(HKEY_CURRENT_USER,key.c_str());
        std::error_code code;std::filesystem::remove_all(folder,code);
    };
    try {
        policy();parsing();preference();runValue(key);shortcut(folder);clean();
        std::cout<<"PASS: repair policy, command parsing, installed copy preference, "
                   "Run value and Startup folder anchors\n";
    } catch(const std::exception& error) {
        clean();std::cerr<<error.what()<<'\n';return 1;
    }
}
