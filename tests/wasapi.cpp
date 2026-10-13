#include "dps/wasapi.hpp"
#include "../src/audio/capture_timeline.hpp"
#include <windows.h>
#include <objbase.h>
#include <functional>
#include <iostream>
#include <limits>
#include <stdexcept>
namespace {
void reject(const std::function<void()>& action) {
    bool failed=false;try {action();} catch(const std::exception&) {failed=true;}
    if(!failed) throw std::runtime_error("Invalid measurement accepted");
}
}
int main() {
    // Deliberately use STA, as a UI may after a file dialog. No microphone is
    // opened: validation and a nonexistent output fail before capture begins.
    const HRESULT hr=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED);
    if(FAILED(hr)) return 1;
    int status=0;
    try {
        dps::detail::CaptureTimeline timeline;
        if(timeline.packet(0,480,true).missing||timeline.packet(480,480,true).missing)
            throw std::runtime_error("Contiguous packets reported as lost samples");
        if(timeline.packet(1200,480,true).missing!=240)
            throw std::runtime_error("Real missing capture frames not detected");
        if(!timeline.packet(0,480,true).reset) throw std::runtime_error("Clock reset not detected");
        timeline.packet(99999,480,false);
        if(timeline.packet(5000,480,true).missing) throw std::runtime_error("Invalid timestamp produced false loss");
        reject([]{(void)dps::captureMeasurement(L"missing",L"microphone",{});});
        reject([]{(void)dps::captureMeasurement(L"missing",L"microphone",{.1f});});
        reject([]{(void)dps::captureMeasurement(L"same",L"same",{.1f,.1f});});
        reject([]{(void)dps::captureMeasurement(L"missing",L"microphone",{std::numeric_limits<float>::quiet_NaN(),0});});
        reject([]{(void)dps::captureMeasurement(L"missing",L"microphone",{2,2});});
        reject([]{(void)dps::captureMeasurement(L"missing",L"microphone",{.1f,.1f});});
        // Enumeration must still accept the caller's existing COM apartment.
        (void)dps::endpoints();
        std::cout<<"PASS: STA enumeration, invalid measurement signal and missing endpoint errors; no microphone opened\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';status=1;}
    CoUninitialize();return status;
}
