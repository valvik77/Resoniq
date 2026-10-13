#include "dps/profile.hpp"
#include <algorithm>
#include <cmath>
#include <iostream>
#include <numbers>
#include <vector>
// Offline electrical characterization. These numbers describe processing,
// not acoustic response or listening preference. Never changes a profile.
int main(int argc,char** argv) {
    if(argc<2||argc>3) {std::cerr<<"Usage: dps_quality PROFILE [FREQUENCY_HZ]\n";return 2;}
    try {
        const auto profile=dps::loadProfile(argv[1]);
        double frequency=1000;
        if(argc==3) {std::size_t used=0;frequency=std::stod(argv[2],&used);
            if(used!=std::string_view(argv[2]).size()||!std::isfinite(frequency)||frequency<20||frequency>10000)
                throw std::invalid_argument("Frequency must be 20..10000 Hz");}
        constexpr std::size_t rate=48000,frames=rate*8,begin=rate*6;
        std::cout<<"signal,input_scale,intensity,gain_db,output_peak,crest_db,harmonics_db\n";
        for(bool programme:{false,true}) for(float level:{.02f,.08f,.25f,.6f})
            for(float intensity:{0.f,.25f,.5f,.75f,1.f}) {
                auto parameters=profile.parameters;parameters.enabled=true;parameters.intensity=intensity;
                dps::Engine engine;engine.prepare(rate,parameters);
                std::vector<float> input(frames*2),output(input.size());
                for(std::size_t frame=0;frame<frames;++frame) {
                    const double t=2*std::numbers::pi*frame/rate;
                    const double sample=programme
                        ? .45*std::sin(90*t)+.3*std::sin(650*t)+.15*std::sin(2800*t)+.1*std::sin(6000*t)
                        : std::sin(frequency*t);
                    input[2*frame]=input[2*frame+1]=static_cast<float>(level*sample);
                }
                engine.process(input.data(),output.data(),frames);
                double dryEnergy=0,wetEnergy=0,peak=0;
                std::array<double,5> real{},imaginary{};
                for(std::size_t frame=begin;frame<frames;++frame) {
                    const double sample=output[2*frame];dryEnergy+=input[2*frame]*input[2*frame];
                    wetEnergy+=sample*sample;peak=std::max(peak,std::abs(sample));
                    if(!programme) for(std::size_t harmonic=0;harmonic<5;++harmonic) {
                        if(frequency*(harmonic+1)>=rate/2) continue;
                        const double angle=2*std::numbers::pi*frequency*(harmonic+1)*frame/rate;
                        real[harmonic]+=sample*std::cos(angle);imaginary[harmonic]+=sample*std::sin(angle);
                    }
                }
                const double rms=std::sqrt(wetEnergy/(frames-begin));
                double harmonicEnergy=0;
                for(std::size_t harmonic=1;harmonic<5;++harmonic)
                    harmonicEnergy+=real[harmonic]*real[harmonic]+imaginary[harmonic]*imaginary[harmonic];
                std::cout<<(programme?"multitone":"sine_"+std::to_string(frequency)+"hz")<<','<<level<<','<<intensity<<','
                    <<10*std::log10(std::max(wetEnergy/dryEnergy,1e-12))<<','<<peak<<','
                    <<20*std::log10(std::max(peak/std::max(rms,1e-12),1e-12))<<',';
                if(programme) std::cout<<"n/a";
                else std::cout<<10*std::log10(std::max(harmonicEnergy/
                    std::max(real[0]*real[0]+imaginary[0]*imaginary[0],1e-12),1e-12));
                std::cout<<'\n';
            }
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
