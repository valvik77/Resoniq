#pragma once
#include "dps/engine.hpp"
#include <filesystem>
#include <string>
namespace dps {
struct Profile {
    std::string name="RESONIQ Enhance",deviceName="Laptop",presetName="RESONIQ Enhance",outputId;
    Parameters parameters{};
    bool raw=true;
};
DeviceCalibration deviceCalibration(std::string_view name);
// A distributed profile adopted on a first run. The sound comes from the file,
// as it was approved; the endpoint belongs to this machine and the DSP switch
// to the start-up preference, so neither is taken from the file.
inline Profile adoptDistributed(Profile distributed,const Profile& current,bool keepEnabled) {
    distributed.outputId=current.outputId;
    if(keepEnabled) distributed.parameters.enabled=current.parameters.enabled;
    return distributed;
}
Profile loadProfile(const std::filesystem::path& path);
void saveProfile(const std::filesystem::path& path,const Profile& profile);
}
