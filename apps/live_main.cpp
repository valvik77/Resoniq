#include "dps/wasapi.hpp"
#include "dps/profile.hpp"
#include <conio.h>
#include <chrono>
#include <exception>
#include <iostream>
#include <iomanip>
#include <thread>
#include <algorithm>
#include <fstream>
#include <cmath>
#include <windows.h>
#include <charconv>
namespace {
std::string endpointText(const std::wstring& text) {
    std::string result;result.reserve(text.size());
    for(wchar_t c:text) {if(c>127) throw std::runtime_error("Non-ASCII endpoint identifier");result.push_back(static_cast<char>(c));}
    return result;
}
float percent(const std::string& value) {
    std::size_t used=0;const float number=std::stof(value,&used);
    if(used!=value.size()||!std::isfinite(number)||number<0||number>100) throw std::runtime_error("Controls must be 0..100");
    return number;
}
int integer(const std::string& value) {
    int result=0;const auto parsed=std::from_chars(value.data(),value.data()+value.size(),result);
    if(parsed.ec!=std::errc{}||parsed.ptr!=value.data()+value.size()||result<0)
        throw std::runtime_error("Expected a non-negative integer");
    return result;
}
void usage(){std::cout<<"dps_live --list\n"
    "dps_live --tone --render INDEX [--seconds N]\n"
    "dps_live --capture INDEX --render INDEX [--preset Enhanced]\n"
    "dps_live --loopback INDEX --render INDEX [--preset \"Laptop Max\"]\n"
    "dps_live --dps --render INDEX [--raw]\n"
    "Persistent IDs: --render-id ID, --loopback-id ID, --capture-id ID\n"
    "Optional: --output-volume 0..100 sets physical output volume.\n"
    "--raw skips system effects on the output stream.\n"
    "Profiles: --profile FILE, --device-profile Laptop|Laptop Focus|Laptop Impact|Laptop Impact+|Headphones|External\n"
    "--export-profile FILE saves and exits; --name NAME; controls 0..100:\n"
    "--bass --clarity --dynamics --spatial --intensity\n"
    "Keys: B bypass, +/- intensity, S save profile, Q quit. Route applications to virtual input first.\n";}
}
int main(int argc,char** argv) {
    try {
        dps::Profile profile;std::string profilePath,exportPath;
        for(int i=1;i<argc;++i) if(std::string_view(argv[i])=="--profile") {
            if(!profilePath.empty()||i+1>=argc) throw std::runtime_error("Select one profile file");
            profilePath=argv[++i];profile=dps::loadProfile(profilePath);
        }
        // Read-only: the enumeration figure beside the direct read the window
        // now shows each second, so any disagreement is visible here.
        if(argc==2&&std::string_view(argv[1])=="--status") {for(int k=0;k<5;++k) {for(const auto& d:dps::endpoints()) if(!d.capture) std::wcout<<(d.defaultRender?L"DEFAULT ":L"        ")<<d.name<<L" peak="<<d.peak<<L" volume="<<d.volume*100<<L"% muted="<<d.muted<<L" | "<<dps::volumeCaption(dps::endpointLevel(d.id))<<L'\n';std::this_thread::sleep_for(std::chrono::seconds(1));}return 0;}
        auto devices=dps::endpoints();if(argc==1||std::string_view(argv[1])=="--list") {for(std::size_t i=0;i<devices.size();++i) {std::wcout<<i<<L" ["<<(devices[i].capture?L"capture":L"render")<<L"] "<<devices[i].name<<L"\n    "<<devices[i].id<<L'\n';}usage();return 0;}
        dps::LiveOptions options;int input=-1,output=-1,seconds=0;std::string preset=profile.presetName,logPath;float outputVolume=-1;bool selected=false,ownDevice=false;options.raw=profile.raw;options.parameters=profile.parameters;
        auto byId=[&](const std::string& value) {std::wstring id(value.begin(),value.end());auto found=std::find_if(devices.begin(),devices.end(),[&](const auto& d){return d.id==id;});if(found==devices.end()) throw std::runtime_error("Endpoint ID not found; run --list");return static_cast<int>(found-devices.begin());};
        for(int i=1;i<argc;++i) {std::string arg=argv[i];if(arg=="--tone") {options.tone=true;continue;}if(arg=="--raw") {options.raw=true;continue;}if(arg=="--system-effects") {options.raw=false;continue;}if(arg=="--dps") {if(selected) throw std::runtime_error("Choose one input mode");selected=true;ownDevice=true;options.loopback=true;continue;}if(i+1>=argc) throw std::runtime_error("Missing option value");std::string value=argv[++i];
            if(arg=="--render") output=integer(value);
            else if(arg=="--render-id") output=byId(value);
            else if(arg=="--loopback-id"||arg=="--capture-id") {if(selected) throw std::runtime_error("Choose one input mode");selected=true;input=byId(value);options.loopback=arg=="--loopback-id";}
            else if(arg=="--capture"||arg=="--loopback") {if(selected) throw std::runtime_error("Choose one input mode");selected=true;input=integer(value);options.loopback=arg=="--loopback";}
            else if(arg=="--profile") {} // Loaded before overrides, regardless of argument order.
            else if(arg=="--export-profile") exportPath=value;
            else if(arg=="--name") profile.name=value;
            else if(arg=="--device-profile") {profile.deviceName=value;options.parameters.device=dps::deviceCalibration(value);}
            else if(arg=="--bass"||arg=="--clarity"||arg=="--dynamics"||arg=="--spatial"||arg=="--intensity") {
                const float number=percent(value);
                auto& p=options.parameters;float* target=arg=="--bass"?&p.bass:arg=="--clarity"?&p.clarity:arg=="--dynamics"?&p.dynamics:arg=="--spatial"?&p.spatial:&p.intensity;*target=number/100;
            }
            else if(arg=="--preset") {preset=value;const auto p=dps::preset(value);options.parameters.bass=p.bass;options.parameters.clarity=p.clarity;options.parameters.dynamics=p.dynamics;options.parameters.spatial=p.spatial;}else if(arg=="--seconds") {seconds=integer(value);if(seconds<1) throw std::runtime_error("seconds must be positive");}
            else if(arg=="--output-volume") {outputVolume=percent(value);}
            else if(arg=="--log") logPath=value;
            else throw std::runtime_error("Unknown option");
        }
        profile.presetName=preset;profile.parameters=options.parameters;profile.raw=options.raw;
        if(!exportPath.empty()) {
            if(output>=0) {if(std::size_t(output)>=devices.size()||devices[output].capture) throw std::runtime_error("Invalid render endpoint");profile.outputId=endpointText(devices[output].id);}
            dps::saveProfile(exportPath,profile);std::cout<<"Perfil guardado: "<<exportPath<<'\n';return 0;
        }
        if(output<0&&!profile.outputId.empty()) output=byId(profile.outputId);
        if(ownDevice) {for(std::size_t i=0;i<devices.size();++i) if(!devices[i].capture&&devices[i].name.find(L"DPS Audio")!=std::wstring::npos) {if(input!=-1) throw std::runtime_error("Multiple DPS endpoints; select --loopback-id explicitly");input=static_cast<int>(i);}if(input==-1) throw std::runtime_error("DPS Audio virtual driver is not installed or its endpoint is inactive");}
        if(output<0||std::size_t(output)>=devices.size()||devices[output].capture) throw std::runtime_error("Select a render endpoint with --render");
        if(options.tone&&selected) throw std::runtime_error("Tone cannot have capture input");
        if(!options.tone) {if(input<0||std::size_t(input)>=devices.size()||devices[input].capture==options.loopback) throw std::runtime_error("Invalid capture/loopback endpoint");options.inputId=devices[input].id;}
        options.outputId=devices[output].id;if(!options.tone&&options.inputId==options.outputId) throw std::runtime_error("Input and output must differ");profile.outputId=endpointText(options.outputId);
        struct HostGuard {HANDLE handle=nullptr;~HostGuard(){if(handle) CloseHandle(handle);}} guard;
        if(!options.tone) {guard.handle=CreateMutexW(nullptr,FALSE,L"Local\\DpsAudioEngine.LiveHost");if(!guard.handle) throw std::runtime_error("Cannot create host guard");if(GetLastError()==ERROR_ALREADY_EXISTS) throw std::runtime_error("DPS audio host is already running; use its console");}
        if(outputVolume>=0) dps::setEndpointVolume(options.outputId,outputVolume/100.f);
        std::wcout<<L"Salida: "<<devices[output].name<<L" | Volumen fisico: "<<(outputVolume>=0?outputVolume:devices[output].volume*100)<<L"%\n";
        std::ofstream log;if(!logPath.empty()) {log.open(logPath);if(!log) throw std::runtime_error("Cannot open telemetry log");log<<"preset="<<preset<<"\n";}
        dps::LiveState state;state.bass=options.parameters.bass;state.clarity=options.parameters.clarity;state.dynamics=options.parameters.dynamics;state.spatial=options.parameters.spatial;state.enabled=options.parameters.enabled;state.intensity=options.parameters.intensity;std::atomic<bool> finished=false;std::exception_ptr failure;
        std::cout<<"48 kHz stereo float32. Preset: "<<preset<<" | Perfil: "<<profile.name<<" ("<<profile.deviceName<<")"<<" B=activar/desactivar | +/-=intensidad (10%) | S=guardar | Q=salir\n";
        std::jthread worker([&](std::stop_token stop){
            std::stop_callback onStop(stop,[&]{state.stop=true;});
            try {dps::runWasapi(options,state);}catch(...) {failure=std::current_exception();}
            finished.store(true,std::memory_order_release);
        });
        const auto start=std::chrono::steady_clock::now();
        unsigned display=0;
        while(!finished.load(std::memory_order_acquire)) {
            if(_kbhit()) {
                int key=_getch();bool changed=false;
                if(key=='q'||key=='Q'||key==27) state.stop=true;
                else if(key=='s'||key=='S') {
                    try {
                        profile.parameters.enabled=state.enabled.load();profile.parameters.intensity=state.intensity.load();
                        const auto destination=profilePath.empty()?"config/current.user.dps":profilePath;
                        dps::saveProfile(destination,profile);profilePath=destination;
                        std::cout<<"\nPerfil guardado: "<<destination<<'\n';
                    } catch(const std::exception& e) {std::cerr<<"\nNo se pudo guardar: "<<e.what()<<'\n';}
                }
                else if(key=='b'||key=='B') {state.enabled=!state.enabled.load();changed=true;}
                else if(key=='+'||key=='=') {state.intensity=std::min(1.f,state.intensity.load()+0.1f);changed=true;}
                else if(key=='-'||key=='_') {state.intensity=std::max(0.f,state.intensity.load()-0.1f);changed=true;}
                if(changed) std::cout<<"\r                                                                           \rDSP "<<(state.enabled.load()?"ACTIVO":"DESACTIVADO")<<" | Intensidad "<<static_cast<int>(std::lround(state.intensity.load()*100))<<"%"<<(state.intensity.load()>=1?" (maximo)":"")<<"\n";
            }
            if(seconds&&std::chrono::steady_clock::now()-start>=std::chrono::seconds(seconds)) state.stop=true;
            std::cout<<'\r'<<"DSP "<<(state.enabled.load()?"ON ":"OFF")<<" | Intensidad "<<std::setw(3)<<static_cast<int>(std::lround(state.intensity.load()*100))<<"% | "<<std::fixed<<std::setprecision(2)<<"In "<<state.inputPeak.load()<<" Out "<<state.outputPeak.load()<<" GR "<<state.reduction.load()<<" dB    "<<std::flush;
            if(log&&display++%10==0) {log<<"seconds="<<std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count()<<" cable_volume="<<state.cableVolume.load()<<" input="<<state.inputPeak.load()<<" output="<<state.outputPeak.load()<<" reduction="<<state.reduction.load()<<" enhancement="<<state.enhancement.load()<<" intensity="<<state.intensity.load()<<" enabled="<<state.enabled.load()<<" under="<<state.underruns.load()<<" over="<<state.overflows.load()<<'\n';log.flush();}
            std::this_thread::sleep_for(std::chrono::milliseconds(100));
        }
        worker.join();std::cout<<'\n';if(failure) std::rethrow_exception(failure);
    }catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
