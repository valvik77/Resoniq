#include "dps/transient.hpp"
#include "dps/profile.hpp"
#include <vector>
#include <iostream>
#include <stdexcept>
#include <numbers>
#include <limits>
void check(bool b,const char* m){if(!b) throw std::runtime_error(m);}
int main(){try {
    for(double rate:{16000.,44100.,48000.,96000.,192000.}) {
        dps::TransientShaper t;t.prepare(rate);
        for(int i=0;i<rate;++i) check(t.tick(.00001f,6)==1,"Below-gate noise is lifted");
        float maximum=1;
        for(int i=0;i<rate*.05;++i) maximum=std::max(maximum,t.tick(.1f,6));
        check(maximum>1.5f,"Onset not enhanced");
        float settled=1;for(int i=0;i<rate*2;++i) settled=t.tick(.1f,6);
        check(std::abs(settled-1)<.002,"Sustained level receives constant gain");
        check(std::isfinite(t.tick(std::numeric_limits<float>::infinity(),6)),"Detector unstable");
        t.reset();check(t.tick(0,6)==1,"Detector reset/silence failure");
    }
    constexpr int rate=48000,frames=rate*12;
    std::vector<float> input(frames*2),a(input.size()),b(input.size()),partitioned(input.size());
    // Repeated mid-band bursts over a sustained low-frequency bed.
    for(int i=0;i<frames;++i) {
        const double time=double(i)/rate,phase=std::fmod(time,.5);
        const double burst=phase<.08?std::exp(-phase/.025):0;
        const float x=float(.09*std::sin(2*std::numbers::pi*90*time)+.07*burst*std::sin(2*std::numbers::pi*1800*time));
        input[2*i]=input[2*i+1]=x;
    }
    dps::Parameters plain;plain.bass=plain.dynamics=plain.spatial=0;plain.clarity=1;
    plain.device.highpassHz=20;plain.device.presenceDb=0;plain.device.detailDb=0;
    auto articulated=plain;articulated.device.detailDb=6;
    dps::Engine x,y,z;x.prepare(rate,plain);y.prepare(rate,articulated);z.prepare(rate,articulated);
    x.setComparison(true);y.setComparison(true);z.setComparison(true);
    x.process(input.data(),a.data(),frames);y.process(input.data(),b.data(),frames);
    for(int i=0;i<frames;) {const auto n=std::min(137,frames-i);z.process(input.data()+2*i,partitioned.data()+2*i,n);i+=n;}
    check(b==partitioned,"Articulation depends on block boundaries");
    check(y.meters().transientLiftDb>2,"Transient meter fails to report processing");
    double ae=0,be=0,attackA=0,attackB=0;
    for(int i=rate*10;i<frames;++i) {
        check(std::isfinite(b[2*i])&&std::abs(b[2*i])<=dps::Engine::ceiling,"Articulation clips");
        check(b[2*i]==b[2*i+1],"Articulation breaks mono compatibility");
        ae+=double(a[2*i])*a[2*i];be+=double(b[2*i])*b[2*i];
        const double phase=std::fmod(double(i)/rate,.5);
        if(phase<.025) {attackA+=double(a[2*i])*a[2*i];attackB+=double(b[2*i])*b[2*i];}
    }
    const double mismatch=10*std::log10(be/ae);
    const double contrast=10*std::log10((attackB/be)/(attackA/ae));
    std::cout<<"Matched-programme RMS delta="<<mismatch<<" dB; attack energy contrast="<<contrast<<" dB\n";
    check(std::abs(mismatch)<.5,"Articulation comparison not matched");
    check(contrast>.4,"Articulation disappears after level matching");
    std::cout<<"PASS: gated onsets, sustained unity, reset, matched transient contrast, stereo and block continuity\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
