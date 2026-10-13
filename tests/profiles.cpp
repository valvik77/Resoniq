#include "dps/profile.hpp"
#include <fstream>
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
        p.parameters.gainDb=6;dps::saveProfile(path,p);
        check(dps::loadProfile(path).parameters.gainDb==6,"Gain lost during profile roundtrip");
        p.parameters.gainDb=0;dps::saveProfile(path,p);
        {std::ifstream input(path);const std::string bytes((std::istreambuf_iterator<char>(input)),{});check(bytes.find("gain_db")==std::string::npos,"Zero Gain rewrites legacy profile format");}
        std::filesystem::remove(path);std::cout<<"PASS: 56 device/preset combinations, exact DSP roundtrip, locale independence, invalid and incomplete profiles\n";
    }catch(const std::exception& e) {std::filesystem::remove(path);std::cerr<<e.what()<<'\n';return 1;}
}
