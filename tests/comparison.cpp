#include "dps/engine.hpp"
#include <vector>
#include <cmath>
#include <numbers>
#include <iostream>
#include <stdexcept>
#include <limits>
void check(bool b,const char* s){if(!b) throw std::runtime_error(s);}
int main(){try {
    constexpr std::size_t frames=144000;
    std::vector<float> input(frames*2),output(input.size());
    for(std::size_t i=0;i<frames;++i) input[2*i]=input[2*i+1]=.04f*std::sin(float(2*std::numbers::pi*1000*i/48000));
    dps::Parameters parameters;parameters.bass=parameters.clarity=parameters.dynamics=parameters.spatial=0;
    dps::Engine engine;engine.prepare(48000,parameters);engine.setComparison(true);
    for(int k=0;k<4;++k) engine.process(input.data(),output.data(),frames);
    auto energy=[&] {double sum=0;for(std::size_t i=96000;i<frames;++i) sum+=output[2*i]*output[2*i];return sum/48000;};
    engine.process(input.data(),output.data(),frames);
    std::cout<<"Matched wet gain="<<engine.meters().comparisonWetDb<<" dB; dry="<<engine.meters().comparisonDryDb<<" dB\n";
    const double wetEnergy=energy();check(engine.meters().comparisonReady,"Comparison never becomes ready");
    check(engine.meters().comparisonWetDb< -5&&engine.meters().comparisonDryDb<=0,"Comparison amplifies or fails to attenuate louder route");
    engine.setEnabled(false);engine.process(input.data(),output.data(),frames);const double dryEnergy=energy();
    const double mismatch=10*std::log10(wetEnergy/dryEnergy);std::cout<<"Steady A/B RMS mismatch="<<mismatch<<" dB\n";
    check(std::abs(mismatch)<.3,"A/B RMS mismatch too large");
    // A programme-level offset must not act as a second dynamic processor.
    // Keep it across louder passages, silence and A/B selection.
    const float fixedWet=engine.meters().comparisonWetDb,fixedDry=engine.meters().comparisonDryDb;
    for(auto& v:input) v*=8;
    engine.setEnabled(true);engine.process(input.data(),output.data(),frames);
    check(std::abs(engine.meters().comparisonWetDb-fixedWet)<.01&&std::abs(engine.meters().comparisonDryDb-fixedDry)<.01,"Matcher follows programme dynamics after calibration");
    std::fill(input.begin(),input.end(),0.f);engine.process(input.data(),output.data(),frames);
    check(engine.meters().comparisonReady,"Silence discards fixed comparison calibration");
    check(std::abs(engine.meters().comparisonWetDb-fixedWet)<.01,"Silence changes comparison offset");
    for(std::size_t i=0;i<frames;++i) input[2*i]=input[2*i+1]=.04f*std::sin(float(2*std::numbers::pi*1000*i/48000));
    engine.setComparison(false);engine.setEnabled(true);engine.process(input.data(),output.data(),frames);
    for(std::size_t i=1;i<frames;++i) check(std::abs(output[2*i]-output[2*i-2])<.03f,"Comparison switch clicks");
    engine.setComparison(true);engine.setIntensity(0);for(int k=0;k<5;++k) engine.process(input.data(),output.data(),frames);
    check(std::abs(engine.meters().comparisonDryDb)<.1&&std::abs(engine.meters().comparisonWetDb)<.1,"Zero intensity remains compensated");
    engine.reset();std::fill(input.begin(),input.end(),0.f);engine.process(input.data(),output.data(),frames);
    check(!engine.meters().comparisonReady,"Silent comparison marked ready");for(float x:output) check(x==0,"Comparison produces noise in silence");
    // Adversarial input and arbitrary blocks with matching enabled.
    input.resize(8192);output.resize(input.size());std::vector<float> other(input.size());
    for(std::size_t i=0;i<input.size();++i) input[i]=float(4*std::sin(i*.71));input[0]=std::numeric_limits<float>::quiet_NaN();
    for(double rate:{16000.,44100.,48000.,96000.,192000.}) for(const auto* preset:{"Natural","Enhanced","Powerful","Laptop Max"}) {
        dps::Engine a,b;a.prepare(rate,dps::preset(preset));b.prepare(rate,dps::preset(preset));a.setComparison(true);b.setComparison(true);
        a.process(input.data(),output.data(),4096);
        for(std::size_t i=0;i<4096;) {const auto n=std::min<std::size_t>(113,4096-i);b.process(input.data()+2*i,other.data()+2*i,n);i+=n;}
        check(output==other,"Comparison depends on block size");for(float x:output) check(std::isfinite(x)&&std::abs(x)<=dps::Engine::ceiling,"Comparison clips or is non-finite");
    }
    std::cout<<"PASS: matched RMS, silence gate, switches, zero intensity, 20 rate/preset combinations and block continuity\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
