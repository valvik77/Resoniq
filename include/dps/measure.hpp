#pragma once
#include "dps/engine.hpp"
#include "dps/audio_gap.hpp"
#include <span>
#include <array>
#include <cstddef>
#include <string>
#include <vector>
namespace dps {
// Acoustic measurement of the speakers using the machine's own microphone.
// The bands are wide on purpose: a laptop microphone cannot resolve more than
// the broad shape of the response, and pretending otherwise would invent
// detail that is not there.
inline constexpr std::array<float,8> measureBands{60,120,250,500,1000,2000,4000,8000};

struct Measurement {
    std::array<float,8> levelDb{};   // relative to the 250 Hz-1 kHz midrange mean
    float noiseFloorDb=-120;         // room noise measured before the sweep
    float signalDb=-120;             // level of the sweep itself
    float alignmentMs=0;             // shared delay fitted using midrange tones
    unsigned reliableBands=0;        // midrange tones above noise and coherent
    bool usable=false;               // enough signal above the noise to mean anything
    std::string warning;             // why it is not usable, or what to be careful about
};

// Analyse a recording of the test tones. The recording is mono float32.
Measurement analyseSweep(const float* recorded,std::size_t frames,double rate,
                         const float* silence,std::size_t silenceFrames,std::span<const AudioGap> gaps={});
// Choose a coherent microphone channel; averaging opposite phases can erase
// test tones on microphone arrays. Never choose by peak/loudness alone.
unsigned selectMeasurementChannel(const Measurement& left,const Measurement& right) noexcept;

// Turn a measurement into a starting calibration. This is a suggestion from one
// microphone in one position: it is a starting point to listen to, not a
// finished profile.
DeviceCalibration calibrationFrom(const Measurement& measurement,bool portable);
DeviceCalibration calibrationFrom(const Measurement& measurement,bool portable,const DeviceCalibration& voicing);

// The test signal: short tones at each band, so a cheap microphone and a noisy
// room still give a usable reading. Returns the stereo samples to play.
std::vector<float> sweepSignal(double rate,float amplitude);
inline constexpr double sweepToneSeconds=0.45;
inline constexpr double sweepGapSeconds=0.15;
}
