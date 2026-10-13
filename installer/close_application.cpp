#include <windows.h>
#include <msi.h>
#include <msiquery.h>
#include <string>
#include <vector>

namespace {
struct Handle {
    HANDLE value=nullptr;
    ~Handle(){if(value) CloseHandle(value);}
};
std::wstring property(MSIHANDLE session,const wchar_t* name) {
    DWORD count=0;wchar_t empty=0;
    const auto result=MsiGetPropertyW(session,name,&empty,&count);
    if(result!=ERROR_MORE_DATA&&result!=ERROR_SUCCESS) throw result;
    std::wstring value(static_cast<std::size_t>(count)+1,L'\0');++count;
    if(MsiGetPropertyW(session,name,value.data(),&count)!=ERROR_SUCCESS) throw ERROR_INVALID_DATA;
    value.resize(count);return value;
}
std::wstring fullPath(const std::wstring& path) {
    std::wstring result(32768,L'\0');
    const auto count=GetFullPathNameW(path.c_str(),static_cast<DWORD>(result.size()),result.data(),nullptr);
    if(!count||count>=result.size()) throw ERROR_INVALID_DATA;
    result.resize(count);return result;
}
struct Context {std::wstring target;std::vector<HANDLE> processes;bool failed=false;};
BOOL CALLBACK closeWindow(HWND window,LPARAM parameter) {
    auto& context=*reinterpret_cast<Context*>(parameter);
    wchar_t cls[128]{};
    if(!GetClassNameW(window,cls,128)||lstrcmpW(cls,L"Resoniq.Gui")) return TRUE;
    DWORD pid=0;GetWindowThreadProcessId(window,&pid);
    Handle process;process.value=OpenProcess(PROCESS_QUERY_LIMITED_INFORMATION|SYNCHRONIZE,FALSE,pid);
    if(!process.value) return TRUE;
    wchar_t path[32768];DWORD length=32768;
    if(!QueryFullProcessImageNameW(process.value,0,path,&length)||lstrcmpiW(path,context.target.c_str())) return TRUE;
    // Stable tray Exit command in released 0.2.x builds: save the current
    // session and stop audio. WM_CLOSE alone merely parks old builds in tray.
    if(!PostMessageW(window,WM_COMMAND,304,0)) {context.failed=true;return TRUE;}
    try {context.processes.push_back(process.value);process.value=nullptr;}
    catch(...) {context.failed=true;return FALSE;}
    return TRUE;
}
void log(MSIHANDLE session,const wchar_t* text) {
    const auto record=MsiCreateRecord(0);
    if(record) {MsiRecordSetStringW(record,0,text);MsiProcessMessage(session,INSTALLMESSAGE_INFO,record);MsiCloseHandle(record);}
}
}
extern "C" __declspec(dllexport) UINT __stdcall CloseResoniq(MSIHANDLE session) noexcept {
    Context context;
    UINT result=ERROR_SUCCESS;
    try {
        auto folder=property(session,L"INSTALLFOLDER");
        if(folder.empty()) throw ERROR_INVALID_DATA;
        if(folder.back()!=L'\\'&&folder.back()!=L'/') folder+=L'\\';
        context.target=fullPath(folder+L"resoniq.exe");
        log(session,L"RESONIQ: requesting graceful exit of the application in INSTALLFOLDER.");
        if(!EnumWindows(closeWindow,reinterpret_cast<LPARAM>(&context))||context.failed) result=ERROR_INSTALL_FAILURE;
        const auto deadline=GetTickCount64()+15000;
        for(const auto process:context.processes) {
            const auto now=GetTickCount64();const DWORD timeout=now>=deadline?0:static_cast<DWORD>(deadline-now);
            if(WaitForSingleObject(process,timeout)!=WAIT_OBJECT_0) result=ERROR_INSTALL_FAILURE;
        }
    } catch(...) {result=ERROR_INSTALL_FAILURE;}
    for(const auto process:context.processes) CloseHandle(process);
    if(result!=ERROR_SUCCESS) {
        log(session,L"RESONIQ: graceful exit failed. Close RESONIQ manually and retry; no process was terminated forcibly.");
        const auto record=MsiCreateRecord(0);
        if(record) {
            MsiRecordSetStringW(record,0,L"No se pudo cerrar RESONIQ. Sal de RESONIQ desde la bandeja y vuelve a instalar.");
            MsiProcessMessage(session,INSTALLMESSAGE_ERROR,record);MsiCloseHandle(record);
        }
    }
    return result;
}
