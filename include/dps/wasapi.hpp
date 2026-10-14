#pragma once
#include <atomic>
#include <string>
#include <vector>
#include "dps/engine.hpp"
#include "dps/audio_gap.hpp"
namespace dps {
struct Endpoint {std::wstring id,name;bool capture=false;float peak=0;bool defaultRender=false;float volume=-1;bool muted=false;std::wstring interfaceName;bool defaultCapture=false;};
std::vector<Endpoint> endpoints();
// Control thread only. Explicit physical endpoint volume, in [0,1].
void setEndpointVolume(const std::wstring& id,float volume);
struct LiveOptions {std::wstring inputId,outputId;bool loopback=false,tone=false,raw=false;Parameters parameters;};
struct LiveState {
    std::atomic<bool> stop=false,enabled=true;
    std::atomic<float> bass=.3f,clarity=.25f,dynamics=.3f,spatial=.15f;
    std::atomic<float> dynamicLiftDb=0,transientLiftDb=0;
    std::atomic<float> intensity=1,inputPeak=0,outputPeak=0,reduction=0,enhancement=1;
    std::atomic<float> processedPeak=0,dspGainDb=0;
    std::atomic<unsigned> underruns=0,overflows=0,discontinuities=0,queued=0;
    // VB-CABLE only: 1 not measured yet, 2 the cable attenuates its own
    // loopback, 3 it ignores the volume and the host applies it. 0 otherwise.
    std::atomic<int> cableVolume=0;
    // Estimated latency from endpoint reports, render capacity and FIFO size.
    // This is not a measured acoustic or electrical end-to-end round trip.
    std::atomic<float> latencyMs=0;
};
// Owns COM, clients, buffers and DSP on the caller's audio worker thread.
void runWasapi(const LiveOptions&,LiveState&);

// Play the measurement tones on one endpoint while recording the room on
// another, and return what was recorded as mono. Blocking: the caller runs it
// on a worker thread. Only called after the user has agreed to it.
struct CaptureResult {
    std::vector<float> recorded,silence; double rate=48000;
    unsigned discontinuities=0;std::size_t missingFrames=0;
    std::vector<AudioGap> gaps;
    bool rawInput=false;std::string rawInputError;unsigned selectedChannel=0;
};
CaptureResult captureMeasurement(const std::wstring& outputId,const std::wstring& inputId,
                                 const std::vector<float>& tones,bool rawOutput=false);
// Capture endpoints, so the caller can offer a microphone to measure with.
std::vector<Endpoint> captureEndpoints();
}
