#include "dps/profile.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <stdexcept>
#include <vector>
void check(bool value,const char* message){if(!value) throw std::runtime_error(message);}
int main() {
    try {
        auto p=dps::preset("RESONIQ Max");p.device=dps::deviceCalibration("Laptop Impact+");
        p.bass=.95f;p.clarity=.9f;p.dynamics=.85f;p.spatial=.3f;p.agcAttackMs=60;p.agcReleaseMs=900;
        std::vector<float> input(48000*4),legacy(input.size()),aligned(input.size());
        for(std::size_t i=0;i<input.size()/2;++i)
            input[2*i]=input[2*i+1]=.02f*static_cast<float>(std::sin(2*std::numbers::pi*1000*i/48000));
        for(float amount:{0.f,1.f}) {
            p.intensity=amount;p.alignedIntensity=false;dps::Engine a,b;a.prepare(48000,p);
            p.alignedIntensity=true;b.prepare(48000,p);
            a.process(input.data(),legacy.data(),input.size()/2);b.process(input.data(),aligned.data(),input.size()/2);
            check(legacy==aligned,"Aligned intensity changes endpoints");
        }
        p.intensity=.25f;p.alignedIntensity=false;dps::Engine a,b;a.prepare(48000,p);
        p.alignedIntensity=true;b.prepare(48000,p);
        a.process(input.data(),legacy.data(),input.size()/2);b.process(input.data(),aligned.data(),input.size()/2);
        double oldEnergy=0,newEnergy=0,inputEnergy=0;
        for(std::size_t i=input.size()/2;i<input.size();++i) {
            oldEnergy+=legacy[i]*legacy[i];newEnergy+=aligned[i]*aligned[i];inputEnergy+=input[i]*input[i];
        }
        std::cout<<"1kHz/25% legacy="<<10*std::log10(oldEnergy/inputEnergy)
            <<" aligned="<<10*std::log10(newEnergy/inputEnergy)<<" dB\n";
        check(newEnergy>oldEnergy*4,"Quarter intensity still has strong phase cancellation");
        dps::Engine blocked;blocked.prepare(48000,p);std::vector<float> pieces(input.size());
        for(std::size_t i=0;i<input.size()/2;i+=113) {
            const auto count=std::min<std::size_t>(113,input.size()/2-i);
            blocked.process(input.data()+2*i,pieces.data()+2*i,count);
        }
        check(pieces==aligned,"Phase reference depends on block size");
        b.prepare(48000,p);float previous=0;
        for(float target:{0.f,.01f,.1f,.25f,.5f,.75f,1.f,0.f}) {
            b.setIntensity(target);b.process(input.data(),aligned.data(),input.size()/2);
            for(std::size_t i=0;i<aligned.size();i+=2) {
                check(std::abs(aligned[i]-previous)<.04f,"Intensity automation has an abrupt jump");
                previous=aligned[i];
            }
        }
        b.setEnabled(false);for(int repeat=0;repeat<8;++repeat) b.process(input.data(),aligned.data(),input.size()/2);
        check(std::abs(aligned.back()-input.back())<1e-5,"Bypass does not return to original");
        for(double rate:{16000.,48000.,192000.}) {
            b.prepare(rate,p);b.process(input.data(),aligned.data(),input.size()/2);
            for(float sample:aligned) check(std::isfinite(sample)&&std::abs(sample)<=dps::Engine::ceiling,"Aligned mix unstable");
        }
        std::cout<<"PASS: exact zero/full, cancellation reduction, block continuity, bypass and rate stability\n";
    }catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
