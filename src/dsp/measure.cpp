#include "dps/measure.hpp"
#include <algorithm>
#include <cmath>
#include <numbers>
#include <stdexcept>
namespace dps {
namespace {
bool validRate(double rate) {return std::isfinite(rate)&&rate>16000&&rate<=192000;}
bool finiteSamples(const float* samples,std::size_t count) {
    return samples&&count&&std::all_of(samples,samples+count,[](float sample){return std::isfinite(sample);});
}
// Energy at one frequency over a span of samples (Goertzel).
double toneEnergy(const float* x,std::size_t frames,double rate,double frequency) {
    if(frames<4) return 0;
    const double k=2*std::numbers::pi*frequency/rate,c=2*std::cos(k);
    double s1=0,s2=0;
    for(std::size_t i=0;i<frames;++i) {
        const double s0=static_cast<double>(x[i])+c*s1-s2;s2=s1;s1=s0;
    }
    return std::max(s1*s1+s2*s2-c*s1*s2,0.0)/static_cast<double>(frames);
}
double rmsDb(const float* x,std::size_t frames) {
    if(!frames) return -120;
    double sum=0;for(std::size_t i=0;i<frames;++i) sum+=static_cast<double>(x[i])*x[i];
    return 10*std::log10(std::max(sum/static_cast<double>(frames),1e-12));
}
}
std::vector<float> sweepSignal(double rate,float amplitude) {
    if(!validRate(rate)||!std::isfinite(amplitude)||amplitude<=0||amplitude>1)
        throw std::invalid_argument("Invalid measurement signal configuration");
    const auto tone=static_cast<std::size_t>(rate*sweepToneSeconds);
    const auto gap=static_cast<std::size_t>(rate*sweepGapSeconds);
    std::vector<float> out;
    out.reserve((tone+gap)*measureBands.size()*2);
    for(const float frequency:measureBands) {
        for(std::size_t i=0;i<tone;++i) {
            // Fade each tone in and out so the speaker is not asked for a step,
            // which would measure the click rather than the tone.
            const double t=static_cast<double>(i)/static_cast<double>(tone);
            const double fade=std::min({1.0,t/0.1,(1.0-t)/0.1});
            const auto v=static_cast<float>(amplitude*fade*
                std::sin(2*std::numbers::pi*frequency*static_cast<double>(i)/rate));
            out.push_back(v);out.push_back(v);
        }
        for(std::size_t i=0;i<gap;++i) {out.push_back(0);out.push_back(0);}
    }
    return out;
}
Measurement analyseSweep(const float* recorded,std::size_t frames,double rate,
                         const float* silence,std::size_t silenceFrames,std::span<const AudioGap> gaps) {
    Measurement m;
    if(!validRate(rate)||!finiteSamples(recorded,frames)||!finiteSamples(silence,silenceFrames)) {
        m.warning="Datos de medicion invalidos o incompletos.";return m;
    }
    m.noiseFloorDb=static_cast<float>(rmsDb(silence,silenceFrames));
    m.signalDb=-120; // filled in below, over the tones themselves
    const auto tone=static_cast<std::size_t>(rate*sweepToneSeconds);
    const auto gap=static_cast<std::size_t>(rate*sweepGapSeconds);
    const std::size_t step=tone+gap;
    if(frames<step*measureBands.size()) {
        m.warning="La grabacion es mas corta de lo esperado.";return m;
    }
    // Fit one common delay from the complete midrange tones. Their spectral
    // energy is maximal when the window contains the full faded tone. Unlike
    // onset thresholds, this does not jump to another band when bass is absent
    // or depend on an arbitrary noise threshold. Analysis is outside audio RT.
    const std::size_t required=(measureBands.size()-1)*step+tone;
    const std::size_t maxOffset=std::min(static_cast<std::size_t>(rate*.3),frames-required);
    const std::size_t increment=std::max<std::size_t>(1,static_cast<std::size_t>(rate*.005));
    std::vector<std::array<double,4>> candidates;
    std::array<double,4> strongest{};
    for(std::size_t delay=0;delay<=maxOffset;delay+=increment) {
        std::array<double,4> powers{};
        for(std::size_t band=0;band<4;++band) {
            powers[band]=toneEnergy(recorded+delay+(band+2)*step,tone,rate,measureBands[band+2]);
            strongest[band]=std::max(strongest[band],powers[band]);
        }
        candidates.push_back(powers);
    }
    std::size_t offset=0;double bestScore=-1;
    const double noisePower=std::pow(10.0,m.noiseFloorDb/10.0);
    for(std::size_t candidate=0;candidate<candidates.size();++candidate) {
        double score=0;
        for(std::size_t band=0;band<4;++band)
            if(2*strongest[band]/static_cast<double>(tone)>noisePower*16)
                score+=candidates[candidate][band]/strongest[band];
        if(score>bestScore) {bestScore=score;offset=candidate*increment;}
    }
    m.alignmentMs=static_cast<float>(offset*1000.0/rate);
    // Skip the fade at each end of the tone and measure the steady part.
    const std::size_t skip=tone/5,window=tone-2*skip;
    std::array<double,measureBands.size()> energy{};
    for(std::size_t b=0;b<measureBands.size();++b) {
        const std::size_t start=offset+b*step+skip;
        if(start+window>frames) {m.warning="La grabacion se corto antes de terminar.";return m;}
        for(const auto& loss:gaps) {
            if(loss.begin>frames||loss.frames>frames-loss.begin) {
                m.warning="Datos de perdida de muestras invalidos.";return m;
            }
            if(loss.begin<start+window&&loss.begin+loss.frames>start) {
                m.warning="Faltan muestras dentro del tono de "+std::to_string(static_cast<int>(measureBands[b]))+
                    " Hz. Repite la medicion.";return m;
            }
        }
        energy[b]=toneEnergy(recorded+start,window,rate,measureBands[b]);
    }
    for(std::size_t b=0;b<measureBands.size();++b)
        m.levelDb[b]=static_cast<float>(10*std::log10(std::max(energy[b],1e-12)));
    // Reference against the midrange (250 Hz to 1 kHz), not the average of all
    // bands: on a laptop the missing bass drags the average down so far that
    // every other band looks like a boost, which is the opposite of the truth.
    const double reference=(m.levelDb[2]+m.levelDb[3]+m.levelDb[4])/3.0;
    for(auto& level:m.levelDb) level-=static_cast<float>(reference);

    // A measurement is only worth anything if the tones stood clearly above the
    // room. Otherwise it describes the noise, not the speakers.
    // Judge the noise margin against the tones, not the whole recording: the
    // gaps between them and the silent tail would drag the figure down and
    // reject a perfectly good measurement.
    double midrange=-120;
    for(std::size_t band=2;band<6;++band) {
        const std::size_t start=offset+band*step+skip;
        const double coherentPower=2*energy[band]/static_cast<double>(window);
        const double totalDb=rmsDb(recorded+start,window);
        const double toneDb=10*std::log10(std::max(coherentPower,1e-12));
        midrange=std::max(midrange,toneDb);
        // A loud click, speech or unrelated music must not qualify as a tone.
        if(toneDb-m.noiseFloorDb>=12&&toneDb>=totalDb-4)
            ++m.reliableBands;
    }
    m.signalDb=static_cast<float>(midrange);
    // Nothing was recorded at all: a muted or disconnected microphone, which is
    // a different problem from a noisy room and needs a different message.
    if(m.signalDb<=-110&&m.noiseFloorDb<=-110) {
        m.warning="El microfono no capto nada. Comprueba que no esta silenciado.";
        return m;
    }
    const float headroom=m.signalDb-m.noiseFloorDb;
    if(headroom<12) {
        m.warning="Los tonos de prueba no se distinguen con suficiente margen en la captura. "
                  "Esto no demuestra que la sala tenga demasiado ruido: comprueba que los tonos "
                  "se oyen por la salida elegida y que el microfono no aplica supresion de ruido o eco.";
        return m;
    }
    if(m.reliableBands<2) {
        m.warning="No se identificaron suficientes tonos estables. Comprueba la salida, el microfono "
                  "y sus mejoras de audio; evita musica o voces durante la medicion.";
        return m;
    }
    if(headroom<20) m.warning="Margen de ruido justo: la medida es orientativa.";
    if(!gaps.empty()) {
        if(!m.warning.empty()) m.warning+=' ';
        m.warning+="Hubo muestras perdidas fuera de los tramos analizados.";
    }
    m.usable=true;
    return m;
}
unsigned selectMeasurementChannel(const Measurement& left,const Measurement& right) noexcept {
    if(left.usable!=right.usable) return right.usable?1u:0u;
    if(left.reliableBands!=right.reliableBands) return right.reliableBands>left.reliableBands?1u:0u;
    return right.signalDb-right.noiseFloorDb>left.signalDb-left.noiseFloorDb?1u:0u;
}
DeviceCalibration calibrationFrom(const Measurement& measurement,bool portable,const DeviceCalibration& voicing) {
    if(!measurement.usable||!std::all_of(measurement.levelDb.begin(),measurement.levelDb.end(),
        [](float value){return std::isfinite(value);})) return voicing;
    auto result=calibrationFrom(measurement,portable);
    // Acoustic tones estimate broad EQ, not compression, attack or stereo
    // preferences. A new room correction must not erase the chosen voicing.
    result.harmonicGain=voicing.harmonicGain;result.harmonicFocus=voicing.harmonicFocus;
    result.stereoAmount=voicing.stereoAmount;result.detailDb=voicing.detailDb;
    result.bodyFloor=voicing.bodyFloor;result.adaptiveDepth=voicing.adaptiveDepth;
    result.loudDensity=voicing.loudDensity;result.bandDensity=voicing.bandDensity;
    result.transientAccentDb=voicing.transientAccentDb;
    return result;
}
DeviceCalibration calibrationFrom(const Measurement& measurement,bool portable) {
    DeviceCalibration c;
    if(!measurement.usable||!std::all_of(measurement.levelDb.begin(),measurement.levelDb.end(),
        [](float level){return std::isfinite(level);})) return c;
    const auto& level=measurement.levelDb;

    // Where the speaker stops reproducing: the lowest band that is not far
    // below the rest. Cutting below that frees excursion without losing
    // anything that was audible.
    float cut=portable?120.f:40.f;
    for(std::size_t band=0;band<3;++band) {
        // Spread each transition over 6 dB. Crossing -10 dB by a fraction no
        // longer causes a discrete 38 Hz jump in the suggested high-pass.
        const float position=std::clamp((-level[band]-7.f)/6.f,0.f,1.f);
        const float blend=position*position*(3-2*position);
        const float target=std::clamp(measureBands[band]*1.35f,20.f,200.f);
        cut+=blend*std::max(0.f,target-cut);
    }
    c.highpassHz=std::clamp(cut,20.f,200.f);

    // Body sits just above the cut, within what the engine accepts.
    c.bodyHz=std::clamp(c.highpassHz*1.7f,100.f,500.f);
    // Body is the 250 Hz band measured against the midrange that the speaker
    // does reproduce (500 Hz and 1 kHz). The 120 Hz band is left out: below the
    // cut it is not going to be lifted anyway, and including it would ask for a
    // correction the speaker cannot deliver. Half the dip is applied, since the
    // harmonics below are already filling part of that range.
    const float bodyDip=((level[3]+level[4])*0.5f-level[2])*0.5f;
    c.bodyDb=std::clamp(bodyDip,0.f,12.f);

    // Presence: how far the 2-4 kHz region sits below the midrange. Only two
    // thirds of the measured dip is applied: a laptop microphone sits on the
    // lid and reads this region with its own colouring, so correcting it fully
    // overshoots. Exclude 8 kHz because screen reflections can bias it upward.
    // The user can always push Clarity further by ear.
    c.presenceHz=3000;
    const float presenceDip=-(level[5]+level[6])*0.5f;
    c.presenceDb=std::clamp(presenceDip*0.66f,0.f,10.f);

    // A hump in the lower mids is what makes small speakers sound boxy.
    const float mudBump=level[4];
    c.mudHz=650;
    c.mudDb=std::clamp(-mudBump,-9.f,0.f);

    // Harmonics replace the bass that was cut, more of it the higher the cut.
    c.harmonicGain=std::clamp(0.8f+(c.highpassHz-60.f)/120.f,0.f,2.f);
    c.harmonicFocus=portable?0.8f:0.3f;
    c.stereoAmount=portable?0.5f:0.3f;
    c.detailDb=portable?5.f:3.f;
    c.bodyFloor=portable?0.85f:0.5f;
    return c;
}
}
