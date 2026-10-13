#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include "dps/profile.hpp"
#include <filesystem>
#include <fstream>
#include <iostream>
#include <stdexcept>
#include <string>
namespace fs=std::filesystem;
void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
struct WindowSearch {DWORD pid;HWND window=nullptr;};
BOOL CALLBACK findWindow(HWND window,LPARAM value) {
    auto& search=*reinterpret_cast<WindowSearch*>(value);DWORD pid=0;
    GetWindowThreadProcessId(window,&pid);wchar_t cls[64]{};GetClassNameW(window,cls,64);
    if(pid==search.pid&&!lstrcmpW(cls,L"Resoniq.Gui")) search.window=window;
    return TRUE;
}
struct Fixture {
    PROCESS_INFORMATION process{};HWND window=nullptr;fs::path folder,data;
    Fixture(const fs::path& source,const fs::path& destination,const fs::path& profile):folder(destination),data(folder/L"data") {
        fs::create_directories(folder/L"profiles");fs::create_directories(data);
        const auto exe=folder/L"resoniq.exe";fs::copy_file(source,exe,fs::copy_options::overwrite_existing);
        std::ofstream(folder/L"installed.marker")<<"isolated shutdown fixture\n";
        std::wstring command=L"\""+exe.wstring()+L"\" --tray --data-dir \""+data.wstring()+L"\" --profile \""+profile.wstring()+L"\"";
        STARTUPINFOW startup{};startup.cb=sizeof(startup);startup.dwFlags=STARTF_USESHOWWINDOW;startup.wShowWindow=SW_HIDE;
        check(CreateProcessW(exe.c_str(),command.data(),nullptr,nullptr,FALSE,0,nullptr,folder.c_str(),&startup,&process)!=FALSE,"Cannot start isolated fixture");
        for(unsigned i=0;i<100&&!window;++i) {
            WindowSearch search{process.dwProcessId};EnumWindows(findWindow,reinterpret_cast<LPARAM>(&search));window=search.window;
            if(!window) Sleep(50);
        }
        check(window!=nullptr,"Fixture window missing");
    }
    ~Fixture() {
        if(process.hProcess&&WaitForSingleObject(process.hProcess,0)==WAIT_TIMEOUT&&window) {
            PostMessageW(window,WM_COMMAND,304,0);WaitForSingleObject(process.hProcess,5000);
        }
        if(process.hThread) CloseHandle(process.hThread);if(process.hProcess) CloseHandle(process.hProcess);
    }
    bool alive() const {return WaitForSingleObject(process.hProcess,0)==WAIT_TIMEOUT;}
};
int wmain(int argc,wchar_t** argv) {
    if(argc!=6) {std::cerr<<"Usage: shutdown_tests MSI close.dll new.exe legacy.exe work-directory\n";return 2;}
    try {
        const auto work=fs::absolute(argv[5]);fs::create_directories(work);
        dps::Profile profile;profile.outputId="missing-isolated-shutdown-output";
        const auto profilePath=work/L"fixture.dps";dps::saveProfile(profilePath,profile);
        // A missing saved output prevents either fixture from opening audio.
        Fixture legacy(fs::absolute(argv[4]),work/L"legacy",profilePath);
        Fixture current(fs::absolute(argv[3]),work/L"current",profilePath);
        DWORD_PTR response=0;
        check(SendMessageTimeoutW(current.window,WM_QUERYENDSESSION,0,ENDSESSION_CLOSEAPP,
            SMTO_ABORTIFHUNG,3000,&response)&&response,"Restart Manager query rejected");
        SendMessageTimeoutW(current.window,WM_ENDSESSION,FALSE,ENDSESSION_CLOSEAPP,SMTO_ABORTIFHUNG,3000,&response);
        check(current.alive(),"Cancelled shutdown exits application");
        SendMessageTimeoutW(legacy.window,WM_CLOSE,0,0,SMTO_ABORTIFHUNG,3000,&response);
        check(legacy.alive(),"Legacy fixture does not reproduce close-to-tray");
        MsiSetInternalUI(INSTALLUILEVEL_NONE,nullptr);
        MSIHANDLE session=0;
        check(MsiOpenPackageExW(fs::absolute(argv[1]).c_str(),MSIOPENPACKAGEFLAGS_IGNOREMACHINESTATE,&session)==ERROR_SUCCESS,"Cannot open MSI session");
        const auto dll=LoadLibraryW(fs::absolute(argv[2]).c_str());check(dll!=nullptr,"Cannot load close action");
        const auto close=reinterpret_cast<UINT(__stdcall*)(MSIHANDLE)>(GetProcAddress(dll,"CloseResoniq"));
        check(close!=nullptr,"Close action export missing");
        MsiSetPropertyW(session,L"INSTALLFOLDER",legacy.folder.c_str());
        const auto status=close(session);FreeLibrary(dll);MsiCloseHandle(session);
        check(status==ERROR_SUCCESS&&!legacy.alive(),"Installer cannot close released legacy application");
        check(current.alive(),"Installer closed another copy outside target folder");
        check(fs::exists(legacy.data/L"config/session.user.dps"),"Legacy exit did not save isolated session");
        SendMessageTimeoutW(current.window,WM_ENDSESSION,TRUE,ENDSESSION_CLOSEAPP,SMTO_ABORTIFHUNG,3000,&response);
        check(WaitForSingleObject(current.process.hProcess,5000)==WAIT_OBJECT_0,"Confirmed Restart Manager shutdown fails");
        check(fs::exists(current.data/L"config/session.user.dps"),"Restart Manager exit did not save session");
        std::cout<<"PASS: legacy close-to-tray, installer graceful Exit, exact path scope, saved sessions, cancelled and confirmed Restart Manager shutdown; isolated copies only\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
