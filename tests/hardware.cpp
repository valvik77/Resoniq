#include "dps/hardware.hpp"
#include <iostream>
#include <stdexcept>
void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
int main() {
    try {
        dps::Hardware hardware;
        hardware.manufacturer="LENOVO";hardware.model="83LU";hardware.portable=true;
        auto suggestion=dps::suggestCalibration(hardware);
        check(suggestion.deviceName=="Laptop Impact+"&&suggestion.basis==dps::Suggestion::Basis::Tuned,
              "Known model lost its approved profile");
        hardware.headphones=true;suggestion=dps::suggestCalibration(hardware);
        check(suggestion.deviceName=="Headphones"&&suggestion.calibration.highpassHz<=60,
              "Known laptop overrides headphone correction");
        hardware.headphones=false;hardware.model="unknown";
        check(dps::suggestCalibration(hardware).basis==dps::Suggestion::Basis::Inferred,
              "Unknown laptop claims model calibration");
        hardware.portable=false;suggestion=dps::suggestCalibration(hardware);
        check(suggestion.deviceName=="External"&&suggestion.basis==dps::Suggestion::Basis::Generic,
              "Unknown desktop does not use generic calibration");
        hardware.model="unknown";hardware.family="LEGION 83LU";hardware.portable=true;
        check(dps::suggestCalibration(hardware).deviceName=="Laptop Impact+","Family model match failed");
        // A first run adopts the whole approved profile, so every device name
        // a detection can return must name a distributed file.
        hardware.model="83LU";hardware.family.clear();
        const auto tuned=dps::suggestCalibration(hardware);
        check(tuned.basis==dps::Suggestion::Basis::Tuned,"Known model lost its tuned basis");
        check(dps::approvedProfileFile(tuned.deviceName)=="laptop-impact-plus.dps",
              "Approved model has no distributed profile to start from");
        for(const auto* name:{"Laptop","Laptop Focus","Laptop Impact","Laptop Impact+","Headphones","External"})
            check(!dps::approvedProfileFile(name).empty(),"Detected device name without a distributed profile");
        // A measurement belongs to the machine that made it, and an unknown
        // name must not be answered with some other device's file.
        check(dps::approvedProfileFile("Measured").empty(),"Measured profile taken from the distribution");
        check(dps::approvedProfileFile("").empty()&&dps::approvedProfileFile("Laptop Impact++").empty(),
              "Unknown device name matched a distributed profile");
        std::cout<<"PASS: known model, headphones first, family fallback, inferred laptop, generic desktop and distributed profile names\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
