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
        auto base=dps::preset("RESONIQ Max");base.device=dps::deviceCalibration("Laptop Impact+");
        base.bass=.95f;base.clarity=.9f;base.dynamics=.85f;base.spatial=.3f;
        base.agcAttackMs=60;base.agcReleaseMs=900;
        auto candidate=base;candidate.device.transientAccentDb=4.f;
        constexpr std::size_t rate=48000,frames=rate*8;
        std::vector<float> input(frames*2),reference(input.size()),output(input.size()),pieces(input.size());
        double bestContrast=-100;
        for(float amplitude:{.08f,.16f,.32f}) {
            for(std::size_t frame=0;frame<frames;++frame) {
                const double t=double(frame)/rate,phase=std::fmod(t,.5);
                const double burst=phase<.12?std::exp(-phase/.045):0;
                const float sample=static_cast<float>(.025*std::sin(2*std::numbers::pi*90*t)+
                    amplitude*burst*std::sin(2*std::numbers::pi*1800*t));
                input[2*frame]=input[2*frame+1]=sample;
            }
            dps::Engine a,b,c;a.prepare(rate,base);b.prepare(rate,candidate);c.prepare(rate,candidate);
            a.process(input.data(),reference.data(),frames);b.process(input.data(),output.data(),frames);
            for(std::size_t offset=0;offset<frames;offset+=137) {
                const auto count=std::min<std::size_t>(137,frames-offset);
                c.process(input.data()+offset*2,pieces.data()+offset*2,count);
            }
            check(output==pieces,"Attack variant depends on block size");
            double oldTotal=0,newTotal=0,oldAttack=0,newAttack=0,oldBody=0,newBody=0;
            for(std::size_t frame=rate*6;frame<frames;++frame) {
                const double old=reference[2*frame],current=output[2*frame],phase=std::fmod(double(frame)/rate,.5);
                check(std::isfinite(current)&&std::abs(current)<=dps::Engine::ceiling,"Attack variant exceeds ceiling");
                check(output[2*frame]==output[2*frame+1],"Attack variant breaks mono");
                oldTotal+=old*old;newTotal+=current*current;
                if(phase<.02) {oldAttack+=old*old;newAttack+=current*current;}
                if(phase>=.04&&phase<.12) {oldBody+=old*old;newBody+=current*current;}
            }
            const double contrast=10*std::log10((newAttack/newTotal)/(oldAttack/oldTotal));
            const double body=10*std::log10((newBody/newTotal)/(oldBody/oldTotal));
            const double level=10*std::log10(newTotal/oldTotal);
            bestContrast=std::max(bestContrast,contrast);
            std::cout<<"input_burst="<<amplitude<<" attack_contrast_db="<<contrast
                <<" body_relative_db="<<body<<" total_rms_delta_db="<<level<<'\n';
            check(body>-.8,"Attack variant loses too much burst body");
            check(std::abs(level)<1.5,"Attack variant is mostly a level increase");
        }
        check(bestContrast>.3,"Short accent does not preserve attacks");
        for(double sampleRate:{16000.,44100.,48000.,96000.,192000.}) {
            dps::AttackAccent accent;accent.prepare(sampleRate);
            for(int i=0;i<1000;++i) check(accent.tick(.00001f,4)==1,"Accent raises low noise");
            for(int i=0;i<static_cast<int>(sampleRate);++i) accent.tick(.05f,4);
            check(std::abs(accent.tick(.05f,4)-1)<.0001f,"Accent repeatedly boosts sustained signal");
            accent.reset();check(accent.tick(0,4)==1,"Accent reset creates signal");
        }
        std::cout<<"PASS: attack contrast, body, level, sample safety, mono and block continuity\n";
    } catch(const std::exception& error){std::cerr<<error.what()<<'\n';return 1;}
}
