#include "../src/audio/endpoint_silence.hpp"
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
        std::cout<<"PASS: mute, zero volume, exact silence, unchanged normal samples, unmute, stale snapshot and allocation-free callback\n";
    } catch(const std::exception& error) {watch=false;std::cerr<<error.what()<<'\n';return 1;}
}
