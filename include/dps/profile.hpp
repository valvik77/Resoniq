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
Profile loadProfile(const std::filesystem::path& path);
void saveProfile(const std::filesystem::path& path,const Profile& profile);
}
