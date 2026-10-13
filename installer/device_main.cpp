#include <windows.h>
#include <setupapi.h>
#include <newdev.h>
#include <cfgmgr32.h>
#include <wintrust.h>
#include <softpub.h>
#include <array>
#include <filesystem>
#include <iostream>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
constexpr wchar_t hardware[]=L"ROOT\\DPSAUDIOVIRTUAL";
constexpr GUID media={0x4d36e96c,0xe325,0x11ce,{0xbf,0xc1,0x08,0x00,0x2b,0xe1,0x03,0x18}};
void require(bool ok,const char* message){if(!ok) throw std::runtime_error(std::string(message)+" (Windows "+std::to_string(GetLastError())+")");}
struct Devices {
    HDEVINFO handle;
    explicit Devices(bool enumerate):handle(enumerate?SetupDiGetClassDevsW(nullptr,nullptr,nullptr,DIGCF_ALLCLASSES):SetupDiCreateDeviceInfoList(&media,nullptr)) {
        require(handle!=INVALID_HANDLE_VALUE,"Device list");
    }
    ~Devices(){SetupDiDestroyDeviceInfoList(handle);}
};
bool isOurs(HDEVINFO set,SP_DEVINFO_DATA& item) {
    std::array<wchar_t,8192> ids{};DWORD type=0,bytes=0;
    if(!SetupDiGetDeviceRegistryPropertyW(set,&item,SPDRP_HARDWAREID,&type,
        reinterpret_cast<BYTE*>(ids.data()),static_cast<DWORD>(sizeof(ids)),&bytes)) {
        const auto error=GetLastError();
        if(error==ERROR_INVALID_DATA||error==ERROR_INSUFFICIENT_BUFFER) return false;
        require(false,"Read hardware ID");
    }
    if(type!=REG_MULTI_SZ||bytes>sizeof(ids)||bytes%sizeof(wchar_t)) return false;
    const auto count=bytes/sizeof(wchar_t);
    for(std::size_t begin=0;begin<count&&ids[begin];) {
        auto end=begin;while(end<count&&ids[end]) ++end;
        if(end==count) return false;
        if(_wcsicmp(ids.data()+begin,hardware)==0) return true;
        begin=end+1;
    }
    return false;
}
std::vector<SP_DEVINFO_DATA> ours(Devices& devices) {
    std::vector<SP_DEVINFO_DATA> found;
    for(DWORD i=0;;++i){SP_DEVINFO_DATA item{};item.cbSize=sizeof(item);
        if(!SetupDiEnumDeviceInfo(devices.handle,i,&item)) {
            if(GetLastError()==ERROR_NO_MORE_ITEMS) break;
            require(false,"Enumerate devices");
        }
        if(isOurs(devices.handle,item)) found.push_back(item);
    }
    return found;
}
bool administrator(){SID_IDENTIFIER_AUTHORITY authority=SECURITY_NT_AUTHORITY;PSID sid=nullptr;BOOL member=FALSE;
    require(AllocateAndInitializeSid(&authority,2,SECURITY_BUILTIN_DOMAIN_RID,DOMAIN_ALIAS_RID_ADMINS,0,0,0,0,0,0,&sid)!=FALSE,"Administrator SID");
    const BOOL ok=CheckTokenMembership(nullptr,sid,&member);FreeSid(sid);require(ok!=FALSE,"Administrator check");return member!=FALSE;
}
void verifyCatalog(const std::filesystem::path& catalog){
    WINTRUST_FILE_INFO file{};file.cbStruct=sizeof(file);file.pcwszFilePath=catalog.c_str();
    WINTRUST_DATA data{};data.cbStruct=sizeof(data);data.dwUIChoice=WTD_UI_NONE;data.fdwRevocationChecks=WTD_REVOKE_NONE;
    data.dwUnionChoice=WTD_CHOICE_FILE;data.pFile=&file;data.dwStateAction=WTD_STATEACTION_VERIFY;
    GUID policy=WINTRUST_ACTION_GENERIC_VERIFY_V2;
    const LONG result=WinVerifyTrust(nullptr,&policy,&data);
    data.dwStateAction=WTD_STATEACTION_CLOSE;WinVerifyTrust(nullptr,&policy,&data);
    if(result!=ERROR_SUCCESS) throw std::runtime_error("Driver catalog is not trusted: "+std::to_string(result));
}
int install(const std::filesystem::path& directory){
    require(administrator(),"Run driver installation as administrator");
    const auto inf=std::filesystem::absolute(directory/L"DpsAudioVirtual.inf");
    verifyCatalog(directory/L"DpsAudioVirtual.cat");
    GUID klass{};std::array<wchar_t,256> name{};
    require(SetupDiGetINFClassW(inf.c_str(),&klass,name.data(),static_cast<DWORD>(name.size()),nullptr)!=FALSE,"Read INF");
    if(!IsEqualGUID(klass,media)) throw std::runtime_error("Expected MEDIA driver class");
    Devices all(true);const auto existing=ours(all);
    if(existing.size()>1) throw std::runtime_error("Multiple DPS devices already exist; resolve duplicates before installing");
    BOOL reboot=FALSE;
    // Stage/check package before creating a root node. Windows enforces kernel
    // signing and catalog membership; we never disable its security policy.
    require(DiInstallDriverW(nullptr,inf.c_str(),0,&reboot)!=FALSE,"Stage driver");
    Devices created(false);SP_DEVINFO_DATA item{};item.cbSize=sizeof(item);bool registered=false;
    try {
        if(existing.empty()) {
            require(SetupDiCreateDeviceInfoW(created.handle,L"DPSAUDIOVIRTUAL",&media,L"DPS Audio Virtual Transport",nullptr,DICD_GENERATE_ID,&item)!=FALSE,"Create DPS device");
            const wchar_t ids[]=L"ROOT\\DPSAUDIOVIRTUAL\0";
            require(SetupDiSetDeviceRegistryPropertyW(created.handle,&item,SPDRP_HARDWAREID,
                reinterpret_cast<const BYTE*>(ids),sizeof(ids))!=FALSE,"Set DPS hardware ID");
            require(SetupDiCallClassInstaller(DIF_REGISTERDEVICE,created.handle,&item)!=FALSE,"Register DPS device");registered=true;
        }
        BOOL updateReboot=FALSE;
        if(!UpdateDriverForPlugAndPlayDevicesW(nullptr,hardware,inf.c_str(),0,&updateReboot)) {
            if(GetLastError()!=ERROR_NO_MORE_ITEMS) require(false,"Install DPS driver");
        }
        reboot=reboot||updateReboot;
        Devices current(true);auto installed=ours(current);
        if(installed.size()!=1) throw std::runtime_error("DPS device was not created uniquely");
        ULONG status=0,problem=0;
        const auto result=CM_Get_DevNode_Status(&status,&problem,installed[0].DevInst,0);
        if(!reboot&&(result!=CR_SUCCESS||problem!=0)) throw std::runtime_error("DPS device has PnP error "+std::to_string(problem));
        std::cout<<"DPS device installed; reboot="<<(reboot?1:0)<<"\n";return reboot?3010:0;
    }catch(...){
        // Only remove a node created by this attempt. Existing devices survive.
        if(registered) SetupDiCallClassInstaller(DIF_REMOVE,created.handle,&item);
        throw;
    }
}
int remove(){require(administrator(),"Run device removal as administrator");Devices all(true);auto devices=ours(all);bool reboot=false;
    for(auto& item:devices){SP_REMOVEDEVICE_PARAMS parameters{};parameters.ClassInstallHeader.cbSize=sizeof(SP_CLASSINSTALL_HEADER);
        parameters.ClassInstallHeader.InstallFunction=DIF_REMOVE;parameters.Scope=DI_REMOVEDEVICE_GLOBAL;
        require(SetupDiSetClassInstallParamsW(all.handle,&item,&parameters.ClassInstallHeader,sizeof(parameters))!=FALSE,"Prepare removal");
        require(SetupDiCallClassInstaller(DIF_REMOVE,all.handle,&item)!=FALSE,"Remove DPS device");
        SP_DEVINSTALL_PARAMS_W flags{};flags.cbSize=sizeof(flags);
        if(SetupDiGetDeviceInstallParamsW(all.handle,&item,&flags)) reboot=reboot||(flags.Flags&(DI_NEEDREBOOT|DI_NEEDRESTART))!=0;
    }
    std::cout<<"Removed DPS nodes only; driver store retained; reboot="<<(reboot?1:0)<<"\n";return reboot?3010:0;
}
}
int wmain(int argc,wchar_t** argv){try {
    USHORT processMachine=0,nativeMachine=0;
    require(IsWow64Process2(GetCurrentProcess(),&processMachine,&nativeMachine)!=FALSE,"Read architecture");
    if(nativeMachine!=IMAGE_FILE_MACHINE_AMD64) throw std::runtime_error("Only native Windows x64 is supported");
    if(argc==2&&std::wstring_view(argv[1])==L"--check") {
        Devices all(true);auto devices=ours(all);
        std::cout<<"{\"administrator\":"<<(administrator()?"true":"false")<<",\"dps_devices\":"<<devices.size()<<",\"modified_system\":false}\n";return 0;
    }
    if(argc==3&&std::wstring_view(argv[1])==L"--install") return install(std::filesystem::absolute(argv[2]));
    if(argc==2&&std::wstring_view(argv[1])==L"--remove") return remove();
    std::cerr<<"Usage: resoniq_device --check | --install DRIVER_DIRECTORY | --remove\n";return 2;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
