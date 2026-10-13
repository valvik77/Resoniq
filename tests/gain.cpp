#include "dps/engine.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
int main() {
    try {
        std::array<float,960> signal{},reference{},boosted{},original{};
        for(std::size_t i=0;i<480;++i) signal[2*i]=signal[2*i+1]=.0001f*std::sin(static_cast<float>(i)*.13f);
        dps::Parameters p;dps::Engine flat,gain,bypass;
        flat.prepare(48000,p);p.gainDb=6;gain.prepare(48000,p);p.enabled=false;bypass.prepare(48000,p);
        flat.process(signal.data(),reference.data(),480);gain.process(signal.data(),boosted.data(),480);bypass.process(signal.data(),original.data(),480);
        const float amplitude=std::pow(10.f,6.f/20.f);
        for(std::size_t i=0;i<signal.size();++i) {
            check(std::abs(boosted[i]-reference[i]*amplitude)<1e-7f,"Gain is not applied after DSP with its dB curve");
            check(original[i]==signal[i],"Gain changes original/bypass audio");
        }
        p.enabled=true;p.intensity=0;gain.prepare(48000,p);gain.process(signal.data(),original.data(),480);
        check(original==signal,"Gain changes zero intensity");
        p.intensity=1;p.gainDb=12;gain.prepare(48000,p);
        for(auto& sample:signal) sample=.95f;
        gain.process(signal.data(),boosted.data(),480);
        for(float sample:boosted) check(std::isfinite(sample)&&std::abs(sample)<=dps::Engine::ceiling,"Gain bypasses sample limiter");
        bool rejected=false;try {p.gainDb=13;gain.prepare(48000,p);}catch(const std::invalid_argument&){rejected=true;}
        check(rejected,"Out-of-range Gain accepted in profile");
        rejected=false;try {p.gainDb=std::numeric_limits<float>::quiet_NaN();gain.prepare(48000,p);}catch(const std::invalid_argument&){rejected=true;}
        check(rejected,"Nonfinite Gain accepted");
        std::cout<<"PASS: dB gain after DSP, original and zero intensity intact, protected peaks and invalid values rejected\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
