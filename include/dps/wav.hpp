#pragma once
#include <cstdint>
#include <filesystem>
#include <vector>
namespace dps {
struct Wav { std::uint32_t sampleRate; std::vector<float> samples; };
Wav readWav(const std::filesystem::path& path);
void writeWav(const std::filesystem::path& path, const Wav& wav);
}
