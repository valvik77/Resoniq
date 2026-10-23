#pragma once
#include <atomic>
#include <string>
#include <vector>
#include "dps/engine.hpp"
#include "dps/audio_gap.hpp"
#include "dps/endpoint_level.hpp"
namespace dps {
struct Endpoint {std::wstring id,name;bool capture=false;float peak=0;bool defaultRender=false;float volume=-1;bool muted=false;std::wstring interfaceName;bool defaultCapture=false;};
std::vector<Endpoint> endpoints();
// Whether Windows still knows this endpoint at all, whatever its state. One
// that is merely unplugged or disabled is still known; one that has been
// replaced, as a driver update can do, is not. Enumeration only reports the
// active ones, so this is what tells the two cases apart.
bool endpointKnown(const std::wstring& id);
// Control thread only. Explicit physical endpoint volume, in [0,1].
void setEndpointVolume(const std::wstring& id,float volume);
// Control-thread read for the Windows volume caption; does not set volume.
EndpointLevel endpointLevel(const std::wstring& id);
struct LiveOptions {std::wstring inputId,outputId;bool loopback=false,tone=false,raw=false;Parameters parameters;};
struct LiveState {
    std::atomic<bool> stop=false,enabled=true;
    std::atomic<float> bass=.3f,clarity=.25f,dynamics=.3f,spatial=.15f;
    std::atomic<float> dynamicLiftDb=0,transientLiftDb=0;
    std::atomic<float> intensity=1,inputPeak=0,outputPeak=0,reduction=0,enhancement=1;
    std::atomic<unsigned> underruns=0,overflows=0,discontinuities=0,queued=0;
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
};
CaptureResult captureMeasurement(const std::wstring& outputId,const std::wstring& inputId,
                                 const std::vector<float>& tones,bool rawOutput=false);
// Capture endpoints, so the caller can offer a microphone to measure with.
std::vector<Endpoint> captureEndpoints();
}
