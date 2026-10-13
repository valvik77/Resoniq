#include "dps/engine.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
namespace dps {
namespace {
float bounded(float x) { return std::isfinite(x)?std::clamp(x,0.f,1.f):0.f; }
float coefficient(double rate,double ms) { return static_cast<float>(std::exp(-1.0/(rate*ms*0.001))); }
float clean(float x) { return std::isfinite(x)?std::clamp(x,-16.f,16.f):0.f; }
}
Parameters preset(std::string_view name) {
    if(name=="Natural"||name=="RESONIQ Natural") return {0.15f,0.15f,0.15f,0.05f};
    if(name=="Enhanced"||name=="RESONIQ Enhance") return {};
    if(name=="Powerful"||name=="RESONIQ Power") return {0.6f,0.4f,0.6f,0.25f};
    if(name=="Laptop Max"||name=="RESONIQ Max") return {0.8f,0.5f,0.8f,0.2f};
    throw std::invalid_argument("Unknown preset");
}
void Biquad::configure(Type type,double rate,double frequency,double q,double gain) {
    if(!std::isfinite(rate)||!std::isfinite(frequency)||!std::isfinite(q)||!std::isfinite(gain)||rate<8000||frequency<=0||frequency>=rate/2||q<=0||q>100||std::abs(gain)>24) throw std::invalid_argument("Invalid biquad parameters");
    const double w=2*std::numbers::pi*frequency/rate,c=std::cos(w),alpha=std::sin(w)/(2*q),A=std::pow(10.,gain/40);
    double a0;
    if(type==Type::Peak) { b0_=1+alpha*A;b1_=-2*c;b2_=1-alpha*A;a0=1+alpha/A;a1_=-2*c;a2_=1-alpha/A; }
    else if(type==Type::Allpass) {b0_=1-alpha;b1_=-2*c;b2_=1+alpha;a0=1+alpha;a1_=-2*c;a2_=1-alpha;}
    else { const double sign=type==Type::Lowpass?1:-1; b0_=(1-sign*c)/2;b1_=sign*(1-sign*c);b2_=b0_;a0=1+alpha;a1_=-2*c;a2_=1-alpha; }
    b0_/=a0;b1_/=a0;b2_/=a0;a1_/=a0;a2_/=a0;reset();
}
float Biquad::tick(float x) noexcept {
    const double y=b0_*x+z1_;z1_=b1_*x-a1_*y+z2_;z2_=b2_*x-a2_*y;
    if(std::abs(z1_)<1e-25) z1_=0;
    if(std::abs(z2_)<1e-25) z2_=0;
    return static_cast<float>(y);
}
void Biquad::configurePhase(double rate,double frequency,double fraction) {
    if(!std::isfinite(fraction)||fraction<0||fraction>1) throw std::invalid_argument("Invalid phase fraction");
    configure(Type::Allpass,rate,frequency,std::sqrt(.5));
    if(fraction==0) {b0_=1;b1_=b2_=a1_=a2_=0;return;}
    const double real=-a1_/2,imaginary=std::sqrt(std::max(a2_-real*real,0.0));
    const double movedReal=1+fraction*(real-1),movedImaginary=fraction*imaginary;
    a1_=-2*movedReal;a2_=movedReal*movedReal+movedImaginary*movedImaginary;
    b0_=a2_;b1_=a1_;b2_=1;reset();
}
void Engine::Split::configure(double rate,double frequency) {
    for(auto& f:low) f.configure(Biquad::Type::Lowpass,rate,frequency,std::sqrt(0.5));
    for(auto& f:high) f.configure(Biquad::Type::Highpass,rate,frequency,std::sqrt(0.5));
}
void Engine::Split::tick(float x,float& l,float& h) noexcept { l=low[1].tick(low[0].tick(x));h=high[1].tick(high[0].tick(x)); }
void Engine::prepare(double rate,Parameters p) {
    if(!std::isfinite(rate)||rate<16000||rate>192000||!std::isfinite(p.agcAttackMs)||!std::isfinite(p.agcReleaseMs)||p.agcAttackMs<1||p.agcAttackMs>10000||p.agcReleaseMs<1||p.agcReleaseMs>30000) throw std::invalid_argument("Invalid engine configuration");
    p.bass=bounded(p.bass);p.clarity=bounded(p.clarity);p.dynamics=bounded(p.dynamics);p.spatial=bounded(p.spatial);p.intensity=bounded(p.intensity);
    const auto& d=p.device;
    auto valid=[](float v,float lo,float hi){return std::isfinite(v)&&v>=lo&&v<=hi;};
    if(!valid(d.highpassHz,20,200)||!valid(d.bodyHz,100,500)||!valid(d.bodyDb,0,12)||!valid(d.presenceHz,1000,6000)||!valid(d.presenceDb,0,12)||!valid(d.harmonicGain,0,2)||!valid(d.stereoAmount,0,1)||!valid(d.mudHz,300,1200)||!valid(d.mudDb,-9,0)||!valid(d.detailDb,0,6)||!valid(d.harmonicFocus,0,1)||!valid(d.bodyFloor,0,1)||!valid(d.adaptiveDepth,0,0.9f)||!valid(d.loudDensity,1,2.5f)||!valid(d.bandDensity,1,4)) throw std::invalid_argument("Invalid device calibration");
    if(!valid(d.transientAccentDb,0,6)) throw std::invalid_argument("Invalid transient accent");
    parameters_=p;rate_=rate;
    if(p.alignedIntensity) for(std::size_t bank=0;bank<intensityPhase_.size();++bank)
        for(auto& channel:intensityPhase_[bank]) for(std::size_t stage=0;stage<3;++stage)
            channel[stage].configurePhase(rate,std::array<double,3>{180,900,3500}[stage],double(bank)/32);
    for(std::size_t ch=0;ch<2;++ch) {
        for(std::size_t b=0;b<3;++b) splits_[ch][b].configure(rate,std::array<double,3>{180,900,3500}[b]);
        compensation_[ch][0].configure(rate,900);compensation_[ch][1].configure(rate,3500);compensation_[ch][2].configure(rate,3500);
        bassFilter_[ch].configure(Biquad::Type::Lowpass,rate,120,0.707);
        harmonicHighpass_[ch].configure(Biquad::Type::Highpass,rate,130,0.707);
        harmonicLowpass_[ch].configure(Biquad::Type::Lowpass,rate,650,0.707);
        body_[ch].configure(Biquad::Type::Peak,rate,d.bodyHz,0.7,d.bodyDb);
        protection_[ch].configure(Biquad::Type::Highpass,rate,d.highpassHz,0.707);
        presence_[ch].configure(Biquad::Type::Peak,rate,d.presenceHz,0.8,d.presenceDb);
        mud_[ch].configure(Biquad::Type::Peak,rate,d.mudHz,0.9,d.mudDb);
    }
    attack_=coefficient(rate,8);release_=coefficient(rate,160);
    agcAttack_=coefficient(rate,p.agcAttackMs);agcRelease_=coefficient(rate,p.agcReleaseMs);
    mixCoefficient_=static_cast<float>(1-std::exp(-1/(rate*0.02)));
    controlSmoothing_=1-coefficient(rate,20);comparisonCoefficient_=coefficient(rate,400);
    levelAttack_=coefficient(rate,30);levelRelease_=coefficient(rate,600);
    bassAttack_=coefficient(rate,5);bassRelease_=coefficient(rate,120);
    for(auto& transient:transients_) transient.prepare(rate);
    for(auto& accent:accents_) accent.prepare(rate);
    prepared_=true;reset();
}
void Engine::reset() noexcept {
    for(auto& bank:intensityPhase_) for(auto& channel:bank) for(auto& filter:channel) filter.reset();
    for(auto* bank:{&splits_,&compensation_}) for(auto& channel:*bank) for(auto& s:channel) { for(auto& f:s.low) f.reset();for(auto& f:s.high) f.reset(); }
    for(auto* bank:{&bassFilter_,&harmonicHighpass_,&harmonicLowpass_,&body_,&presence_,&protection_,&mud_}) for(auto& f:*bank) f.reset();
    comparisonDryTarget_=comparisonWetTarget_=1;comparisonDryEnergy_=comparisonWetEnergy_=0;comparisonDryGain_=comparisonWetGain_=comparisonDryLimiter_=comparisonWetLimiter_=1;
    comparisonIntensity_=parameters_.intensity;comparisonBlend_=parameters_.enabled?1.f:0.f;comparisonMix_=comparison_?1.f:0.f;comparisonSamples_=0;
    restartComparisonMeasurement();
    controls_={parameters_.bass,parameters_.clarity,parameters_.dynamics,parameters_.spatial};
    bassEnvelope_=0;levelEnvelope_=0;enhancement_=1;
    envelope_.fill(0);bandGain_.fill(1);agcEnvelope_=0;agcGain_=limiterGain_=1;mix_=parameters_.enabled?parameters_.intensity:0;meters_={};
    for(auto& transient:transients_) transient.reset();
    for(auto& accent:accents_) accent.reset();
}
void Engine::restartComparisonMeasurement() noexcept {
    comparisonDrySum_=comparisonWetSum_=0;
    comparisonWarmup_=comparisonSamples_=0;
}
void Engine::setComparison(bool enabled) noexcept {
    if(enabled&&!comparison_) restartComparisonMeasurement();
    comparison_=enabled;
}
void Engine::setIntensity(float v) noexcept {
    v=bounded(v);if(v!=parameters_.intensity) restartComparisonMeasurement();parameters_.intensity=v;
}
void Engine::setControls(float bass,float clarity,float dynamics,float spatial) noexcept {
    bass=bounded(bass);clarity=bounded(clarity);dynamics=bounded(dynamics);spatial=bounded(spatial);
    if(bass!=parameters_.bass||clarity!=parameters_.clarity||dynamics!=parameters_.dynamics||spatial!=parameters_.spatial) restartComparisonMeasurement();
    parameters_.bass=bass;parameters_.clarity=clarity;
    parameters_.dynamics=dynamics;parameters_.spatial=spatial;
}
void Engine::process(const float* input,float* output,std::size_t frames) noexcept {
    meters_={};if(!prepared_||!input||!output) return;
    float minimumGain=1;
    float maximumTransientGain=1;
    for(std::size_t i=0;i<frames;++i) {
        const std::array<float,4> targets{parameters_.bass,parameters_.clarity,parameters_.dynamics,parameters_.spatial};
        for(std::size_t k=0;k<4;++k) controls_[k]+=controlSmoothing_*(targets[k]-controls_[k]);
        const std::array<float,2> dry{clean(input[2*i]),clean(input[2*i+1])};
        std::array<std::array<float,2>,33> phaseReferences{};
        if(parameters_.alignedIntensity) for(std::size_t bank=0;bank<intensityPhase_.size();++bank)
            for(std::size_t ch=0;ch<2;++ch) {
                float sample=dry[ch];for(auto& filter:intensityPhase_[bank][ch]) sample=filter.tick(sample);
                phaseReferences[bank][ch]=sample;
            }
        const auto intensityMix=[&](const std::array<float,2>& processed,float amount) {
            auto reference=dry;
            if(parameters_.alignedIntensity&&amount>0&&amount<1) {
                // Move the dry phase continuously toward the crossover phase,
                // using prepared, stable allpasses rather than changing RT coefficients.
                const float position=32*std::pow(amount,.125f);
                const auto lower=std::min<std::size_t>(static_cast<std::size_t>(position),31);
                const float fraction=position-static_cast<float>(lower);
                for(std::size_t ch=0;ch<2;++ch) reference[ch]=phaseReferences[lower][ch]+
                    fraction*(phaseReferences[lower+1][ch]-phaseReferences[lower][ch]);
            }
            std::array<float,2> result{};
            for(std::size_t ch=0;ch<2;++ch) result[ch]=reference[ch]+amount*(processed[ch]-reference[ch]);
            return result;
        };
        // Stereo-linked level adaptation. Never change filter coefficients on audio thread.
        const float inputLevel=std::max(std::abs(dry[0]),std::abs(dry[1]));
        const float lc=inputLevel>levelEnvelope_?levelAttack_:levelRelease_;
        levelEnvelope_=lc*levelEnvelope_+(1-lc)*inputLevel;
        const float position=std::clamp((levelEnvelope_-0.12f)/0.38f,0.f,1.f);
        const float smooth=position*position*(3-2*position);
        enhancement_=parameters_.adaptive?1-parameters_.device.adaptiveDepth*smooth:1.f;
        meters_.enhancement=enhancement_;
        std::array<std::array<float,4>,2> bands{};
        for(std::size_t ch=0;ch<2;++ch) {
            meters_.inputPeak=std::max(meters_.inputPeak,std::abs(dry[ch]));
            float rest=dry[ch];
            for(std::size_t b=0;b<3;++b) splits_[ch][b].tick(rest,bands[ch][b],rest);
            bands[ch][3]=rest;
            float l,h;compensation_[ch][0].tick(bands[ch][0],l,h);bands[ch][0]=l+h;
            compensation_[ch][1].tick(bands[ch][0],l,h);bands[ch][0]=l+h;
            compensation_[ch][2].tick(bands[ch][1],l,h);bands[ch][1]=l+h;
        }
        const float density=parameters_.device.bandDensity;
        const float ratio=1+3*controls_[2]*enhancement_*density;
        const float knee=0.16f/density;
        const float makeup=std::pow(0.16f/knee,1-1/ratio);
        for(std::size_t b=0;b<4;++b) {
            const float peak=std::max(std::abs(bands[0][b]),std::abs(bands[1][b]));
            float c=peak>envelope_[b]?attack_:release_;envelope_[b]=c*envelope_[b]+(1-c)*peak;
            // bandDensity lowers the knee and raises the ratio together, so
            // ordinary programme material is compressed instead of only peaks.
            // Compression alone just pulls peaks down, so the band is brought
            // back up by the gain the knee itself loses: that make-up is what
            // fills the quiet part of each band and reads as density.
            float target=envelope_[b]>knee?makeup*std::pow(knee/envelope_[b],1-1/ratio):makeup;
            // Gated upward dynamics for audible mid-band detail. Signal below
            // the gate receives no lift; strong material gets no added makeup.
            if(parameters_.device.detailDb>0&&(b==1||b==2)) {
                const float gate=std::clamp((envelope_[b]-.002f)/.018f,0.f,1.f);
                const float lift=parameters_.device.detailDb*controls_[2]*gate*std::clamp(1-envelope_[b]/.12f,0.f,1.f);
                target*=std::pow(10.f,lift/20);
                meters_.dynamicLiftDb=std::max(meters_.dynamicLiftDb,lift);
            }
            c=target<bandGain_[b]?attack_:release_;bandGain_[b]=c*bandGain_[b]+(1-c)*target;
            // Report compression relative to the band's own make-up, otherwise
            // the upward make-up hides the downward work the compressor does.
            minimumGain=std::min(minimumGain,bandGain_[b]/std::max(makeup,1e-3f));
            // Preserve the attack of mid/high events rather than relying on
            // global makeup gain. Legacy profiles have detailDb=0 and retain
            // their exact path. Detection is linked to preserve stereo position.
            if((parameters_.device.detailDb>0||parameters_.device.transientAccentDb>0)&&b>0) {
                const float amount=parameters_.device.detailDb*controls_[1]*(b==3?.5f:1.f);
                float lift=transients_[b].tick(peak,amount);
                if(parameters_.device.transientAccentDb>0)
                    lift*=accents_[b].tick(peak,parameters_.device.transientAccentDb*controls_[1]*(b==3?.5f:1.f));
                maximumTransientGain=std::max(maximumTransientGain,lift);
                for(std::size_t ch=0;ch<2;++ch) bands[ch][b]*=lift;
            }
        }
        std::array<float,2> wet{};
        const std::array<float,2> low{bassFilter_[0].tick(dry[0]),bassFilter_[1].tick(dry[1])};
        const float bassPeak=std::max(std::abs(low[0]),std::abs(low[1]));
        const float bc=bassPeak>bassEnvelope_?bassAttack_:bassRelease_;
        bassEnvelope_=bc*bassEnvelope_+(1-bc)*bassPeak;
        for(std::size_t ch=0;ch<2;++ch) {
            for(std::size_t b=0;b<4;++b) wet[ch]+=bands[ch][b]*bandGain_[b];
            // Envelope-normalized second/third harmonics remain proportional to
            // signal level, instead of vanishing cubically at moderate volume.
            const float v=std::clamp(low[ch]/std::max(bassEnvelope_,0.01f),-1.f,1.f);
            const float classic=0.65f*(2*v*v-1)+0.35f*(4*v*v*v-3*v);
            float generated=classic*bassEnvelope_;
            if(parameters_.device.harmonicFocus>0) {
                const float focused=.55f*(4*v*v*v-3*v)+.45f*(8*v*v*v*v-8*v*v+1);
                generated=(classic+parameters_.device.harmonicFocus*(focused-classic))*bassEnvelope_;
            }
            const float harmonic=harmonicLowpass_[ch].tick(harmonicHighpass_[ch].tick(generated));
            const float source=wet[ch]+parameters_.device.harmonicGain*controls_[0]*(0.8f+0.2f*enhancement_)*harmonic;
            const float body=body_[ch].tick(source);
            float shaped=source+controls_[0]*std::max(enhancement_,parameters_.device.bodyFloor)*(body-source);
            const float clear=mud_[ch].tick(shaped);
            if(parameters_.device.mudDb!=0) shaped+=controls_[1]*(clear-shaped);
            const float presence=presence_[ch].tick(shaped);
            // Keep presence and audible bass harmonics at high levels. Only the
            // fundamental/body boost backs off with the signal envelope.
            wet[ch]=protection_[ch].tick(shaped+controls_[1]*(presence-shaped));
        }
        const float peak=std::max(std::abs(wet[0]),std::abs(wet[1]));
        float c=peak>agcEnvelope_?agcAttack_:agcRelease_;agcEnvelope_=c*agcEnvelope_+(1-c)*peak;
        // Follow the original level for loud input instead of normalizing it down
        // to the quiet-passage target. Final limiter still owns sample safety.
        const float levelTarget=std::max(0.22f+0.1f*controls_[2],levelEnvelope_*parameters_.device.loudDensity);
        // Discount the band make-up from what the AGC sees. Without this the
        // AGC measures the already-lifted signal and pulls its own gain down by
        // the same amount, cancelling the density the compressor just created.
        const float seen=agcEnvelope_/std::max(makeup,1e-3f);
        const float target=seen>0.015f?std::clamp(levelTarget/std::max(seen,0.015f),0.5f,2.f):1.f;
        c=target<agcGain_?agcAttack_:agcRelease_;agcGain_=c*agcGain_+(1-c)*target;
        const float mid=(wet[0]+wet[1])*0.5f,side=(wet[0]-wet[1])*0.5f*(1+parameters_.device.stereoAmount*controls_[3]);
        wet={(mid+side)*agcGain_,(mid-side)*agcGain_};
        // Measure both audition paths continuously, including their sample
        // limiting, irrespective of which path is currently selected.
        comparisonIntensity_+=controlSmoothing_*(parameters_.intensity-comparisonIntensity_);
        std::array<float,2> processed{},reference=dry;
        processed=intensityMix(wet,comparisonIntensity_);
        auto limit=[&](std::array<float,2>& samples,float& gain) {
            const float p=std::max(std::abs(samples[0]),std::abs(samples[1]));
            const float required=p>ceiling?ceiling/p:1.f;
            gain=required<gain?required:release_*gain+(1-release_)*required;
            for(auto& x:samples) x=std::clamp(x*gain,-ceiling,ceiling);
        };
        limit(processed,comparisonWetLimiter_);limit(reference,comparisonDryLimiter_);
        const float dryEnergy=.5f*(reference[0]*reference[0]+reference[1]*reference[1]);
        const float wetEnergy=.5f*(processed[0]*processed[0]+processed[1]*processed[1]);
        comparisonDryEnergy_=comparisonCoefficient_*comparisonDryEnergy_+(1-comparisonCoefficient_)*dryEnergy;
        comparisonWetEnergy_=comparisonCoefficient_*comparisonWetEnergy_+(1-comparisonCoefficient_)*wetEnergy;
        if(dryEnergy>1e-8f&&wetEnergy>1e-8f) {
            // Warm up the DSP, then measure a programme window and hold its
            // offset. A continuously following matcher cancels some of the
            // dynamic changes we are trying to audition. A/B switching does
            // not recalibrate; controls or a new comparison do.
            if(comparisonWarmup_<static_cast<std::size_t>(rate_*5)) ++comparisonWarmup_;
            else if(comparisonSamples_<static_cast<std::size_t>(rate_*3)) {
                comparisonDrySum_+=dryEnergy;comparisonWetSum_+=wetEnergy;
                ++comparisonSamples_;
                if(comparisonSamples_==static_cast<std::size_t>(rate_*3)) {
                    const float levelRatio=static_cast<float>(std::sqrt(comparisonWetSum_/std::max(comparisonDrySum_,1e-12)));
                    comparisonDryTarget_=std::clamp(levelRatio,.12589254f,1.f);
                    comparisonWetTarget_=std::clamp(1/levelRatio,.12589254f,1.f);
                }
            }
        }
        // Only attenuation, smoothly applied; hold calibration through silence.
        comparisonDryGain_+=(1-comparisonCoefficient_)*(comparisonDryTarget_-comparisonDryGain_);
        comparisonWetGain_+=(1-comparisonCoefficient_)*(comparisonWetTarget_-comparisonWetGain_);
        comparisonBlend_+=controlSmoothing_*((parameters_.enabled?1.f:0.f)-comparisonBlend_);
        comparisonMix_+=controlSmoothing_*((comparison_?1.f:0.f)-comparisonMix_);
        mix_+=mixCoefficient_*((parameters_.enabled?parameters_.intensity:0)-mix_);
        wet=intensityMix(wet,mix_);
        if(comparisonMix_>0) for(std::size_t ch=0;ch<2;++ch) {
            const float a=reference[ch]*comparisonDryGain_,b=processed[ch]*comparisonWetGain_;
            const float audition=a+comparisonBlend_*(b-a);
            wet[ch]+=comparisonMix_*(audition-wet[ch]);
        }
        const float p=std::max(std::abs(wet[0]),std::abs(wet[1]));
        const float required=p>ceiling?ceiling/p:1.f;
        limiterGain_=required<limiterGain_?required:release_*limiterGain_+(1-release_)*required;
        minimumGain=std::min(minimumGain,limiterGain_);
        for(std::size_t ch=0;ch<2;++ch) { const float y=std::clamp(wet[ch]*limiterGain_,-ceiling,ceiling);output[2*i+ch]=y;meters_.outputPeak=std::max(meters_.outputPeak,std::abs(y)); }
    }
    meters_.gainReductionDb=-20*std::log10(std::max(minimumGain,1e-9f));
    meters_.transientLiftDb=20*std::log10(maximumTransientGain);
    meters_.comparisonDryDb=20*std::log10(comparisonDryGain_);meters_.comparisonWetDb=20*std::log10(comparisonWetGain_);
    meters_.comparisonReady=comparisonSamples_>=static_cast<std::size_t>(rate_*3)&&std::abs(comparisonDryGain_/comparisonDryTarget_-1)<.02f&&std::abs(comparisonWetGain_/comparisonWetTarget_-1)<.02f;
}
}
