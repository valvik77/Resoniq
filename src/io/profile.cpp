#include "dps/profile.hpp"
#include "atomic_file.hpp"
#include <fstream>
#include <sstream>
#include <iomanip>
#include <set>
#include <cmath>
#include <stdexcept>
#include <locale>
namespace dps {
DeviceCalibration deviceCalibration(std::string_view name) {
    if(name=="Laptop") return {};
    if(name=="Laptop Focus") return {130,320,11,2800,11,.9f,.4f,650,-7,6,.8f,.85f};
    // Approved by the user after listening on the Legion's speakers: holds the
    // effect on loud material instead of letting the AGC gain fall away.
    if(name=="Laptop Impact") return {175,300,11,3000,10,1.6f,.5f,600,-6,6,.85f,.9f,.15f,1.45f};
    // Same voicing with the band compressor reaching into ordinary programme
    // level, which is what makes it read as consistently dense.
    if(name=="Laptop Impact+") return {175,300,11,3000,10,1.6f,.5f,600,-6,6,.85f,.9f,.15f,1.45f,1.8f};
    if(name=="Headphones") return {20,150,2,2600,3,0,0.15f};
    if(name=="External") return {25,150,3,2600,3,0.3f,0.3f};
    // A measured profile carries its own values in the file: there is no fixed
    // calibration to look up, so it falls back to the laptop defaults only when
    // asked for one out of context.
    if(name=="Measured") return deviceCalibration("Laptop Impact");
    throw std::invalid_argument("Device profile must be Laptop, Laptop Focus, Laptop Impact, Laptop Impact+, Measured, Headphones or External");
}
namespace {
void validate(const Profile& p) {
    // Full DSP validation without touching the running audio instance.
    for(float v:{p.parameters.bass,p.parameters.clarity,p.parameters.dynamics,p.parameters.spatial,p.parameters.intensity})
        if(!std::isfinite(v)||v<0||v>1) throw std::invalid_argument("Profile control outside 0..1");
    (void)preset(p.presetName);
    (void)deviceCalibration(p.deviceName);
    for(const auto* s:{&p.name,&p.deviceName,&p.presetName,&p.outputId})
        if(s->size()>1024||s->find_first_of("\r\n")!=std::string::npos) throw std::invalid_argument("Invalid profile text");
    Engine engine;engine.prepare(48000,p.parameters);
}
}
Profile loadProfile(const std::filesystem::path& path) {
    if(std::filesystem::file_size(path)>16384) throw std::runtime_error("Profile too large");
    std::ifstream in(path);if(!in) throw std::runtime_error("Cannot open profile");
    Profile p;std::set<std::string> keys;std::string line;std::size_t bytes=0;int version=0;
    while(std::getline(in,line)) {
        bytes+=line.size();if(bytes>16384) throw std::runtime_error("Profile too large");
        if(line.empty()||line[0]=='#') continue;
        std::istringstream row(line);row.imbue(std::locale::classic());std::string key;row>>key;
        if(key.empty()) continue;
        if(!keys.insert(key).second) throw std::runtime_error("Duplicate profile field: "+key);
        auto& v=p.parameters;auto& d=v.device;
        if(key=="version") {row>>version;if(version!=1&&version!=2) throw std::runtime_error("Unsupported profile version");}
        else if(key=="name") row>>std::quoted(p.name);
        else if(key=="device") row>>std::quoted(p.deviceName);
        else if(key=="preset") row>>std::quoted(p.presetName);
        else if(key=="output_id") row>>std::quoted(p.outputId);
        else if(key=="raw") row>>p.raw;
        else if(key=="enabled") row>>v.enabled;
        else if(key=="adaptive") row>>v.adaptive;
        else if(key=="aligned_intensity") row>>v.alignedIntensity;
        else if(key=="bass") row>>v.bass;
        else if(key=="clarity") row>>v.clarity;
        else if(key=="dynamics") row>>v.dynamics;
        else if(key=="spatial") row>>v.spatial;
        else if(key=="intensity") row>>v.intensity;
        else if(key=="gain_db") row>>v.gainDb;
        else if(key=="agc_attack_ms") row>>v.agcAttackMs;
        else if(key=="agc_release_ms") row>>v.agcReleaseMs;
        else if(key=="highpass_hz") row>>d.highpassHz;
        else if(key=="body_hz") row>>d.bodyHz;
        else if(key=="body_db") row>>d.bodyDb;
        else if(key=="presence_hz") row>>d.presenceHz;
        else if(key=="presence_db") row>>d.presenceDb;
        else if(key=="harmonic_gain") row>>d.harmonicGain;
        else if(key=="stereo_amount") row>>d.stereoAmount;
        else if(key=="mud_hz") row>>d.mudHz;
        else if(key=="mud_db") row>>d.mudDb;
        else if(key=="detail_db") row>>d.detailDb;
        else if(key=="harmonic_focus") row>>d.harmonicFocus;
        else if(key=="body_floor") row>>d.bodyFloor;
        // Optional even in v2: profiles written before it keep the historical
        // 0.7 default, so their sound does not change when reloaded.
        else if(key=="adaptive_depth") row>>d.adaptiveDepth;
        else if(key=="loud_density") row>>d.loudDensity;
        else if(key=="band_density") row>>d.bandDensity;
        else if(key=="transient_accent_db") row>>d.transientAccentDb;
        else throw std::runtime_error("Unknown profile field: "+key);
        if(!row) throw std::runtime_error("Invalid profile value: "+key);
        row>>std::ws;if(!row.eof()) throw std::runtime_error("Trailing profile data: "+key);
    }
    if(in.bad()) throw std::runtime_error("Cannot read profile");
    constexpr const char* required[]={"version","name","device","preset","output_id","raw","enabled","adaptive","bass","clarity","dynamics","spatial","intensity","agc_attack_ms","agc_release_ms","highpass_hz","body_hz","body_db","presence_hz","presence_db","harmonic_gain","stereo_amount"};
    for(const auto* key:required) if(!keys.contains(key)) throw std::runtime_error(std::string("Missing profile field: ")+key);
    if(version==2) for(const auto* key:{"mud_hz","mud_db","detail_db","harmonic_focus","body_floor"})
        if(!keys.contains(key)) throw std::runtime_error(std::string("Missing v2 profile field: ")+key);
    validate(p);return p;
}
void saveProfile(const std::filesystem::path& path,const Profile& p) {
    validate(p);detail::AtomicFile file(path);auto& out=file.stream();out.imbue(std::locale::classic());
    const auto& v=p.parameters;const auto& d=v.device;
    out<<std::setprecision(9)<<"version 2\nname "<<std::quoted(p.name)<<"\ndevice "<<std::quoted(p.deviceName)<<"\npreset "<<std::quoted(p.presetName)<<"\noutput_id "<<std::quoted(p.outputId)<<"\nraw "<<p.raw
       <<"\nenabled "<<v.enabled<<"\nadaptive "<<v.adaptive<<"\nbass "<<v.bass<<"\nclarity "<<v.clarity<<"\ndynamics "<<v.dynamics<<"\nspatial "<<v.spatial<<"\nintensity "<<v.intensity
       <<"\nagc_attack_ms "<<v.agcAttackMs<<"\nagc_release_ms "<<v.agcReleaseMs<<"\nhighpass_hz "<<d.highpassHz<<"\nbody_hz "<<d.bodyHz<<"\nbody_db "<<d.bodyDb<<"\npresence_hz "<<d.presenceHz<<"\npresence_db "<<d.presenceDb<<"\nharmonic_gain "<<d.harmonicGain<<"\nstereo_amount "<<d.stereoAmount<<"\nmud_hz "<<d.mudHz<<"\nmud_db "<<d.mudDb<<"\ndetail_db "<<d.detailDb<<"\nharmonic_focus "<<d.harmonicFocus<<"\nbody_floor "<<d.bodyFloor<<"\nadaptive_depth "<<d.adaptiveDepth<<"\nloud_density "<<d.loudDensity<<"\nband_density "<<d.bandDensity<<'\n';
    if(v.alignedIntensity) out<<"aligned_intensity 1\n";
    if(v.gainDb!=0) out<<"gain_db "<<v.gainDb<<'\n';
    if(d.transientAccentDb>0) out<<"transient_accent_db "<<d.transientAccentDb<<'\n';
    file.commit();
}
}
