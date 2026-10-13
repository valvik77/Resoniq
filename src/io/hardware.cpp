#include "dps/hardware.hpp"
#include "dps/profile.hpp"
#include <windows.h>
#include <algorithm>
#include <array>
#include <cctype>
namespace dps {
namespace {
std::string lower(std::string value) {
    std::transform(value.begin(),value.end(),value.begin(),
        [](unsigned char c){return static_cast<char>(std::tolower(c));});
    return value;
}
bool contains(const std::string& haystack,std::string_view needle) {
    return haystack.find(needle)!=std::string::npos;
}
// Firmware strings, read from the registry rather than WMI: no COM, no service
// dependency, and it is the same data the BIOS reports.
std::string systemString(const wchar_t* value) {
    wchar_t buffer[256]{};DWORD size=sizeof(buffer);
    if(RegGetValueW(HKEY_LOCAL_MACHINE,L"HARDWARE\\DESCRIPTION\\System\\BIOS",value,
        RRF_RT_REG_SZ,nullptr,buffer,&size)!=ERROR_SUCCESS) return {};
    const int needed=WideCharToMultiByte(CP_UTF8,0,buffer,-1,nullptr,0,nullptr,nullptr);
    if(needed<=1) return {};
    std::string out(static_cast<std::size_t>(needed)-1,0);
    WideCharToMultiByte(CP_UTF8,0,buffer,-1,out.data(),needed,nullptr,nullptr);
    return out;
}
}
Hardware detectHardware(const std::wstring& endpointName) {
    Hardware h;
    h.manufacturer=systemString(L"SystemManufacturer");
    h.model=systemString(L"SystemProductName");
    h.family=systemString(L"SystemFamily");
    if(!endpointName.empty()) {
        const int needed=WideCharToMultiByte(CP_UTF8,0,endpointName.c_str(),-1,nullptr,0,nullptr,nullptr);
        if(needed>1) {
            h.endpoint.resize(static_cast<std::size_t>(needed)-1);
            WideCharToMultiByte(CP_UTF8,0,endpointName.c_str(),-1,h.endpoint.data(),needed,nullptr,nullptr);
        }
    }
    // A battery means a portable chassis. Simpler and more reliable than the
    // chassis type, which many firmwares fill in wrongly.
    SYSTEM_POWER_STATUS power{};
    if(GetSystemPowerStatus(&power)) h.portable=power.BatteryFlag!=128&&power.BatteryFlag!=255;
    const std::string endpoint=lower(h.endpoint);
    for(const auto* hint:{"auricular","headphone","headset","earphone","cascos"})
        if(contains(endpoint,hint)) h.headphones=true;
    return h;
}
}
