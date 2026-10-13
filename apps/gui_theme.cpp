#include "gui_theme.hpp"
namespace resoniq::ui {
// Windows exposes the user's app theme here; absent on older builds, where the
// sensible answer is the dark look the app has always had.
bool systemPrefersLight() {
    DWORD value=0,size=sizeof(value);
    if(RegGetValueW(HKEY_CURRENT_USER,L"Software\\Microsoft\\Windows\\CurrentVersion\\Themes\\Personalize",
        L"AppsUseLightTheme",RRF_RT_REG_DWORD,nullptr,&value,&size)!=ERROR_SUCCESS) return false;
    return value!=0;
}
}
