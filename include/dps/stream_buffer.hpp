#pragma once
#include <array>
#include <algorithm>
#include <cmath>
#include <cstddef>
#include <numbers>
namespace dps {
// Single audio-thread FIFO with adaptive resampling between the capture and
// render clocks. No concurrent access.
//
// The read position drifts through every fractional phase as the two clocks
// slide past each other. Interpolating linearly there lost 0.8 dB at 8 kHz and
// 3 dB at 16 kHz on average, and made that loss breathe by several dB as the
// phase swept. A 16-tap windowed-sinc fractional delay keeps the band flat and
// steady instead. The table is built once, off the audio path.
class StreamBuffer {
    static constexpr std::size_t capacity=16384;
    static constexpr int taps=16,before=7,phases=256;
    using Table=std::array<std::array<float,taps>,phases+1>;
    static const Table& table() {
        static const Table built=[] {
            Table t{};
            const auto bessel=[](double x) {double sum=1,term=1;for(int k=1;k<32;++k) {term*=(x/(2*k))*(x/(2*k));sum+=term;}return sum;};
            const double beta=7.5,norm=bessel(beta);
            for(int p=0;p<=phases;++p) {
                const double fraction=static_cast<double>(p)/phases;double total=0;std::array<double,taps> row{};
                for(int k=0;k<taps;++k) {
                    const double x=(k-before)-fraction,reach=x/(taps/2);
                    const double sinc=std::abs(x)<1e-12?1.0:std::sin(std::numbers::pi*x)/(std::numbers::pi*x);
                    row[k]=std::abs(reach)>=1?0.0:sinc*bessel(beta*std::sqrt(1-reach*reach))/norm;total+=row[k];
                }
                // Unity at DC for every phase: a steady level must not move with drift.
                for(int k=0;k<taps;++k) t[p][k]=static_cast<float>(row[k]/total);
            }
            // Whole-sample positions pass the stored sample through untouched.
            t[0].fill(0);t[0][before]=1;t[phases].fill(0);t[phases][before+1]=1;
            return t;
        }();
        return built;
    }
    std::array<std::array<float,2>,capacity> data_{};
    const Table* table_=&table();
    std::size_t read_=0,count_=0,history_=0;
    double phase_=0;
    double ratio_=1;
public:
    std::size_t size() const noexcept {return count_;}
    void reset() noexcept {read_=count_=history_=0;phase_=0;ratio_=1;}
    bool push(float l,float r) noexcept {
        if(count_==capacity) return false;
        data_[(read_+count_)%capacity]={l,r};++count_;
        // Newly queued frames can overwrite consumed samples behind read_.
        history_=std::min(history_,capacity-count_);
        return true;
    }
    void tune(std::size_t target) noexcept {
        if(target==0) return;
        const double desired=1+std::clamp((double(count_)-double(target))/double(target)*0.001,-0.002,0.002);
        ratio_+=0.01*(desired-ratio_);
    }
    bool pop(float& l,float& r) noexcept {
        if(count_<2) {l=r=0;return false;}
        // The filter reaches seven samples back and eight ahead. Samples already
        // read stay in the ring behind the read position; until that history
        // exists, and when the queue is nearly empty, fall back to the two-point
        // read. push() tracks history overwritten by a nearly full queue,
        // including a successful fill that never reports an overflow.
        if(history_>=before&&count_>before+1) {
            const double position=phase_*phases;const auto index=std::min<std::size_t>(static_cast<std::size_t>(position),phases-1);
            const float blend=static_cast<float>(position-static_cast<double>(index));
            const auto& low=(*table_)[index];const auto& high=(*table_)[index+1];
            float left=0,right=0;const std::size_t start=read_+capacity-before;
            for(int k=0;k<taps;++k) {
                const float weight=low[k]+blend*(high[k]-low[k]);const auto& sample=data_[(start+k)%capacity];
                left+=weight*sample[0];right+=weight*sample[1];
            }
            l=left;r=right;
        } else {
            auto& a=data_[read_];auto& b=data_[(read_+1)%capacity];
            l=static_cast<float>(a[0]+phase_*(b[0]-a[0]));r=static_cast<float>(a[1]+phase_*(b[1]-a[1]));
        }
        phase_+=ratio_;auto consumed=static_cast<std::size_t>(phase_);consumed=std::min(consumed,count_);
        phase_-=double(consumed);read_=(read_+consumed)%capacity;count_-=consumed;
        history_=std::min<std::size_t>(history_+consumed,before);return true;
    }
};
}
