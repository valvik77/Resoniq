#pragma once
#include <algorithm>
#include <cmath>
#include <cstddef>

namespace dps::detail {
// Audio-thread state for the initial cable measurement hold and 10 ms reopen.
class StartupGate {
    float gain_=1;
public:
    float apply(float* samples,std::size_t frames,bool hold) noexcept {
        if(hold) {
            gain_=0;
            std::fill_n(samples,frames*2,0.f);
            return 0;
        }
        float peak=0;
        for(std::size_t i=0;i<frames;++i) {
            gain_=std::min(1.f,gain_+1.f/480);
            for(std::size_t ch=0;ch<2;++ch) {
                auto& sample=samples[2*i+ch];
                sample*=gain_;
                peak=std::max(peak,std::abs(sample));
            }
        }
        return peak;
    }
};
}
