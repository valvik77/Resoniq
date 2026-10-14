#pragma once
#include <algorithm>
#include <cmath>
#include <string>
namespace dps {
// What Windows itself reports for a render endpoint: the scalar its volume
// slider shows, in [0,1], and the mute flag. A negative volume means the
// endpoint did not answer, which is not the same as silence.
struct EndpointLevel {float volume=-1;bool muted=false;};
// The window shows the same whole percentage as the system slider instead of
// a dB figure: the user compares the two readings directly while listening to
// DSP on against DSP off, and no volume is matched for them anywhere.
inline std::wstring volumeCaption(const EndpointLevel& level) {
    if(!std::isfinite(level.volume)||level.volume<0) return L"Volumen de Windows sin dato";
    const auto percent=std::lround(std::clamp(level.volume,0.f,1.f)*100);
    const auto value=std::to_wstring(percent)+L" %";
    return level.muted?L"Volumen de Windows silenciado ("+value+L")":L"Volumen de Windows "+value;
}
}
