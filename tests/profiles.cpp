#include "dps/profile.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <iterator>
#include <string>
#include <iostream>
#include <cmath>
#include <stdexcept>
#include <locale>
struct DecimalComma : std::numpunct<char> {char do_decimal_point() const override {return ',';}};
void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
int main() {
    const auto path=std::filesystem::temp_directory_path()/"dps-profile-validation.dps";
    try {
        for(const auto* device:{"Laptop","Headphones","External","Laptop Focus","Laptop Impact","Laptop Impact+","Measured"}) for(const auto* preset:{"Natural","Enhanced","Powerful","Laptop Max","RESONIQ Natural","RESONIQ Enhance","RESONIQ Power","RESONIQ Max"}) {
            dps::Profile p;p.name="Mi perfil con espacios y \"comillas\"";p.deviceName=device;p.presetName=preset;p.outputId="{test-endpoint}";
            p.parameters=dps::preset(preset);p.parameters.device=dps::deviceCalibration(device);p.parameters.intensity=.37f;p.parameters.enabled=false;p.raw=false;
            if(p.deviceName=="Measured") {p.parameters.device.highpassHz=162;p.parameters.device.bodyDb=7.3f;}
            dps::saveProfile(path,p);const auto loaded=dps::loadProfile(path);
            check(loaded.name==p.name&&loaded.outputId==p.outputId&&loaded.deviceName==device&&loaded.presetName==preset&&!loaded.raw,"Profile metadata roundtrip");
            check(loaded.parameters.intensity==.37f&&!loaded.parameters.enabled,"Saved controls lost");
            dps::Engine a,b;a.prepare(48000,p.parameters);b.prepare(48000,loaded.parameters);
            float input[2048],x[2048],y[2048];for(int i=0;i<2048;++i) input[i]=.1f*std::sin(float(i));
            a.setEnabled(true);b.setEnabled(true);a.process(input,x,1024);b.process(input,y,1024);
            for(int i=0;i<2048;++i) check(x[i]==y[i]&&std::isfinite(y[i])&&std::abs(y[i])<=dps::Engine::ceiling,"Profile changes DSP after reload");
        }
        // Legacy v1 profiles remain valid and acquire neutral feature defaults.
        dps::Profile legacy;dps::saveProfile(path,legacy);
        std::ifstream legacyIn(path);std::string legacyText,line;
        while(std::getline(legacyIn,line)) if(line.rfind("mud_",0)!=0&&line.rfind("detail_db",0)!=0&&line.rfind("harmonic_focus",0)!=0&&line.rfind("body_floor",0)!=0) legacyText+=(line=="version 2"?"version 1":line)+"\n";
        legacyIn.close();{std::ofstream out(path);out<<legacyText;}
        const auto old=dps::loadProfile(path);check(old.parameters.device.detailDb==0&&old.parameters.device.mudDb==0&&old.parameters.device.harmonicFocus==0,"Legacy profile sound changed");
        dps::Profile p;dps::saveProfile(path,p);bool failed=false;
        auto reject=[&](const std::string& text,bool append) {
            {std::ofstream out(path,append?std::ios::app:std::ios::trunc);out<<text;}
            bool failed=false;try {(void)dps::loadProfile(path);}catch(const std::exception&) {failed=true;}
            check(failed,"Malformed profile accepted");dps::saveProfile(path,p);
        };
        reject("bass 0.5\n",true);reject("unknown 1\n",true);
        reject("version 2\n",false);reject("version 1\nbass nan\n",false);
        reject("version 1\nbass 1 trailing\n",false);reject("version 1\n",false);
        {std::ofstream out(path,std::ios::app);out<<"   \n\t\n";}
        (void)dps::loadProfile(path);
        // A Spanish system locale must not change the on-disk decimal separator.
        const auto previousLocale=std::locale();
        std::locale::global(std::locale(previousLocale,new DecimalComma));
        dps::saveProfile(path,p);
        check(dps::loadProfile(path).parameters.bass==p.parameters.bass,"Locale changes profile controls");
        std::locale::global(previousLocale);
        p.deviceName="Personalizado";failed=false;
        try {dps::saveProfile(path,p);}catch(const std::exception&) {failed=true;}
        check(failed,"Unknown device label saved");p.deviceName="Laptop";
        p.parameters.bass=2;failed=false;try {dps::saveProfile(path,p);}catch(const std::exception&) {failed=true;}check(failed,"Invalid control saved");
        p.parameters.bass=.3f;p.parameters.device.highpassHz=0;failed=false;try {dps::saveProfile(path,p);}catch(const std::exception&) {failed=true;}check(failed,"Invalid calibration saved");
        p.parameters.device=dps::deviceCalibration("Laptop");p.parameters.alignedIntensity=true;p.parameters.device.transientAccentDb=4;
        dps::saveProfile(path,p);check(dps::loadProfile(path).parameters.alignedIntensity,"Experimental intensity flag lost on save");
        check(dps::loadProfile(path).parameters.device.transientAccentDb==4,"Transient accent lost on save");
        // The level control range is optional: absent means the historical
        // 6 dB, and a profile that never set it is written back unchanged.
        {
            dps::saveProfile(path,p);
            {std::ifstream saved(path);const std::string bytes((std::istreambuf_iterator<char>(saved)),{});check(bytes.find("agc_range_db")==std::string::npos,"Default level range rewrites legacy profile format");}
            check(dps::loadProfile(path).parameters.agcRangeDb==6,"Legacy profile lost its level range");
            auto fixed=p;fixed.parameters.agcRangeDb=0;dps::saveProfile(path,fixed);
            check(dps::loadProfile(path).parameters.agcRangeDb==0,"Level range lost during profile roundtrip");
            // With no range the level control cannot follow the input: a quiet and
            // a loud copy of the same passage differ by exactly their own ratio.
            std::array<float,9600> quiet{},loud{},a{},b{};
            for(std::size_t i=0;i<4800;++i) {quiet[2*i]=quiet[2*i+1]=.02f*std::sin(static_cast<float>(i)*.09f);loud[2*i]=loud[2*i+1]=4*quiet[2*i];}
            auto still=fixed.parameters;still.dynamics=0;still.device.detailDb=0;still.adaptive=false;still.device.harmonicGain=0;still.device.transientAccentDb=0;
            dps::Engine first,second;first.prepare(48000,still);second.prepare(48000,still);
            first.process(quiet.data(),a.data(),4800);second.process(loud.data(),b.data(),4800);
            float worst=0;for(std::size_t i=4800;i<9600;++i) worst=std::max(worst,std::abs(b[i]-4*a[i]));
            check(worst<1e-4f,"Fixed level range still follows the input level");
            bool rejected=false;fixed.parameters.agcRangeDb=7;
            try {dps::saveProfile(path,fixed);} catch(const std::invalid_argument&) {rejected=true;}
            check(rejected,"Out-of-range level control accepted");
            dps::saveProfile(path,p);
        }
        // Gain existed in 0.2.9 only. Its files must still load, sounding as
        // the same profile without the field, and must not be written back.
        {
            dps::saveProfile(path,p);const auto plain=dps::loadProfile(path);
            {std::ofstream appended(path,std::ios::app);appended<<"gain_db 12\n";}
            const auto withdrawn=dps::loadProfile(path);
            std::array<float,512> input{},a{},b{};
            for(std::size_t i=0;i<256;++i) input[2*i]=input[2*i+1]=.2f*std::sin(static_cast<float>(i)*.07f);
            dps::Engine first,second;first.prepare(48000,plain.parameters);second.prepare(48000,withdrawn.parameters);
            first.process(input.data(),a.data(),256);second.process(input.data(),b.data(),256);
            check(a==b,"Profile saved by 0.2.9 with Gain no longer loads at unity");
            dps::saveProfile(path,withdrawn);
            std::ifstream saved(path);const std::string bytes((std::istreambuf_iterator<char>(saved)),{});
            check(bytes.find("gain_db")==std::string::npos,"Withdrawn Gain written back to a profile");
            bool rejected=false;
            {std::ofstream broken(path,std::ios::app);broken<<"gain_db loud\n";}
            try {(void)dps::loadProfile(path);} catch(const std::exception&) {rejected=true;}
            check(rejected,"Malformed legacy Gain value accepted");
        }
        // Adopting a distributed profile on a first run: the approved sound
        // comes from the file, the endpoint and the DSP switch do not.
        dps::Profile distributed;distributed.name="Portatil - impacto maximo";distributed.deviceName="Laptop Impact+";
        distributed.outputId="not-this-machine";distributed.parameters.bass=.95f;distributed.parameters.enabled=true;
        dps::Profile here;here.outputId="{this-machine-speaker}";here.parameters.bass=.3f;here.parameters.enabled=false;
        auto adopted=dps::adoptDistributed(distributed,here,true);
        check(adopted.outputId=="{this-machine-speaker}","Adopted profile moved the output to another machine");
        check(adopted.parameters.bass==.95f&&adopted.name=="Portatil - impacto maximo","Adopted profile lost the approved sound");
        check(!adopted.parameters.enabled,"Adopted profile ignored the start-up DSP preference");
        adopted=dps::adoptDistributed(distributed,here,false);
        check(adopted.parameters.enabled,"Adopted profile dropped its own DSP state without a preference");
        check(adopted.outputId=="{this-machine-speaker}","Adopted profile moved the output without a preference");
        std::filesystem::remove(path);std::cout<<"PASS: 56 device/preset combinations, exact DSP roundtrip, locale independence, invalid and incomplete profiles, first-run adoption\n";
    }catch(const std::exception& e) {std::filesystem::remove(path);std::cerr<<e.what()<<'\n';return 1;}
}
