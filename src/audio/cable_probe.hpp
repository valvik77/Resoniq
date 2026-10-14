#pragma once
#include <algorithm>
#include <atomic>
#include <bit>
#include <cmath>
#include <cstddef>
#include <cstdint>

namespace dps::detail {
// VB-CABLE either attenuates its own loopback (its Windows Volume Control
// option is live) or ignores the endpoint volume entirely (option off, or set
// in the registry but not yet read by the driver). Both states report the same
// hardware volume support, and one machine moves from one to the other after a
// restart, so the host measures it instead of assuming: a short pilot is
// rendered into the cable and its level is read back from the loopback.
enum class CableVolume {Unknown,Attenuates,Ignores};
inline constexpr double pilotHz=15000;
inline constexpr float pilotAmplitude=.008f; // -42 dBFS for 0.6 s: clear of programme energy in its 10 Hz bin
inline constexpr unsigned pilotWindow=4800;  // 100 ms at 48 kHz: 1500 whole pilot cycles

// Audio thread: single-bin level of the captured loopback, published once per
// window. No allocation, locks or COM.
class PilotMeter {
    double s1_=0,s2_=0;unsigned count_=0;
    unsigned windows_=0; // audio thread only
    std::atomic<std::uint64_t> published_{0};
    std::atomic<unsigned> discontinuities_{0};
public:
    struct Reading {float amplitude;unsigned windows;};
    static_assert(std::atomic<std::uint64_t>::is_always_lock_free);
    Reading reading() const noexcept {
        const auto value=published_.load(std::memory_order_acquire);
        return {std::bit_cast<float>(static_cast<std::uint32_t>(value)),static_cast<unsigned>(value>>32)};
    }
    void discontinuity() noexcept {
        s1_=s2_=0;count_=0;
        discontinuities_.fetch_add(1,std::memory_order_release);
    }
    unsigned revision() const noexcept {return discontinuities_.load(std::memory_order_acquire);}
    void push(float left,float right) noexcept {
        static constexpr double coefficient=-0.76536686473017954; // 2 cos(2 pi 15000/48000)
        const double s0=.5*(static_cast<double>(left)+right)+coefficient*s1_-s2_;s2_=s1_;s1_=s0;
        if(++count_<pilotWindow) return;
        const double power=std::max(s1_*s1_+s2_*s2_-coefficient*s1_*s2_,0.0);
        const float amplitude=static_cast<float>(2*std::sqrt(power)/pilotWindow);
        // Publish level and window number together so the observer cannot pair
        // the next window's level with the previous window's number.
        published_.store((static_cast<std::uint64_t>(++windows_)<<32)|std::bit_cast<std::uint32_t>(amplitude),std::memory_order_release);
        s1_=s2_=0;count_=0;
    }
    float amplitude() const noexcept {return reading().amplitude;}
    unsigned windows() const noexcept {return reading().windows;}
};
// The pilot is only conclusive where the two answers are at least 3 dB apart
// on either side and the cable still passes signal: VB-CABLE's own control
// mutes outright near the bottom of its range.
inline bool pilotRange(float volumeDb) noexcept {return std::isfinite(volumeDb)&&volumeDb<=-6&&volumeDb>=-40;}
inline CableVolume classifyPilot(float measured,float baseline,float sent,float volumeDb) noexcept {
    if(!pilotRange(volumeDb)||!std::isfinite(measured)||!std::isfinite(baseline)||baseline<0||!std::isfinite(sent)||sent<=0) return CableVolume::Unknown;
    // Programme material at the pilot frequency must not decide the answer.
    if(measured<=0||measured<4*baseline) return CableVolume::Unknown;
    const float measuredDb=20*std::log10(measured/sent);
    // Require agreement with an actual hypothesis, not merely its half of
    // the range. Partial attenuation or unrelated gain is inconclusive.
    // VB-CABLE's own control moves in steps and lands up to about 4 dB below the
    // requested level (measured: -42.1 dB at -38.1 requested, 8 %), so the
    // attenuating answer allows 5 dB of extra loss and 3 dB of shortfall.
    if(measuredDb>volumeDb-5&&measuredDb<volumeDb+3) return CableVolume::Attenuates;
    if(std::abs(measuredDb)<3) return CableVolume::Ignores;
    return CableVolume::Unknown;
}
}
