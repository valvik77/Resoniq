#pragma once
#include "dps/wasapi.hpp"
#include <windows.h>
#include <thread>
#include <stdexcept>
namespace resoniq::ui {
class AudioSession {
public:
    dps::LiveState state;
    std::thread thread;
    std::atomic<bool> finished=true;
    std::string failure;
    HANDLE guard=nullptr;
    ~AudioSession(){stop();}
    bool running() const {return thread.joinable();}
    void stop() {state.stop=true;if(thread.joinable()) thread.join();if(guard) {CloseHandle(guard);guard=nullptr;}}
    void start(const dps::LiveOptions& options) {
        stop();guard=CreateMutexW(nullptr,FALSE,L"Local\\DpsAudioEngine.LiveHost");
        if(!guard) throw std::runtime_error("No se pudo iniciar el motor");
        if(GetLastError()==ERROR_ALREADY_EXISTS) {CloseHandle(guard);guard=nullptr;throw std::runtime_error("Ya hay otro motor RESONIQ abierto. Cierra su consola antes de iniciar.");}
        state.stop=false;state.dynamicLiftDb=0;state.transientLiftDb=0;state.inputPeak=0;state.outputPeak=0;state.reduction=0;state.underruns=0;state.overflows=0;state.discontinuities=0;state.queued=0;state.latencyMs=0;state.enhancement=1;
        const auto& p=options.parameters;state.enabled=p.enabled;state.intensity=p.intensity;state.bass=p.bass;state.clarity=p.clarity;state.dynamics=p.dynamics;state.spatial=p.spatial;
        failure.clear();finished=false;
        try {thread=std::thread([this,options]{try {dps::runWasapi(options,state);}catch(const std::exception& e){failure=e.what();}catch(...){failure="Error inesperado de audio";}finished.store(true,std::memory_order_release);});}
        catch(...) {finished=true;stop();throw;}
    }
};
}
