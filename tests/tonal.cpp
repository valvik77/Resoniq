#include "dps/profile.hpp"
#include <array>
#include <vector>
#include <cmath>
#include <numbers>
#include <iostream>
#include <fstream>
#include <random>
#include <limits>
#include <stdexcept>
void check(bool b,const char* s){if(!b) throw std::runtime_error(s);}
constexpr std::size_t rate=48000,frames=rate*12;
constexpr std::array<double,6> frequencies{90,270,650,1400,2800,6000};
struct Result {double rms=0;std::array<double,6> levels{};};
Result measure(dps::Parameters p,float level,bool matched=false) {
    std::vector<float> input(frames*2),output(input.size());
    for(std::size_t i=0;i<frames;++i) {
        double value=0;for(double f:frequencies) value+=level*std::sin(2*std::numbers::pi*f*i/rate);
        input[2*i]=input[2*i+1]=static_cast<float>(value);
    }
    dps::Engine engine;engine.prepare(rate,p);engine.setComparison(matched);engine.process(input.data(),output.data(),frames);
    Result r;double energy=0;std::array<double,6> real{},imag{};
    for(std::size_t i=frames-rate;i<frames;++i) {
        const float x=output[2*i];check(std::isfinite(x)&&std::abs(x)<=dps::Engine::ceiling,"Tonal programme clips or is non-finite");energy+=x*x;
        for(std::size_t k=0;k<frequencies.size();++k) {const double phase=2*std::numbers::pi*frequencies[k]*i/rate;real[k]+=x*std::cos(phase);imag[k]+=x*std::sin(phase);}
    }
    r.rms=std::sqrt(energy/rate);
    for(std::size_t k=0;k<frequencies.size();++k) r.levels[k]=20*std::log10((2*std::hypot(real[k],imag[k])/rate)/r.rms);
    return r;
}
int main(int argc,char** argv){try {
    auto old=dps::preset("Laptop Max"),focused=old;focused.device=dps::deviceCalibration("Laptop Focus");focused.bass=.9f;focused.clarity=.9f;focused.dynamics=.65f;
    std::ofstream csv;if(argc==2) {csv.open(argv[1]);if(!csv) throw std::runtime_error("Cannot write report");csv<<"level,frequency_hz,reference_normalized_db,focused_normalized_db,delta_db\n";}
    for(float level:{.025f,.12f}) {
        const auto a=measure(old,level),b=measure(focused,level);
        std::cout<<"Input tone peak="<<level<<"; RMS-normalized spectral delta (dB): ";
        for(std::size_t k=0;k<frequencies.size();++k) {std::cout<<frequencies[k]<<"Hz="<<b.levels[k]-a.levels[k]<<" ";if(csv) csv<<level<<','<<frequencies[k]<<','<<a.levels[k]<<','<<b.levels[k]<<','<<b.levels[k]-a.levels[k]<<'\n';}
        std::cout<<'\n';
        check((b.levels[1]-b.levels[2])-(a.levels[1]-a.levels[2])>3,"New body/mud contrast fails at matched RMS");
        check((b.levels[4]-b.levels[2])-(a.levels[4]-a.levels[2])>2,"New presence/mud contrast fails at matched RMS");
    }
    // Verify the actual online A/B matcher, not only offline normalization.
    for(float level:{.025f,.12f}) {
        const auto processed=measure(focused,level,true);auto bypass=focused;bypass.enabled=false;
        const auto original=measure(bypass,level,true);
        const double mismatch=20*std::log10(processed.rms/original.rms);
        for(std::size_t k=0;k<frequencies.size();++k) std::cout<<"Processed vs bypass "<<frequencies[k]<<"Hz: "<<processed.levels[k]-original.levels[k]<<" dB at matched RMS\n";
        std::cout<<"Focused live A/B RMS mismatch="<<mismatch<<" dB at "<<level<<" per tone\n";
        check(std::abs(mismatch)<.3,"Focused comparison is not level matched");
        check(processed.levels[4]-original.levels[4]>3,"Presence contrast against actual bypass is insufficient at matched RMS");
        check(processed.levels[1]-original.levels[1]>1,"Body contrast against actual bypass is insufficient at matched RMS");
        check((processed.levels[1]-processed.levels[2])-(original.levels[1]-original.levels[2])>4,"Focused tonal change disappears with live level matching");
    }
    // Move virtual bass energy into the 210/280 Hz harmonics of a 70 Hz note.
    std::vector<float> bassInput(rate*4*2),legacyBass(bassInput.size()),focusBass(bassInput.size());
    for(std::size_t i=0;i<bassInput.size()/2;++i) bassInput[2*i]=bassInput[2*i+1]=.03f*std::sin(float(2*std::numbers::pi*70*i/rate));
    dps::Engine legacyEngine,focusEngine;legacyEngine.prepare(rate,old);focusEngine.prepare(rate,focused);
    legacyEngine.process(bassInput.data(),legacyBass.data(),bassInput.size()/2);focusEngine.process(bassInput.data(),focusBass.data(),bassInput.size()/2);
    auto bassAmplitude=[&](const std::vector<float>& data,double f) {double re=0,im=0;for(std::size_t i=rate*3;i<rate*4;++i) {const double phase=2*std::numbers::pi*f*i/rate;re+=data[2*i]*std::cos(phase);im+=data[2*i]*std::sin(phase);}return 2*std::hypot(re,im)/rate;};
    const double legacy280=bassAmplitude(legacyBass,280),focused280=bassAmplitude(focusBass,280);
    std::cout<<"70 Hz note: 280 Hz harmonic reference="<<legacy280<<" focused="<<focused280<<'\n';
    check(focused280>legacy280*1.5&&focused280>.001,"Focused bass does not move harmonics into audible midbass");
    // Gated upward dynamics increase quiet mid-band details relative to bass;
    // this is a level-dependent effect, not a constant overall volume gain.
    std::vector<float> input(rate*4*2),a(input.size()),b(input.size());
    for(std::size_t i=0;i<input.size()/2;++i) {const double t=2*std::numbers::pi*i/rate;input[2*i]=input[2*i+1]=float(.25*std::sin(80*t)+.02*std::sin(1400*t));}
    dps::Parameters plain;plain.bass=plain.clarity=plain.spatial=0;plain.dynamics=.7f;plain.device.highpassHz=20;plain.device.bodyDb=plain.device.presenceDb=plain.device.harmonicGain=0;
    auto lifted=plain;lifted.device.detailDb=4;
    dps::Engine x,y;x.prepare(rate,plain);y.prepare(rate,lifted);x.process(input.data(),a.data(),input.size()/2);y.process(input.data(),b.data(),input.size()/2);
    auto amplitude=[&](const std::vector<float>& data,double f) {double re=0,im=0;for(std::size_t i=rate*3;i<rate*4;++i) {const double phase=2*std::numbers::pi*f*i/rate;re+=data[2*i]*std::cos(phase);im+=data[2*i]*std::sin(phase);}return std::hypot(re,im);};
    const double detailDelta=20*std::log10((amplitude(b,1400)/amplitude(b,80))/(amplitude(a,1400)/amplitude(a,80)));
    std::cout<<"Dynamic mid detail/bass delta="<<detailDelta<<" dB\n";check(detailDelta>1,"Upward dynamics only changes global gain");
    std::fill(input.begin(),input.end(),.00001f);y.reset();y.process(input.data(),b.data(),input.size()/2);check(y.meters().dynamicLiftDb<.01,"Detail module lifts below-gate noise");
    // New calibration under adversarial input and arbitrary block boundaries.
    input.resize(16384);a.resize(input.size());b.resize(input.size());std::mt19937 rng(73);std::uniform_real_distribution<float> noise(-4,4);for(auto& v:input) v=noise(rng);input[0]=std::numeric_limits<float>::quiet_NaN();
    for(double sampleRate:{16000.,44100.,48000.,96000.,192000.}) for(const auto* name:{"Natural","Enhanced","Powerful","Laptop Max"}) {
        auto p=dps::preset(name);p.device=dps::deviceCalibration("Laptop Focus");x.prepare(sampleRate,p);y.prepare(sampleRate,p);
        x.process(input.data(),a.data(),8192);for(std::size_t i=0;i<8192;) {const auto n=std::min<std::size_t>(211,8192-i);y.process(input.data()+2*i,b.data()+2*i,n);i+=n;}
        check(a==b,"Focused calibration depends on block size");for(float v:a) check(std::isfinite(v)&&std::abs(v)<=dps::Engine::ceiling,"Focused calibration unstable");
    }
    std::cout<<"PASS: matched-level tonal contrast, gated dynamic detail, 20 calibration stress cases\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
