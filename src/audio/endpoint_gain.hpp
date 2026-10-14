#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>

namespace dps::detail {
// Follows a render endpoint's actual dB curve; its scalar is an audio-taper
// control, not a linear sample multiplier. Used ahead of the DSP for a cable
// whose loopback ignores the endpoint volume, so that Windows volume acts at
// the same point as on transports that attenuate their own loopback: the
// approved voicing was tuned with the volume in front of the engine.
// Only the observer thread calls COM; apply() belongs to the audio thread.
class EndpointGain {
    std::atomic<float> target_{1.f};
    float current_=1.f;
public:
    static float amplitude(float db) noexcept {
        return std::isfinite(db)?std::pow(10.f,std::min(db,0.f)/20.f):0.f;
    }
    void initialise(float db) noexcept {current_=amplitude(db);target_.store(current_,std::memory_order_release);}
    void setDb(float db) noexcept {target_.store(amplitude(db),std::memory_order_release);}
    float apply(float* samples,std::size_t frames) noexcept {
        const float target=target_.load(std::memory_order_acquire);
        float peak=0;
        for(std::size_t i=0;i<frames;++i) {
            // 5 ms smoothing at 48 kHz, without FIFO flushes or DSP resets.
            current_+=(target-current_)*.004158f;
            for(std::size_t channel=0;channel<2;++channel) {
                auto& sample=samples[2*i+channel];sample*=current_;
                peak=std::max(peak,std::abs(sample));
            }
        }
        return peak;
    }
};
}
