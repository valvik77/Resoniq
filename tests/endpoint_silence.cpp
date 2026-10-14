#include "../src/audio/endpoint_silence.hpp"
#include "../src/audio/endpoint_gain.hpp"
#include "../src/audio/cable_probe.hpp"
#include "../src/audio/startup_gate.hpp"
#include <array>
#include <cstdlib>
#include <iostream>
#include <limits>
#include <new>
#include <stdexcept>
namespace {
bool watch=false;unsigned allocations=0;
void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
}
void* operator new(std::size_t size) {if(watch) ++allocations;if(auto memory=std::malloc(size?size:1)) return memory;throw std::bad_alloc();}
void* operator new[](std::size_t size) {return ::operator new(size);}
void operator delete(void* memory) noexcept {std::free(memory);}
void operator delete[](void* memory) noexcept {std::free(memory);}
void operator delete(void* memory,std::size_t) noexcept {std::free(memory);}
void operator delete[](void* memory,std::size_t) noexcept {std::free(memory);}
int main() {
    try {
        const auto callback=Microsoft::WRL::Make<dps::detail::EndpointSilence>();
        check(callback&&callback->silent(),"Uninitialised endpoint is not fail-closed");
        callback->initialise(callback->snapshot(),.2f,false);
        std::array<float,8> music{.8f,-.7f,.6f,-.5f,.4f,-.3f,.2f,-.1f};
        const auto reference=music;
        check(!callback->apply(music.data(),4)&&music==reference,"Normal endpoint changes music");
        AUDIO_VOLUME_NOTIFICATION_DATA notification{};
        notification.fMasterVolume=.2f;notification.bMuted=TRUE;
        watch=true;
        check(callback->OnNotify(&notification)==S_OK,"Mute callback failed");
        check(callback->apply(music.data(),4),"Mute does not silence output");
        watch=false;
        for(float sample:music) check(sample==0,"Muted endpoint leaked music/filter tails");
        notification.bMuted=FALSE;notification.fMasterVolume=0;
        callback->OnNotify(&notification);music=reference;
        check(callback->apply(music.data(),4),"Zero volume without mute leaks music");
        for(float sample:music) check(sample==0,"Zero volume did not produce exact silence");
        notification.fMasterVolume=.01f;callback->OnNotify(&notification);
        music=reference;check(callback->apply(music.data(),4),"Unmute leaked queued full-scale audio");
        for(float sample:music) check(sample==0,"Reopening guard leaks samples");
        for(unsigned i=0;i<2160;++i) {music=reference;callback->apply(music.data(),4);}
        music=reference;check(!callback->apply(music.data(),4)&&music==reference,"Settled unmute changes gain");
        notification.fMasterVolume=.02f;callback->OnNotify(&notification);
        music=reference;check(!callback->apply(music.data(),4)&&music==reference,"Normal volume step interrupts audio");
        notification.fMasterVolume=.8f;callback->OnNotify(&notification);
        music=reference;check(!callback->apply(music.data(),4)&&music==reference,"Large volume step interrupts audio");
        notification.bMuted=TRUE;callback->OnNotify(&notification);
        notification.bMuted=FALSE;callback->OnNotify(&notification);
        music=reference;check(callback->apply(music.data(),4),"Quick mute/unmute bypasses protection");
        const auto stale=callback->snapshot();notification.bMuted=TRUE;callback->OnNotify(&notification);
        callback->initialise(stale,1,false);check(callback->silent(),"Stale initial volume overrides new mute");
        notification.bMuted=FALSE;notification.fMasterVolume=std::numeric_limits<float>::quiet_NaN();
        callback->OnNotify(&notification);check(callback->silent(),"Invalid endpoint volume opens output");
        check(callback->OnNotify(nullptr)==E_POINTER,"Null notification accepted");
        check(allocations==0,"Endpoint callback allocates");
        dps::detail::EndpointGain volume;
        volume.initialise(0);music=reference;
        check(volume.apply(music.data(),4)==.8f&&music==reference,"100 percent volume changes approved samples");
        volume.initialise(-58.1738f);music=reference;
        const float atTwo=volume.apply(music.data(),4);
        check(atTwo<.002f&&atTwo>0,"VB-CABLE 2 percent is full scale or muted");
        check(std::abs(atTwo/.8f-std::pow(10.f,-58.1738f/20.f))<1e-6f,"Endpoint dB curve was replaced by linear scalar");
        volume.setDb(-10.5086f);
        float settled=0;
        watch=true;
        for(unsigned i=0;i<12000;++i) {music=reference;settled=volume.apply(music.data(),4);}
        watch=false;
        check(std::abs(settled/.8f-std::pow(10.f,-10.5086f/20.f))<1e-5f,"Normal volume did not settle to Windows gain");
        check(settled>atTwo&&settled<.8f,"VB-CABLE volume is not progressive");
        volume.initialise(std::numeric_limits<float>::quiet_NaN());music=reference;
        check(volume.apply(music.data(),4)==0,"Invalid volume curve leaks audio");
        check(allocations==0,"Cable volume allocates on audio thread");
        {
            dps::detail::StartupGate gate;
            music=reference;
            check(gate.apply(music.data(),4,false)==.8f&&music==reference,"Open startup gate changes samples");
            check(gate.apply(music.data(),4,true)==0,"Startup hold leaks audio");
            for(float sample:music) check(sample==0,"Startup hold is not exact silence");
            music.fill(1);
            check(std::abs(gate.apply(music.data(),4,false)-4.f/480)<1e-7f,"Startup peak does not reflect final ramp");
            check(music[0]>0&&music[0]<music[6],"Startup ramp is discontinuous");
            watch=true;
            for(unsigned i=0;i<120;++i) {music.fill(1);gate.apply(music.data(),4,false);}
            watch=false;
            check(music[0]==1&&music[7]==1,"Startup ramp never reaches unity");
            check(allocations==0,"Startup gate allocates on audio thread");
        }
        // Whether a cable already attenuates its loopback is measured with a
        // pilot, under programme material and on both sides of the answer.
        {
            const double turn=2*3.14159265358979323846/48000;
            const auto listen=[&](float pilot,float programme) {
                dps::detail::PilotMeter meter;
                watch=true;
                for(unsigned i=0;i<dps::detail::pilotWindow*3;++i) {
                    const float tone=pilot*static_cast<float>(std::sin(turn*dps::detail::pilotHz*i));
                    const float music=programme*static_cast<float>(std::sin(turn*440*i)+.001*std::sin(turn*14950*i));
                    meter.push(tone+music,tone+music);
                }
                watch=false;
                check(meter.windows()==3,"Pilot windows miscounted");
                return meter.amplitude();
            };
            using dps::detail::CableVolume;using dps::detail::classifyPilot;
            const float sent=dps::detail::pilotAmplitude,gain=std::pow(10.f,-18.24f/20.f);
            check(std::abs(listen(sent,0)/sent-1)<.01f,"Pilot level misread");
            const float baseline=listen(0,.5f);
            check(classifyPilot(listen(sent,.5f),baseline,sent,-18.24f)==CableVolume::Ignores,"Cable ignoring its volume not recognised under programme");
            check(classifyPilot(listen(sent*gain,.5f*gain),baseline*gain,sent,-18.24f)==CableVolume::Attenuates,"Attenuating cable not recognised under programme");
            check(classifyPilot(listen(sent*std::pow(10.f,-36.1f/20.f),0),0,sent,-34.75f)==CableVolume::Attenuates,"Stepped cable volume not recognised");
            check(classifyPilot(listen(sent,0),0,sent,-3)==CableVolume::Unknown,"Cable mode decided where both answers are too close");
            check(classifyPilot(listen(0,.5f),baseline,sent,-18.24f)==CableVolume::Unknown,"Programme energy decided the cable mode");
            check(classifyPilot(0,0,sent,-45)==CableVolume::Unknown,"Muted bottom of the cable range decided the mode");
            check(classifyPilot(listen(sent*.5f,0),0,sent*.5f,-18.24f)==CableVolume::Ignores,"Session volume of the pilot not discounted");
            check(classifyPilot(listen(sent*4,0),0,sent,-18.24f)==CableVolume::Unknown,"Unexplained gain accepted as a cable mode");
            check(classifyPilot(sent*std::pow(10.f,-10.f/20),0,sent,-30)==CableVolume::Unknown,"Partial attenuation incorrectly classified");
            check(classifyPilot(sent*std::pow(10.f,-42.1f/20),0,sent,-38.08f)==CableVolume::Attenuates,"Bottom VB-CABLE step not recognised");
            check(classifyPilot(sent*std::pow(10.f,-44.f/20),0,sent,-38.08f)==CableVolume::Unknown,"Excess loss beyond a cable step accepted");
            check(classifyPilot(sent*std::pow(10.f,-3.f/20),0,sent,-6)==CableVolume::Unknown,"Ambiguous midpoint incorrectly classified");
            check(classifyPilot(sent,-1,sent,-18)==CableVolume::Unknown,"Negative baseline accepted");
            check(allocations==0,"Pilot meter allocates on audio thread");
            dps::detail::PilotMeter interrupted;
            for(unsigned i=0;i<100;++i) interrupted.push(1,1);
            interrupted.discontinuity();
            for(unsigned i=0;i<dps::detail::pilotWindow;++i) interrupted.push(0,0);
            check(interrupted.revision()==1&&interrupted.windows()==1&&interrupted.amplitude()==0,
                  "Pilot window carries samples across a capture gap");
        }
        std::cout<<"PASS: mute, zero volume, exact silence, unchanged normal samples, unmute, stale snapshot and allocation-free callback\n";
    } catch(const std::exception& error) {watch=false;std::cerr<<error.what()<<'\n';return 1;}
}
