#include "dps/profile.hpp"
#include <algorithm>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <vector>
#include <random>
int main(int argc,char** argv) {
    if(argc>2) {std::cerr<<"Usage: dps_benchmark [PROFILE]\n";return 2;}
    dps::Parameters selected;
    try {if(argc==2) selected=dps::loadProfile(argv[1]).parameters;}
    catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
    std::cout<<"rate,frames,comparison,median_ms,p99_ms,max_ms,deadline_ms,p99_cpu_percent\n";
    for(double rate:{48000.,192000.}) for(std::size_t frames:{128,480,960}) for(bool matched:{false,true}) {
        auto p=dps::preset("Laptop Max");p.device=dps::deviceCalibration("Laptop Focus");p.clarity=.75f;p.dynamics=.5f;
        if(argc==2) p=selected;
        dps::Engine engine;engine.prepare(rate,p);engine.setComparison(matched);
        std::vector<float> in(frames*2),out(in.size());std::vector<double> times(500);
        std::mt19937 rng(41);std::uniform_real_distribution<float> noise(-.4f,.4f);for(auto& v:in) v=noise(rng);
        for(int n=0;n<20;++n) engine.process(in.data(),out.data(),frames);
        for(auto& duration:times) {const auto start=std::chrono::steady_clock::now();engine.process(in.data(),out.data(),frames);duration=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();}
        std::sort(times.begin(),times.end());const double deadline=1000*frames/rate;
        std::cout<<std::fixed<<std::setprecision(5)<<rate<<','<<frames<<','<<matched<<','<<times[250]<<','<<times[494]<<','<<times.back()<<','<<deadline<<','<<times[494]/deadline*100<<'\n';
    }
}
