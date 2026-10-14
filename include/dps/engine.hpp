#pragma once
#include <array>
#include <cstddef>
#include <string_view>
#include "dps/transient.hpp"

namespace dps {
struct DeviceCalibration {
    float highpassHz=45,bodyHz=190,bodyDb=7,presenceHz=2600,presenceDb=6;
    float harmonicGain=1.2f,stereoAmount=0.5f;
    float mudHz=650,mudDb=0,detailDb=0,harmonicFocus=0,bodyFloor=0;
    // How far the level adaptation backs off the effect on loud material.
    // 0.7 is the historical curve; 0 holds the full effect at every level,
    // which is what loud, already-compressed music needs. The final limiter
    // still owns sample safety either way.
    float adaptiveDepth=0.7f;
    // Extra density for loud material. The AGC target normally follows the
    // input once it is loud, so its gain falls to unity and the effect reads
    // as "weaker at high volume". Pushing the target above the input drives
    // the band compressor harder instead of merely raising level; the final
    // limiter still owns sample safety. 1 = historical behaviour.
    float loudDensity=1.f;
    // Band compression reach. The historical compressor only acts above 0.16
    // (about -16 dBFS per band) at up to 4:1, so the body of normal music goes
    // through untouched. Lowering the threshold and allowing a higher ratio is
    // what makes the result read as consistently full rather than merely loud.
    // 1 = historical behaviour; higher values compress earlier and harder.
    float bandDensity=1.f;
    float transientAccentDb=0.f; // optional gated attack accent; 0 = legacy
};
struct Parameters {
    float bass=0.3f, clarity=0.25f, dynamics=0.3f, spatial=0.15f, intensity=1;
    float agcAttackMs=100, agcReleaseMs=1500;
    bool enabled=true;
    bool adaptive=true;
    DeviceCalibration device{};
    bool alignedIntensity=false; // experimental; legacy profiles keep their mix
    // How far the level control may move the processed signal either way.
    // 6 dB is the historical range (0.5x to 2x) and keeps approved profiles
    // exact. Zero disables AGC gain movement; EQ, compression, harmonics and
    // the limiter can still change loudness. This is not volume matching.
    float agcRangeDb=6;
};
Parameters preset(std::string_view name);
struct Meters { float inputPeak=0, outputPeak=0, gainReductionDb=0, enhancement=1,comparisonDryDb=0,comparisonWetDb=0,dynamicLiftDb=0,transientLiftDb=0; bool comparisonReady=false; };
class Biquad {
    double b0_=1,b1_=0,b2_=0,a1_=0,a2_=0,z1_=0,z2_=0;
public:
    enum class Type { Lowpass, Highpass, Peak, Allpass };
    void configure(Type type, double rate, double frequency, double q, double gainDb=0);
    void configurePhase(double rate,double frequency,double fraction);
    float tick(float x) noexcept;
    void reset() noexcept { z1_=z2_=0; }
};
class Engine {
    struct Split { std::array<Biquad,2> low,high; void configure(double,double); void tick(float,float&,float&) noexcept; };
    std::array<std::array<Split,3>,2> splits_;
    // Allpass compensation for the serial crossover tree.
    std::array<std::array<Split,3>,2> compensation_;
    std::array<std::array<std::array<Biquad,3>,2>,33> intensityPhase_;
    std::array<Biquad,2> bassFilter_, harmonicHighpass_, harmonicLowpass_, body_, presence_, protection_,mud_;
    std::array<float,4> envelope_{}, bandGain_{1,1,1,1};
    std::array<TransientShaper,4> transients_;
    std::array<AttackAccent,4> accents_;
    Parameters parameters_{};
    double rate_=48000;
    float attack_=0,release_=0,agcAttack_=0,agcRelease_=0;
    float agcEnvelope_=0,agcGain_=1,limiterGain_=1,mix_=1,agcFloor_=.5f,agcCeiling_=2;
    float bassEnvelope_=0,bassAttack_=0,bassRelease_=0;
    std::array<float,4> controls_{.3f,.25f,.3f,.15f};
    float controlSmoothing_=0,mixCoefficient_=0;
    float levelEnvelope_=0,levelAttack_=0,levelRelease_=0,enhancement_=1;
    float comparisonDryEnergy_=0,comparisonWetEnergy_=0,comparisonDryGain_=1,comparisonWetGain_=1,comparisonDryTarget_=1,comparisonWetTarget_=1;
    float comparisonDryLimiter_=1,comparisonWetLimiter_=1,comparisonIntensity_=1,comparisonBlend_=1,comparisonMix_=0,comparisonCoefficient_=0;
    double comparisonDrySum_=0,comparisonWetSum_=0;
    std::size_t comparisonWarmup_=0;
    std::size_t comparisonSamples_=0;bool comparison_=false;
    void restartComparisonMeasurement() noexcept;
    bool prepared_=false;
    Meters meters_{};
public:
    // Configuration and reset belong to the control thread while audio is stopped.
    void prepare(double sampleRate, Parameters parameters={});
    void reset() noexcept;
    // Audio thread only; smooth bypass/intensity, no allocation or locking.
    void setEnabled(bool enabled) noexcept { parameters_.enabled=enabled; }
    void setIntensity(float value) noexcept;
    void setComparison(bool enabled) noexcept;
    void setControls(float bass,float clarity,float dynamics,float spatial) noexcept;
    void process(const float* input, float* output, std::size_t frames) noexcept;
    Meters meters() const noexcept { return meters_; }
    static constexpr float ceiling=0.89125094f; // -1 dBFS sample peak
};
}
