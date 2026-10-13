#pragma once
#include <algorithm>
#include <cmath>

namespace dps {
// Stereo-linked onset detector. Preparation happens outside the audio thread.
// Sustained energy is not given constant makeup gain; below-gate noise is ignored.
class TransientShaper {
    float fast_=0,slow_=0,gain_=1;
    float fastAttack_=0,slowAttack_=0,release_=0,gainAttack_=0,gainRelease_=0;
public:
    void prepare(double rate) noexcept {
        auto c=[rate](double ms){return static_cast<float>(std::exp(-1/(rate*.001*ms)));};
        fastAttack_=c(1);slowAttack_=c(80);release_=c(80);
        gainAttack_=c(2);gainRelease_=c(35);reset();
    }
    void reset() noexcept {fast_=slow_=0;gain_=1;}
    float tick(float peak,float amountDb) noexcept {
        peak=std::isfinite(peak)?std::abs(peak):0;
        const float fc=peak>fast_?fastAttack_:release_;
        const float sc=peak>slow_?slowAttack_:release_;
        fast_=fc*fast_+(1-fc)*peak;slow_=sc*slow_+(1-sc)*peak;
        const float gate=std::clamp((fast_-.003f)/.017f,0.f,1.f);
        const float onset=std::clamp((fast_/std::max(slow_,.003f)-1.35f)/2.f,0.f,1.f);
        const float amount=std::isfinite(amountDb)?std::clamp(amountDb,0.f,6.f):0;
        const float target=std::pow(10.f,amount*gate*onset/20);
        const float gc=target>gain_?gainAttack_:gainRelease_;
        gain_=gc*gain_+(1-gc)*target;
        return gain_;
    }
};
// Short, edge-triggered accent. The cooldown prevents a sustained tone from
// repeatedly firing. Unlike the broad detail envelope it does not lift the tail.
class AttackAccent {
    float fast_=0,slow_=0,pulse_=0,gain_=1;
    float fastAttack_=0,fastRelease_=0,slowAttack_=0,slowRelease_=0,decay_=0,gainAttack_=0,gainRelease_=0;
    unsigned cooldown_=0,hold_=0;
public:
    void prepare(double rate) noexcept {
        auto c=[rate](double ms){return static_cast<float>(std::exp(-1/(rate*.001*ms)));};
        fastAttack_=c(1);fastRelease_=c(8);slowAttack_=c(40);slowRelease_=c(80);
        decay_=c(6);gainAttack_=c(1);gainRelease_=c(8);hold_=static_cast<unsigned>(rate*.12);reset();
    }
    void reset() noexcept {fast_=slow_=pulse_=0;gain_=1;cooldown_=0;}
    float tick(float peak,float amountDb) noexcept {
        peak=std::isfinite(peak)?std::abs(peak):0;
        float c=peak>fast_?fastAttack_:fastRelease_;fast_=c*fast_+(1-c)*peak;
        c=peak>slow_?slowAttack_:slowRelease_;slow_=c*slow_+(1-c)*peak;
        if(cooldown_) --cooldown_;
        if(!cooldown_&&fast_>.008f&&fast_>std::max(slow_,.003f)*1.8f) {pulse_=1;cooldown_=hold_;}
        const float gate=std::clamp((fast_-.004f)/.02f,0.f,1.f);
        const float headroom=std::clamp(1-fast_/.15f,0.f,1.f);
        const float amount=std::isfinite(amountDb)?std::clamp(amountDb,0.f,6.f):0;
        const float target=std::pow(10.f,amount*pulse_*gate*headroom/20);
        pulse_*=decay_;c=target>gain_?gainAttack_:gainRelease_;gain_+=(1-c)*(target-gain_);
        if(pulse_<1e-7f&&std::abs(gain_-1)<1e-4f) gain_=1;
        return gain_;
    }
};
}
