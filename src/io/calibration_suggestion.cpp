#include "dps/hardware.hpp"
#include "dps/profile.hpp"
#include <algorithm>
#include <array>
#include <cctype>
namespace dps {
namespace {
std::string lower(std::string value) {
    std::transform(value.begin(),value.end(),value.begin(),
        [](unsigned char c){return static_cast<char>(std::tolower(c));});
    return value;
}
bool contains(const std::string& haystack,std::string_view needle) {
    return haystack.find(needle)!=std::string::npos;
}
}
Suggestion suggestCalibration(const Hardware& hardware) {
    Suggestion s;
    const std::string model=lower(hardware.model),family=lower(hardware.family);
    const std::string maker=lower(hardware.manufacturer);

    // Headphones first: a correction meant for small speakers is wrong here,
    // and cutting the bass of a headset would be plainly worse than nothing.
    if(hardware.headphones) {
        s.deviceName="Headphones";s.calibration=deviceCalibration("Headphones");
        s.basis=Suggestion::Basis::Inferred;
        s.reason="Salida de auriculares: correccion suave, sin recorte de graves.";
        return s;
    }

    // Model profiles tuned and approved by listening. They are not laboratory
    // measurements of each endpoint connected to this machine.
    struct Known { const char* maker; const char* match; const char* profile; const char* what; };
    static constexpr std::array<Known,1> known{{
        {"lenovo","83lu","Laptop Impact+","Lenovo Legion Pro 5 83LU"},
    }};
    for(const auto& k:known) {
        if(!contains(maker,k.maker)) continue;
        if(!contains(model,k.match)&&!contains(family,k.match)) continue;
        s.deviceName=k.profile;s.calibration=deviceCalibration(k.profile);
        s.basis=Suggestion::Basis::Tuned;
        s.reason=std::string("Ajuste preparado para ")+k.what+".";
        return s;
    }

    // Nothing measured for this machine. Infer from the chassis: laptop
    // speakers share the same problem, little below 150-200 Hz and dull mids,
    // so a moderate version of the laptop correction is a reasonable guess.
    if(hardware.portable) {
        s.deviceName="Laptop Focus";s.calibration=deviceCalibration("Laptop Focus");
        s.basis=Suggestion::Basis::Inferred;
        s.reason="Portatil no medido: correccion estimada para altavoces pequenos.";
        return s;
    }

    s.deviceName="External";s.calibration=deviceCalibration("External");
    s.basis=Suggestion::Basis::Generic;
    s.reason="Equipo no reconocido: ajuste neutro. Elige un perfil si no encaja.";
    return s;
}
}
