#include "dps/engine.hpp"
#include "dps/profile.hpp"
#include "dps/wav.hpp"
#include "dps/stream_buffer.hpp"
#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <numbers>
#include <random>
#include <stdexcept>
#include <vector>
std::atomic<bool> watch=false;
std::atomic<unsigned> allocations=0;
void* operator new(std::size_t n) {if(watch) ++allocations;if(auto p=std::malloc(n?n:1)) return p;throw std::bad_alloc();}
void* operator new[](std::size_t n) {return ::operator new(n);}
void operator delete(void* p) noexcept {std::free(p);}
void operator delete[](void* p) noexcept {std::free(p);}
void operator delete(void* p,std::size_t) noexcept {std::free(p);}
void operator delete[](void* p,std::size_t) noexcept {std::free(p);}
void check(bool yes,const char* message) {if(!yes) throw std::runtime_error(message);}
int main() {
try {
    dps::StreamBuffer fifo;
    float l=1,r=1;check(!fifo.pop(l,r)&&l==0&&r==0,"FIFO underrun");
    for(int i=0;i<16384;++i) check(fifo.push(float(i),float(-i)),"FIFO capacity");
    check(!fifo.push(0,0),"FIFO overflow");
    for(int i=0;i<16000;++i) {check(fifo.pop(l,r),"FIFO read");check(l==float(i)&&r==float(-i),"FIFO ordering");}
    for(int i=0;i<10000;++i) check(fifo.push(.2f,-.2f),"FIFO wraparound");
    fifo.reset();check(fifo.size()==0,"FIFO reset");
    fifo.tune(0);check(fifo.push(.2f,-.2f)&&fifo.push(.2f,-.2f),"FIFO zero target setup");
    check(fifo.pop(l,r)&&std::isfinite(l)&&std::abs(l-.2f)<1e-6f,"FIFO zero target corrupts resampling");
    fifo.reset();
    for(int i=0;i<2000;++i) fifo.push(.2f,-.2f);
    watch=true;
    for(int i=0;i<1000;++i) {fifo.tune(960);check(fifo.pop(l,r)&&std::abs(l-.2f)<1e-6f&&std::abs(l+r)<1e-6f,"Resampler DC or stereo corruption");}
    watch=false;
    std::mt19937 random(42);std::uniform_real_distribution<float> noise(-4,4);
    for(double rate:{16000.,44100.,48000.,96000.,192000.}) for(auto name:{"Natural","Enhanced","Powerful","Laptop Max"}) {
        std::vector<float> input(24000),a(input.size()),b(input.size());for(auto& x:input) x=noise(random);
        input[0]=std::numeric_limits<float>::infinity();input[1]=std::numeric_limits<float>::quiet_NaN();
        dps::Engine whole,blocked;whole.prepare(rate,dps::preset(name));blocked.prepare(rate,dps::preset(name));
        watch=true;whole.process(input.data(),a.data(),input.size()/2);watch=false;
        for(std::size_t i=0;i<input.size()/2;) {auto n=std::min<std::size_t>(1+random()%517,input.size()/2-i);blocked.process(input.data()+2*i,b.data()+2*i,n);i+=n;}
        check(a==b,"Block partition changes output");
        for(float x:a) check(std::isfinite(x)&&std::abs(x)<=dps::Engine::ceiling,"Non-finite or clipped output");
        auto inplace=input;blocked.reset();blocked.process(inplace.data(),inplace.data(),inplace.size()/2);check(inplace==a,"In-place differs");
        whole.reset();whole.process(input.data(),b.data(),input.size()/2);check(a==b,"Reset is not deterministic");
    }
    check(allocations==0,"Allocation on audio thread");
    {
        auto experimental=dps::preset("Laptop Max");experimental.alignedIntensity=true;
        experimental.device=dps::deviceCalibration("Laptop Focus");experimental.device.transientAccentDb=4;
        dps::Engine phase;phase.prepare(48000,experimental);
        std::array<float,1024> signal{},processed{};
        for(std::size_t i=0;i<signal.size();++i) signal[i]=.05f*std::sin(static_cast<float>(i));
        watch=true;
        for(float amount:{0.f,.01f,.1f,.25f,.5f,.75f,1.f}) {
            phase.setIntensity(amount);phase.process(signal.data(),processed.data(),512);
        }
        for(float db:{-12.f,0.f,3.f,6.f,12.f}) {
            phase.setGainDb(db);phase.process(signal.data(),processed.data(),512);
        }
        watch=false;check(allocations==0,"Aligned intensity allocates on audio thread");
    }
    dps::Engine mono;mono.prepare(48000,dps::preset("Laptop Max"));std::vector<float> input(96000),output(input.size());
    for(std::size_t i=0;i<input.size()/2;++i) input[2*i]=input[2*i+1]=0.1f*std::sin(float(2*std::numbers::pi*70*i/48000));
    mono.process(input.data(),output.data(),input.size()/2);for(std::size_t i=0;i<input.size()/2;++i) check(output[2*i]==output[2*i+1],"Mono image changed");
    mono.setEnabled(false);std::fill(input.begin(),input.end(),0.1f);mono.process(input.data(),output.data(),input.size()/2);check(std::abs(output.back()-0.1f)<1e-5,"Bypass fails to settle");
    mono.setIntensity(0);mono.reset();std::fill(input.begin(),input.end(),0.f);mono.process(input.data(),output.data(),input.size()/2);check(std::all_of(output.begin(),output.end(),[](float x){return x==0;}),"Silence generates signal");
    // Quiet bass must create an audible-band harmonic, not only turn up 70 Hz.
    dps::Parameters bassParameters; bassParameters.bass=1; bassParameters.clarity=0; bassParameters.dynamics=0; bassParameters.spatial=0;
    mono.prepare(48000,bassParameters);
    for(std::size_t i=0;i<input.size()/2;++i) input[2*i]=input[2*i+1]=.03f*std::sin(float(2*std::numbers::pi*70*i/48000));
    mono.process(input.data(),output.data(),input.size()/2);
    auto harmonicAmplitude=[&](double frequency) {double real=0,imaginary=0;for(std::size_t i=24000;i<48000;++i) {const double angle=2*std::numbers::pi*frequency*i/48000;real+=output[2*i]*std::cos(angle);imaginary+=output[2*i]*std::sin(angle);}return 2*std::hypot(real,imaginary)/24000;};
    const double bassHarmonic=harmonicAmplitude(140);
    bassParameters.bass=0;mono.prepare(48000,bassParameters);mono.process(input.data(),output.data(),input.size()/2);
    check(bassHarmonic>0.001&&bassHarmonic>harmonicAmplitude(140)*5,"Quiet bass harmonics ineffective");
    std::cout<<"Quiet 70 Hz input (0.03 peak): generated 140 Hz amplitude="<<bassHarmonic<<'\n';
    // Moderate passages keep full enhancement; sustained loud input backs it off.
    // A level change at a sine zero-crossing must not introduce a click.
    input.resize(192000);output.resize(input.size());
    mono.prepare(48000,dps::preset("Laptop Max"));
    for(std::size_t i=0;i<96000;++i) input[2*i]=input[2*i+1]=(i<48000?.08f:.8f)*std::sin(float(2*std::numbers::pi*190*i/48000));
    watch=true;mono.process(input.data(),output.data(),48000);watch=false;
    check(mono.meters().enhancement>.99f,"Moderate signal loses enhancement");
    watch=true;mono.process(input.data()+96000,output.data()+96000,48000);watch=false;
    check(mono.meters().enhancement<.5f&&mono.meters().enhancement>=.3f,"Loud signal does not back off enhancement");
    for(std::size_t i=48000;i<49000;++i) check(std::abs(output[2*i]-output[2*i-2])<.13f,"Adaptive transition clicks");
    check(allocations==0,"Adaptive processing allocates");
    auto fixed=dps::preset("Laptop Max");fixed.adaptive=false;mono.prepare(48000,fixed);
    mono.process(input.data()+96000,output.data()+96000,48000);
    check(mono.meters().enhancement==1,"Fixed enhancement mode changes strength");
    // Return to quiet input: adaptation must recover without a reset.
    mono.prepare(48000,dps::preset("Laptop Max"));mono.process(input.data()+96000,output.data()+96000,48000);
    for(int k=0;k<6;++k) mono.process(input.data(),output.data(),48000);
    check(mono.meters().enhancement>.99f,"Enhancement fails to recover after loud passage");
    // Loud mixed programme: keep audible bass harmonics and useful average
    // level, rather than only boosting sample peaks into the limiter.
    mono.prepare(48000,dps::preset("Laptop Max"));
    for(std::size_t i=0;i<96000;++i) {
        const double t=2*std::numbers::pi*i/48000;
        input[2*i]=input[2*i+1]=float(.45*std::sin(1000*t)+.2*std::sin(2600*t)+.15*std::sin(70*t));
    }
    mono.process(input.data(),output.data(),96000);
    double inEnergy=0,outEnergy=0,real140=0,imag140=0;
    for(std::size_t i=48000;i<96000;++i) {
        inEnergy+=input[2*i]*input[2*i];outEnergy+=output[2*i]*output[2*i];
        const double a=2*std::numbers::pi*140*i/48000;
        real140+=output[2*i]*std::cos(a);imag140+=output[2*i]*std::sin(a);
        check(std::isfinite(output[2*i])&&std::abs(output[2*i])<=dps::Engine::ceiling,"Loud programme clipped");
    }
    const double levelRatio=std::sqrt(outEnergy/inEnergy),loudHarmonic=2*std::hypot(real140,imag140)/48000;
    std::cout<<"Loud programme RMS output/input="<<levelRatio<<"; 140 Hz harmonic="<<loudHarmonic<<'\n';
    check(levelRatio>.85,"Loud programme loses average level");
    check(loudHarmonic>.015,"Loud programme loses audible bass harmonic");
    // Live control automation must be independent of block size and stay
    // allocation-free while filters retain their state.
    dps::Engine automationA,automationB;auto focusControls=dps::preset("Laptop Max");focusControls.device=dps::deviceCalibration("Laptop Focus");automationA.prepare(48000,focusControls);automationB.prepare(48000,focusControls);
    automationA.setComparison(true);automationB.setComparison(true);
    std::vector<float> automated(input.size()),partitioned(input.size());
    for(std::size_t offset=0;offset<96000;offset+=24000) {
        const float setting=(offset/24000)%2?1.f:0.f;
        automationA.setControls(setting,setting,setting,setting);automationB.setControls(setting,setting,setting,setting);
        watch=true;automationA.process(input.data()+2*offset,automated.data()+2*offset,24000);watch=false;
        for(std::size_t n=0;n<24000;) {const auto count=std::min<std::size_t>(127,24000-n);automationB.process(input.data()+2*(offset+n),partitioned.data()+2*(offset+n),count);n+=count;}
    }
    check(automated==partitioned,"Live control automation depends on block size");
    check(allocations==0,"Live controls allocate");
    for(float x:automated) check(std::isfinite(x)&&std::abs(x)<=dps::Engine::ceiling,"Live controls unstable or clipped");
    // LR4 low/high reconstruction is an allpass, with unity steady-state amplitude.
    for(double frequency:{60.,180.,900.,3500.,6000.}) {
        std::array<dps::Biquad,2> lo,hi;for(auto& f:lo) f.configure(dps::Biquad::Type::Lowpass,48000,900,std::sqrt(.5));for(auto& f:hi) f.configure(dps::Biquad::Type::Highpass,48000,900,std::sqrt(.5));
        double crossoverInEnergy=0,crossoverOutEnergy=0;for(int i=0;i<96000;++i) {float x=float(std::sin(2*std::numbers::pi*frequency*i/48000));float y=lo[1].tick(lo[0].tick(x))+hi[1].tick(hi[0].tick(x));if(i>48000) {crossoverInEnergy+=x*x;crossoverOutEnergy+=y*y;}}
        check(std::abs(crossoverOutEnergy/crossoverInEnergy-1)<0.002,"LR crossover reconstruction");
    }
    // Long decay and impulse stress.
    mono.prepare(48000);input.assign(96000,0);output.resize(input.size());input[0]=input[1]=16;
    mono.process(input.data(),output.data(),48000);std::fill(input.begin(),input.end(),0.f);
    for(int k=0;k<30;++k) {mono.process(input.data(),output.data(),48000);for(float x:output) check(std::isfinite(x)&&std::abs(x)<=dps::Engine::ceiling,"Decay unstable");}
    check(std::abs(output.back())<1e-15,"Decay does not converge");
    bool rejected=false;try {mono.prepare(0);} catch(const std::invalid_argument&) {rejected=true;}check(rejected,"Invalid sample rate accepted");
    auto path=std::filesystem::temp_directory_path()/"dps-roundtrip-test.wav";dps::Wav wav{48000,{0,.1f,-.3f,.8f}};dps::writeWav(path,wav);auto result=dps::readWav(path);std::filesystem::remove(path);check(wav.samples==result.samples&&result.sampleRate==48000,"Float WAV roundtrip");
    std::cout<<"PASS: 20 rate/preset combinations, clipping, finite values, blocks, in-place, reset, allocation, mono, bypass, silence, LR4, 30s decay, WAV\n";
} catch(const std::exception& e) {watch=false;std::cerr<<e.what()<<'\n';return 1;}
}
