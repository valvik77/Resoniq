#include "dps/measure.hpp"
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <numbers>
#include <random>
#include <vector>
#include <limits>
#include <stdexcept>
namespace {
constexpr double rate=48000;
void check(bool condition,const char* what) {
    if(!condition) {std::cerr<<"FAIL: "<<what<<'\n';std::exit(1);}
}
// Record the sweep through a speaker that behaves as described: each band is
// attenuated by the given amount, plus room noise.
std::vector<float> simulate(const std::vector<float>& sweep,const std::array<float,8>& bandDb,
                            float noiseAmplitude,unsigned seed) {
    std::mt19937 rng(seed);std::uniform_real_distribution<float> noise(-1.f,1.f);
    const auto tone=static_cast<std::size_t>(rate*dps::sweepToneSeconds);
    const auto gap=static_cast<std::size_t>(rate*dps::sweepGapSeconds);
    const std::size_t step=tone+gap,frames=sweep.size()/2;
    std::vector<float> out(frames);
    for(std::size_t i=0;i<frames;++i) {
        const std::size_t band=std::min<std::size_t>(i/step,dps::measureBands.size()-1);
        const float gain=std::pow(10.f,bandDb[band]/20.f);
        out[i]=sweep[2*i]*gain+noiseAmplitude*noise(rng);
    }
    return out;
}
std::vector<float> roomNoise(std::size_t frames,float amplitude,unsigned seed) {
    std::mt19937 rng(seed);std::uniform_real_distribution<float> noise(-1.f,1.f);
    std::vector<float> out(frames);
    for(auto& v:out) v=amplitude*noise(rng);
    return out;
}
}
int main() {
    const auto sweep=dps::sweepSignal(rate,0.5f);
    check(!sweep.empty(),"Sweep signal is empty");
    check(sweep.size()%2==0,"Sweep signal is not stereo");
    for(double invalidRate:{0.,16000.,std::numeric_limits<double>::quiet_NaN(),1e20}) {
        bool rejected=false;try {(void)dps::sweepSignal(invalidRate,.5f);}catch(const std::invalid_argument&) {rejected=true;}
        check(rejected,"Invalid sweep rate accepted");
        const float sample=0;
        check(!dps::analyseSweep(&sample,1,invalidRate,&sample,1).usable,"Invalid analysis rate accepted");
    }
    const float bad=std::numeric_limits<float>::quiet_NaN(),zero=0;
    check(!dps::analyseSweep(nullptr,1,rate,&zero,1).usable,"Null recording accepted");
    check(!dps::analyseSweep(&bad,1,rate,&zero,1).usable,"Non-finite recording accepted");
    check(!dps::analyseSweep(&zero,1,rate,nullptr,1).usable,"Null room recording accepted");
    check(!dps::analyseSweep(&zero,1,rate,&zero,0).usable,"Missing room recording accepted");
    dps::Measurement invalid;invalid.usable=true;invalid.levelDb[2]=bad;
    check(dps::calibrationFrom(invalid,true).bodyDb==dps::DeviceCalibration{}.bodyDb,
          "Non-finite measurement produced calibration");

    // A flat speaker in a quiet room must measure flat.
    {
        const std::array<float,8> flat{0,0,0,0,0,0,0,0};
        const auto recorded=simulate(sweep,flat,0.0005f,1);
        const auto silence=roomNoise(static_cast<std::size_t>(rate),0.0005f,2);
        const auto m=dps::analyseSweep(recorded.data(),recorded.size(),rate,silence.data(),silence.size());
        check(m.usable,"Flat speaker measurement rejected");
        for(std::size_t b=0;b<dps::measureBands.size();++b) {
            std::cout<<dps::measureBands[b]<<"Hz="<<m.levelDb[b]<<" dB\n";
            check(std::abs(m.levelDb[b])<3,"Flat speaker did not measure flat");
        }
    }

    // A laptop-like speaker: nothing below 120 Hz, dull presence.
    {
        const std::array<float,8> laptop{-26,-14,-2,0,1,-4,-5,-3};
        const auto recorded=simulate(sweep,laptop,0.0005f,3);
        const auto silence=roomNoise(static_cast<std::size_t>(rate),0.0005f,4);
        const auto m=dps::analyseSweep(recorded.data(),recorded.size(),rate,silence.data(),silence.size());
        check(m.usable,"Laptop measurement rejected");
        check(m.levelDb[0]<m.levelDb[3]-10,"Missing bass was not detected");
        const auto c=dps::calibrationFrom(m,true);
        std::cout<<"highpass="<<c.highpassHz<<" body="<<c.bodyDb<<" presence="<<c.presenceDb
                 <<" harmonic="<<c.harmonicGain<<'\n';
        check(c.highpassHz>=80&&c.highpassHz<=200,"Cut frequency outside the sensible range");
        check(c.presenceDb>0,"Dull presence produced no lift");
        // Whatever it produces must be a calibration the engine accepts.
        dps::Parameters p;p.device=c;
        dps::Engine engine;engine.prepare(rate,p);
    }

    // The first two tones may be inaudible, and the speaker/microphone path
    // adds delay. Do not shift the 250 Hz onset into the 60 Hz band.
    {
        const std::array<float,8> laptop{-100,-100,-9,7,2,-13,-23,-3};
        auto recorded=simulate(sweep,laptop,0.00001f,9);
        recorded.insert(recorded.begin(),2400,0.f); // 50 ms of path delay
        const auto silence=roomNoise(48000,0.00001f,10);
        const auto m=dps::analyseSweep(recorded.data(),recorded.size(),rate,silence.data(),silence.size());
        check(m.usable,"Delayed laptop measurement rejected");
        const float reference=(laptop[2]+laptop[3]+laptop[4])/3.f;
        for(std::size_t band=2;band<8;++band)
            check(std::abs(m.levelDb[band]-(laptop[band]-reference))<1,
                  "Inaudible first tones shift the analysis windows");
    }
    // Too much room noise: the measurement must be refused, not guessed.
    {
        const std::array<float,8> laptop{-100,-100,-9,7,2,-13,-23,-3};
        for(unsigned delay:{0u,480u,2400u,8640u,12000u}) {
            auto recorded=simulate(sweep,laptop,.0002f,11);
            recorded.insert(recorded.begin(),delay,0.f);
            // A short noise before the actual signal must not set alignment.
            if(delay>200) recorded[100]=.8f;
            for(float roomLevel:{.0001f,.0003f}) {
                const auto silence=roomNoise(48000,roomLevel,12);
                const auto m=dps::analyseSweep(recorded.data(),recorded.size(),rate,silence.data(),silence.size());
                check(m.usable&&m.reliableBands>=2,"Reliable delayed measurement rejected");
                check(std::abs(m.alignmentMs-delay*1000.f/48000.f)<=10,"Midrange alignment follows noise/delay incorrectly");
                check(std::abs(m.levelDb[2]+9)<.5f,"Body measurement depends on onset/noise threshold");
            }
        }
    }
    // Quiet baseline followed by unrelated noise must not look like test tones.
    {
        const auto recorded=roomNoise(sweep.size()/2,.15f,14);
        const auto silence=roomNoise(48000,.0001f,15);
        check(!dps::analyseSweep(recorded.data(),recorded.size(),rate,silence.data(),silence.size()).usable,
              "Unrelated noise during sweep accepted as tones");
    }
    // Crossing the old -10 dB boundary must no longer jump 38 Hz.
    {
        dps::Measurement m;m.usable=true;m.levelDb={-50,-27,-9.99f,7,2,-13,-23,-3};
        const auto a=dps::calibrationFrom(m,true);
        m.levelDb[2]=-10.01f;const auto b=dps::calibrationFrom(m,true);
        check(std::abs(b.highpassHz-a.highpassHz)<1,"Cut proposal still has a threshold jump");
        m.levelDb[2]=-14;const auto c=dps::calibrationFrom(m,true);
        check(c.highpassHz>=b.highpassHz&&c.highpassHz<=200,"Cut proposal is not bounded/monotonic");
    }
    // Too much room noise: the measurement must be refused, not guessed.
    {
        const std::array<float,8> flat{0,0,0,0,0,0,0,0};
        const auto recorded=simulate(sweep,flat,0.30f,5);
        const auto silence=roomNoise(static_cast<std::size_t>(rate),0.30f,6);
        const auto m=dps::analyseSweep(recorded.data(),recorded.size(),rate,silence.data(),silence.size());
        std::cout<<"ruidoso: signal="<<m.signalDb<<" noise="<<m.noiseFloorDb
                 <<" headroom="<<(m.signalDb-m.noiseFloorDb)<<" usable="<<m.usable<<char(10);
        check(!m.usable,"Noisy measurement was accepted");
        check(!m.warning.empty(),"Rejected measurement gave no reason");
        // A refused measurement must not change anything.
        const auto c=dps::calibrationFrom(m,true);
        const dps::DeviceCalibration untouched{};
        check(c.highpassHz==untouched.highpassHz&&c.bodyDb==untouched.bodyDb,
              "Unusable measurement still produced a calibration");
    }

    // A truncated recording must be refused rather than analysed.
    {
        const std::array<float,8> flat{0,0,0,0,0,0,0,0};
        const auto silence=roomNoise(48000,.0001f,18);
        auto recorded=simulate(sweep,flat,.0001f,17);
        const auto clean=recorded;
        std::array<dps::AudioGap,1> gaps{{{0,480}}};
        std::fill_n(recorded.begin(),480,0.f);
        auto m=dps::analyseSweep(recorded.data(),recorded.size(),rate,silence.data(),silence.size(),gaps);
        check(m.usable,"Startup gap outside analysis windows invalidates sweep");
        recorded=clean;gaps[0]={48000*2+6000,480};
        std::fill_n(recorded.begin()+gaps[0].begin,gaps[0].frames,0.f);
        m=dps::analyseSweep(recorded.data(),recorded.size(),rate,silence.data(),silence.size(),gaps);
        check(!m.usable,"Gap inside a measured tone accepted");
    }
    // A truncated recording must be refused rather than analysed.
    {
        const std::array<float,8> flat{0,0,0,0,0,0,0,0};
        const auto recorded=simulate(sweep,flat,0.0005f,7);
        const auto silence=roomNoise(static_cast<std::size_t>(rate),0.0005f,8);
        const auto m=dps::analyseSweep(recorded.data(),recorded.size()/3,rate,silence.data(),silence.size());
        check(!m.usable,"Truncated recording was accepted");
    }

    std::cout<<"PASS: sweep generation, flat and laptop responses, noise and truncation refused\n";
    return 0;
}
