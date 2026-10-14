#pragma once
#include "dps/engine.hpp"
#include <string>
namespace dps {
// What we can learn about the machine and the endpoint without measuring it.
struct Hardware {
    std::string manufacturer,model,family; // from BIOS registry values
    std::string endpoint;                  // the render endpoint's friendly name
    bool portable=false;                   // laptop, tablet or similar
    bool headphones=false;                 // endpoint looks like a headset
};
Hardware detectHardware(const std::wstring& endpointName);

// A calibration chosen for that machine, plus how it was arrived at.
struct Suggestion {
    std::string deviceName;      // one of the named device profiles, when it fits
    DeviceCalibration calibration{};
    // tuned: a profile approved by listening on this exact model.
    // inferred: derived from what the system reports, not from a measurement.
    // generic: nothing useful was known.
    enum class Basis { Tuned, Inferred, Generic, Measured=Tuned } basis=Basis::Generic;
    std::string reason;          // short text for the user, in Spanish
};
Suggestion suggestCalibration(const Hardware& hardware);

// The file under profiles/ that carries the whole approved profile for a
// device name: controls, AGC and density as they were listened to, not only
// the tone correction. Empty when nothing is distributed for that name.
std::string approvedProfileFile(const std::string& deviceName);
}
