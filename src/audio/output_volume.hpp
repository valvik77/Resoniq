#pragma once
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstddef>

namespace dps::detail {
// VB-CABLE's hardware loopback is before its endpoint volume. Honour the
// endpoint's actual dB curve after DSP; its scalar is an audio-taper control,
// not a linear sample multiplier. Only the observer calls COM.
class OutputVolume {
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
