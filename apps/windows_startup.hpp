#pragma once
#include <windows.h>
#include <shlobj.h>
#include <filesystem>
#include <string>
#include <vector>
#include <stdexcept>

// Two independent anchors register the app with Windows: the per-user Run
// value and a shortcut in the Startup folder. On at least one machine the Run
// value disappears while the app is closed, and a repair that only runs when
// the app starts cannot recover from that on its own. The saved preference is
// the request; both anchors are checked and restored at every start.
namespace resoniq::startup {
inline constexpr auto runKey=L"Software\\Microsoft\\Windows\\CurrentVersion\\Run";
inline constexpr auto entryName=L"RESONIQ";
inline constexpr auto shortcutName=L"RESONIQ.lnk";
inline constexpr auto trayFlag=L"--tray";

// Quoted so a path with spaces still starts, plus the flag that parks the app
// in the tray instead of opening a window over the sign-in.
inline std::wstring command(const std::wstring& executable) {
    return L"\""+executable+L"\" "+trayFlag;
}
inline bool sameText(const std::wstring& a,const std::wstring& b) {
    return a.size()==b.size()&&CompareStringOrdinal(a.c_str(),static_cast<int>(a.size()),
        b.c_str(),static_cast<int>(b.size()),TRUE)==CSTR_EQUAL;
}
// The executable a stored command refers to, without its quotes or flags.
inline std::wstring target(const std::wstring& command) {
    if(command.empty()) return {};
    if(command.front()==L'"') {
        const auto end=command.find(L'"',1);
        return end==std::wstring::npos?command.substr(1):command.substr(1,end-1);
    }
    const auto space=command.find(L' ');
    return space==std::wstring::npos?command:command.substr(0,space);
}
// Windows compares paths without case. Separators are normalised first so the
// same file written two ways is not mistaken for a different one.
inline bool samePath(const std::wstring& a,const std::wstring& b) {
    if(a.empty()||b.empty()) return false;
    return sameText(std::filesystem::path(a).lexically_normal().wstring(),
                    std::filesystem::path(b).lexically_normal().wstring());
}
// What should happen to the registration Windows currently holds.
enum class Repair {None,Registered,Retargeted,Foreign};
// The decision on its own, so the policy can be tested without touching the
// registry, the Startup folder or the disk.
inline Repair decide(bool requested,const std::wstring& stored,const std::wstring& running,
                     bool storedExists,bool runningIsInstalled) {
    if(!requested) return Repair::None;
    if(stored.empty()) return Repair::Registered;
    if(samePath(target(stored),running)) return sameText(stored,command(running))?Repair::None:Repair::Retargeted;
    // A registration pointing at a copy that is still there belongs to that
    // copy: a build run from the source tree must not take sign-in over from
    // the installed application. The installed copy may always claim it back.
    if(!storedExists) return Repair::Retargeted;
    return runningIsInstalled?Repair::Retargeted:Repair::Foreign;
}
inline bool installedLocation(const std::wstring& executable) {
    for(const auto& folder:{FOLDERID_ProgramFiles,FOLDERID_ProgramFilesX86}) {
        PWSTR expanded=nullptr;
        if(SHGetKnownFolderPath(folder,0,nullptr,&expanded)!=S_OK||!expanded) continue;
        const std::wstring prefix(expanded);CoTaskMemFree(expanded);
        if(!prefix.empty()&&executable.size()>prefix.size()&&sameText(executable.substr(0,prefix.size()),prefix))
            return true;
    }
    return false;
}

inline std::wstring stringValue(HKEY root,const wchar_t* subkey,const wchar_t* name) {
    DWORD size=0;
    if(RegGetValueW(root,subkey,name,RRF_RT_REG_SZ,nullptr,nullptr,&size)!=ERROR_SUCCESS) return {};
    std::wstring value(size/sizeof(wchar_t)+1,L'\0');
    size=static_cast<DWORD>(value.size()*sizeof(wchar_t));
    if(RegGetValueW(root,subkey,name,RRF_RT_REG_SZ,nullptr,value.data(),&size)!=ERROR_SUCCESS) return {};
    const auto end=value.find(L'\0');
    if(end!=std::wstring::npos) value.resize(end);
    return value;
}
// Where the installer put the application, when it is installed at all: the
// folder it recorded, or the default one. A copy built in the source tree must
// not hand sign-in to a file that the next build can delete, so sign-in is
// always registered for the installed copy when there is one.
inline std::wstring installedCopy() {
    std::vector<std::filesystem::path> folders;
    if(const auto recorded=stringValue(HKEY_LOCAL_MACHINE,L"Software\\RESONIQ",L"InstallFolder");!recorded.empty())
        folders.emplace_back(recorded);
    PWSTR expanded=nullptr;
    if(SHGetKnownFolderPath(FOLDERID_ProgramFiles,0,nullptr,&expanded)==S_OK&&expanded) {
        folders.emplace_back(std::filesystem::path(expanded)/L"RESONIQ");CoTaskMemFree(expanded);
    }
    for(const auto& folder:folders) {
        const auto candidate=folder/L"resoniq.exe";
        std::error_code code;
        if(std::filesystem::exists(candidate,code)&&!code) return candidate.wstring();
    }
    return {};
}
inline std::wstring preferredCopy(const std::wstring& running) {
    const auto installed=installedCopy();
    return installed.empty()?running:installed;
}

// ---- The Run value -------------------------------------------------------
inline std::wstring registered(const wchar_t* subkey=runKey) {
    return stringValue(HKEY_CURRENT_USER,subkey,entryName);
}
inline bool matches(const std::wstring& executable,const wchar_t* subkey=runKey) {
    return sameText(registered(subkey),command(executable));
}
inline void apply(bool enable,const std::wstring& executable,const wchar_t* subkey=runKey) {
    HKEY key{};
    if(RegCreateKeyExW(HKEY_CURRENT_USER,subkey,0,nullptr,0,KEY_SET_VALUE,nullptr,&key,nullptr)!=ERROR_SUCCESS)
        throw std::runtime_error("No se pudo acceder al inicio de Windows");
    const auto value=command(executable);
    auto result=enable?RegSetValueExW(key,entryName,0,REG_SZ,
        reinterpret_cast<const BYTE*>(value.c_str()),static_cast<DWORD>((value.size()+1)*sizeof(wchar_t))):
        RegDeleteValueW(key,entryName);
    RegCloseKey(key);
    if(!enable&&result==ERROR_FILE_NOT_FOUND) result=ERROR_SUCCESS;
    if(result!=ERROR_SUCCESS) throw std::runtime_error("No se pudo cambiar el inicio con Windows");
    if(enable&&!matches(executable,subkey))
        throw std::runtime_error("No se pudo verificar el inicio con Windows");
}
inline Repair reconcile(bool requested,const std::wstring& executable,const wchar_t* subkey=runKey) {
    const auto stored=registered(subkey);
    std::error_code code;
    const auto decision=decide(requested,stored,executable,
        std::filesystem::exists(std::filesystem::path(target(stored)),code)&&!code,
        installedLocation(executable));
    if(decision==Repair::Registered||decision==Repair::Retargeted) apply(true,executable,subkey);
    return decision;
}

// ---- The Startup folder shortcut -----------------------------------------
namespace detail {
// COM is already running on the audio side; initialising per call and only
// closing what this call opened keeps the two apartments from interfering.
struct Apartment {
    HRESULT opened=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    Apartment()=default;
    ~Apartment(){if(opened==S_OK||opened==S_FALSE) CoUninitialize();}
    Apartment(const Apartment&)=delete;Apartment& operator=(const Apartment&)=delete;
};
template<class T> struct Handle {
    T* pointer=nullptr;
    Handle()=default;
    ~Handle(){if(pointer) pointer->Release();}
    T* operator->() const {return pointer;}
    Handle(const Handle&)=delete;Handle& operator=(const Handle&)=delete;
};
inline void shellLink(Handle<IShellLinkW>& link) {
    if(CoCreateInstance(CLSID_ShellLink,nullptr,CLSCTX_INPROC_SERVER,IID_IShellLinkW,
        reinterpret_cast<void**>(&link.pointer))!=S_OK||!link.pointer)
        throw std::runtime_error("No se pudo preparar el acceso directo de inicio");
}
}
inline std::filesystem::path startupFolder() {
    PWSTR expanded=nullptr;
    if(SHGetKnownFolderPath(FOLDERID_Startup,0,nullptr,&expanded)!=S_OK||!expanded)
        throw std::runtime_error("No se pudo localizar la carpeta de inicio de Windows");
    std::filesystem::path folder(expanded);CoTaskMemFree(expanded);return folder;
}
// Read back as a command line, so one comparison covers both anchors.
inline std::wstring registeredShortcut(const std::filesystem::path& folder) {
    const auto file=folder/shortcutName;
    std::error_code code;
    if(!std::filesystem::exists(file,code)||code) return {};
    detail::Apartment apartment;
    detail::Handle<IShellLinkW> link;detail::shellLink(link);
    detail::Handle<IPersistFile> persist;
    if(link.pointer->QueryInterface(IID_IPersistFile,reinterpret_cast<void**>(&persist.pointer))!=S_OK) return {};
    if(persist->Load(file.c_str(),STGM_READ)!=S_OK) return {};
    wchar_t path[MAX_PATH]{},arguments[MAX_PATH]{};
    if(link->GetPath(path,MAX_PATH,nullptr,SLGP_RAWPATH)!=S_OK||!path[0]) return {};
    link->GetArguments(arguments,MAX_PATH);
    std::wstring stored=L"\""+std::wstring(path)+L"\"";
    if(arguments[0]) stored+=L" "+std::wstring(arguments);
    return stored;
}
inline void applyShortcut(bool enable,const std::wstring& executable,const std::filesystem::path& folder) {
    const auto file=folder/shortcutName;
    if(!enable) {
        std::error_code code;std::filesystem::remove(file,code);
        if(code) throw std::runtime_error("No se pudo retirar el acceso directo de inicio");
        return;
    }
    std::filesystem::create_directories(folder);
    detail::Apartment apartment;
    detail::Handle<IShellLinkW> link;detail::shellLink(link);
    const auto workingDirectory=std::filesystem::path(executable).parent_path().wstring();
    if(link->SetPath(executable.c_str())!=S_OK||link->SetArguments(trayFlag)!=S_OK||
       link->SetDescription(L"RESONIQ")!=S_OK||
       (!workingDirectory.empty()&&link->SetWorkingDirectory(workingDirectory.c_str())!=S_OK))
        throw std::runtime_error("No se pudo describir el acceso directo de inicio");
    detail::Handle<IPersistFile> persist;
    if(link.pointer->QueryInterface(IID_IPersistFile,reinterpret_cast<void**>(&persist.pointer))!=S_OK||
       persist->Save(file.c_str(),TRUE)!=S_OK)
        throw std::runtime_error("No se pudo guardar el acceso directo de inicio");
}
inline Repair reconcileShortcut(bool requested,const std::wstring& executable,const std::filesystem::path& folder) {
    const auto stored=registeredShortcut(folder);
    std::error_code code;
    const auto decision=decide(requested,stored,executable,
        std::filesystem::exists(std::filesystem::path(target(stored)),code)&&!code,
        installedLocation(executable));
    if(decision==Repair::Registered||decision==Repair::Retargeted) applyShortcut(true,executable,folder);
    return decision;
}
}
