#pragma once
#include <windows.h>
#include <endpointvolume.h>
#include <wrl/implements.h>
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>

namespace dps::detail {
// Loopback can still deliver the mix when the render endpoint is muted. The
// host must honour that endpoint's silence after DSP, including filter tails.
// Each callback publishes a versioned snapshot so initial reads cannot replace
// a newer notification. No COM calls, allocation or locks inside OnNotify.
class EndpointSilence final : public Microsoft::WRL::RuntimeClass<
    Microsoft::WRL::RuntimeClassFlags<Microsoft::WRL::ClassicCom>,IAudioEndpointVolumeCallback> {
    std::atomic<unsigned> state_{1}; // low bit = silent; upper bits = silence-transition revision
    // Only the audio thread touches these. Allow endpoint/capture queues to
    // settle before reopening; muted loopback packets can be full scale.
    unsigned observed_=0;
    bool observedValid_=false;
    std::size_t holdFrames_=0;
    static bool closed(float volume,bool muted) noexcept {
        return muted||!std::isfinite(volume)||volume<=0;
    }
public:
    unsigned snapshot() const noexcept {return state_.load(std::memory_order_acquire);}
    bool silent() const noexcept {return (snapshot()&1)!=0;}
    void initialise(unsigned before,float volume,bool muted) noexcept {
        const unsigned desired=(before&~1u)|(closed(volume,muted)?1u:0u);
        state_.compare_exchange_strong(before,desired,std::memory_order_release,std::memory_order_relaxed);
        observed_=desired;observedValid_=true;
    }
    HRESULT STDMETHODCALLTYPE OnNotify(PAUDIO_VOLUME_NOTIFICATION_DATA notification) noexcept override {
        if(!notification) return E_POINTER;
        unsigned previous=state_.load(std::memory_order_relaxed);
        unsigned next;
        do {
            const unsigned silentBit=closed(notification->fMasterVolume,notification->bMuted!=FALSE)?1u:0u;
            // Ordinary nonzero volume steps are already applied by Windows.
            // Preserve revisions for quick mute/unmute pairs between audio wakes.
            next=(previous&1u)==silentBit?previous:((previous+2)&~1u)|silentBit;
        } while(!state_.compare_exchange_weak(previous,next,std::memory_order_release,std::memory_order_relaxed));
        return S_OK;
    }
    bool apply(float* samples,std::size_t frames) noexcept {
        const auto current=snapshot();
        if(observedValid_&&current!=observed_) holdFrames_=8640; // 180 ms at 48 kHz
        observed_=current;observedValid_=true;
        if(current&1) holdFrames_=8640;
        else if(!holdFrames_) return false;
        else holdFrames_=frames>=holdFrames_?0:holdFrames_-frames;
        std::fill_n(samples,frames*2,0.f);return true;
    }
};
}
