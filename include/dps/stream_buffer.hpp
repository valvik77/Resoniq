#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
namespace dps {
// Single audio-thread FIFO with linear adaptive resampling. No concurrent access.
class StreamBuffer {
    static constexpr std::size_t capacity=16384;
    std::array<std::array<float,2>,capacity> data_{};
    std::size_t read_=0,count_=0;
    double phase_=0;
    double ratio_=1;
public:
    std::size_t size() const noexcept {return count_;}
    void reset() noexcept {read_=count_=0;phase_=0;ratio_=1;}
    bool push(float l,float r) noexcept {
        if(count_==capacity) return false;
        data_[(read_+count_)%capacity]={l,r};++count_;return true;
    }
    void tune(std::size_t target) noexcept {
        if(target==0) return;
        const double desired=1+std::clamp((double(count_)-double(target))/double(target)*0.001,-0.002,0.002);
        ratio_+=0.01*(desired-ratio_);
    }
    bool pop(float& l,float& r) noexcept {
        if(count_<2) {l=r=0;return false;}
        auto& a=data_[read_];auto& b=data_[(read_+1)%capacity];
        l=static_cast<float>(a[0]+phase_*(b[0]-a[0]));r=static_cast<float>(a[1]+phase_*(b[1]-a[1]));
        phase_+=ratio_;auto consumed=static_cast<std::size_t>(phase_);consumed=std::min(consumed,count_);
        phase_-=double(consumed);read_=(read_+consumed)%capacity;count_-=consumed;return true;
    }
};
}
