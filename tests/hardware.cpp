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
        std::cout<<"PASS: known model, headphones first, family fallback, inferred laptop and generic desktop\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
