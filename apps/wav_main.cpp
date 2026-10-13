#include "dps/engine.hpp"
#include "dps/profile.hpp"
#include "dps/wav.hpp"
#include <algorithm>
#include <filesystem>
#include <iostream>
int main(int argc,char** argv) {
    if(argc<3||argc>5||(argc==5&&std::string_view(argv[3])!="--profile")) {std::cerr<<"Usage: dps_wav input.wav output.wav [Natural|Enhanced|Powerful|\"Laptop Max\"]\n       dps_wav input.wav output.wav --profile FILE\n";return 2;}
    try {
        if(std::filesystem::weakly_canonical(argv[1])==std::filesystem::weakly_canonical(argv[2])) throw std::runtime_error("Input and output must differ");
        auto wav=dps::readWav(argv[1]);dps::Engine engine;engine.prepare(wav.sampleRate,argc==5?dps::loadProfile(argv[4]).parameters:dps::preset(argc==4?argv[3]:"Enhanced"));
        dps::Meters meters;
        for(std::size_t i=0;i<wav.samples.size()/2;i+=256) {auto n=std::min<std::size_t>(256,wav.samples.size()/2-i);engine.process(wav.samples.data()+2*i,wav.samples.data()+2*i,n);auto m=engine.meters();meters.inputPeak=std::max(meters.inputPeak,m.inputPeak);meters.outputPeak=std::max(meters.outputPeak,m.outputPeak);meters.gainReductionDb=std::max(meters.gainReductionDb,m.gainReductionDb);}
        dps::writeWav(argv[2],wav);std::cout<<"Frames: "<<wav.samples.size()/2<<" Input peak: "<<meters.inputPeak<<" Output peak: "<<meters.outputPeak<<" Max gain reduction (dB): "<<meters.gainReductionDb<<'\n';
    } catch(const std::exception& e) {std::cerr<<e.what()<<'\n';return 1;}
}
