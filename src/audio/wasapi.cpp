#include "dps/wasapi.hpp"
#include "dps/stream_buffer.hpp"
#include "endpoint_silence.hpp"
#include "capture_timeline.hpp"
#include <windows.h>
#include <audioclient.h>
#include <endpointvolume.h>
#include <mmdeviceapi.h>
#include <functiondiscoverykeys_devpkey.h>
#include <avrt.h>
#include <wrl/client.h>
#include <array>
#include <cmath>
#include <chrono>
#include <atomic>
#include <thread>
#include <future>
#include <numbers>
#include <sstream>
#include <stdexcept>
#include <vector>
#include <ks.h>
#include <ksmedia.h>
namespace dps {
using Microsoft::WRL::ComPtr;
namespace {
void require(HRESULT hr,const char* where) {if(FAILED(hr)) {std::ostringstream s;s<<where<<": HRESULT 0x"<<std::hex<<static_cast<unsigned long>(hr);throw std::runtime_error(s.str());}}
// The UI thread may already have COM initialised in another apartment, for
// instance after a common file dialog leaves it as STA. Asking for a different
// apartment then returns RPC_E_CHANGED_MODE (0x80010106), which is not a
// failure: COM is usable as it stands. Only balance CoUninitialize when this
// scope actually performed the initialisation.
struct Com {
    bool owned=false;
    Com() {
        const HRESULT hr=CoInitializeEx(nullptr,COINIT_MULTITHREADED);
        if(hr==RPC_E_CHANGED_MODE) return;
        require(hr,"COM");owned=true;
    }
    ~Com(){if(owned) CoUninitialize();}
    Com(const Com&)=delete;Com& operator=(const Com&)=delete;
};
struct Event {HANDLE handle=CreateEventW(nullptr,FALSE,FALSE,nullptr);Event(){if(!handle) throw std::runtime_error("CreateEvent failed");}~Event(){CloseHandle(handle);}Event(const Event&)=delete;Event& operator=(const Event&)=delete;};
struct Priority {HANDLE handle=nullptr;Priority(){DWORD index=0;handle=AvSetMmThreadCharacteristicsW(L"Pro Audio",&index);}~Priority(){if(handle) AvRevertMmThreadCharacteristics(handle);}};
struct Running {IAudioClient* client;bool started=false;void start(){require(client->Start(),"Start");started=true;}~Running(){if(started) client->Stop();}};
ComPtr<IMMDeviceEnumerator> enumerator() {ComPtr<IMMDeviceEnumerator> e;require(CoCreateInstance(__uuidof(MMDeviceEnumerator),nullptr,CLSCTX_ALL,IID_PPV_ARGS(&e)),"Device enumerator");return e;}
class LoopbackSilence {
    ComPtr<IAudioEndpointVolume> volume_;
    ComPtr<detail::EndpointSilence> callback_;
public:
    unsigned snapshot() const noexcept {return callback_?callback_->snapshot():0;}
    void watch(IMMDeviceEnumerator* enumerator,const std::wstring& id) {
        ComPtr<IMMDevice> device;require(enumerator->GetDevice(id.c_str(),&device),"Loopback volume endpoint");
        require(device->Activate(__uuidof(IAudioEndpointVolume),CLSCTX_ALL,nullptr,
            reinterpret_cast<void**>(volume_.GetAddressOf())),"Loopback volume");
        callback_=Microsoft::WRL::Make<detail::EndpointSilence>();
        if(!callback_) throw std::bad_alloc();
        require(volume_->RegisterControlChangeNotify(callback_.Get()),"Watch loopback mute");
        try {
            const auto before=callback_->snapshot();float scalar=0;BOOL muted=TRUE;
            require(volume_->GetMasterVolumeLevelScalar(&scalar),"Read loopback volume");
            require(volume_->GetMute(&muted),"Read loopback mute");
            callback_->initialise(before,scalar,muted!=FALSE);
        } catch(...) {
            volume_->UnregisterControlChangeNotify(callback_.Get());callback_.Reset();throw;
        }
    }
    ~LoopbackSilence() {if(callback_) volume_->UnregisterControlChangeNotify(callback_.Get());}
    bool apply(float* samples,std::size_t frames) const noexcept {
        return callback_&&callback_->apply(samples,frames);
    }
};
ComPtr<IAudioClient> client(IMMDeviceEnumerator* e,const std::wstring& id) {ComPtr<IMMDevice> device;require(e->GetDevice(id.c_str(),&device),"GetDevice");ComPtr<IAudioClient> c;require(device->Activate(__uuidof(IAudioClient),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(c.GetAddressOf())),"Activate audio");return c;}
WAVEFORMATEX format() {WAVEFORMATEX f{};f.wFormatTag=WAVE_FORMAT_IEEE_FLOAT;f.nChannels=2;f.nSamplesPerSec=48000;f.wBitsPerSample=32;f.nBlockAlign=8;f.nAvgBytesPerSec=384000;return f;}
void initialize(IAudioClient* c,HANDLE event,bool loopback,bool raw) {
    if(raw) {ComPtr<IAudioClient2> c2;require(c->QueryInterface(IID_PPV_ARGS(&c2)),"AudioClient2");AudioClientProperties properties{};properties.cbSize=sizeof(properties);properties.eCategory=AudioCategory_Media;properties.Options=AUDCLNT_STREAMOPTIONS_RAW;require(c2->SetClientProperties(&properties),"Raw stream properties");}
    auto f=format();WAVEFORMATEX* mix=nullptr;require(c->GetMixFormat(&mix),"GetMixFormat");
    bool nativeFloat=mix->wFormatTag==WAVE_FORMAT_IEEE_FLOAT;
    if(mix->wFormatTag==WAVE_FORMAT_EXTENSIBLE&&mix->cbSize>=22) nativeFloat=IsEqualGUID(reinterpret_cast<WAVEFORMATEXTENSIBLE*>(mix)->SubFormat,KSDATAFORMAT_SUBTYPE_IEEE_FLOAT);
    const bool native=nativeFloat&&mix->nChannels==2&&mix->nSamplesPerSec==48000&&mix->wBitsPerSample==32&&mix->nBlockAlign==8;
    DWORD flags=AUDCLNT_STREAMFLAGS_EVENTCALLBACK;if(!native) flags|=AUDCLNT_STREAMFLAGS_AUTOCONVERTPCM|AUDCLNT_STREAMFLAGS_SRC_DEFAULT_QUALITY;if(loopback) flags|=AUDCLNT_STREAMFLAGS_LOOPBACK;
    auto hr=c->Initialize(AUDCLNT_SHAREMODE_SHARED,flags,200000,0,native?mix:&f,nullptr);CoTaskMemFree(mix);
    require(hr,loopback?"Initialize loopback endpoint":"Initialize audio endpoint");require(c->SetEventHandle(event),"SetEventHandle");
}
}
std::vector<Endpoint> endpoints() {
    Com com;auto e=enumerator();std::vector<Endpoint> result;
    std::wstring defaultId;ComPtr<IMMDevice> defaultDevice;
    if(SUCCEEDED(e->GetDefaultAudioEndpoint(eRender,eConsole,&defaultDevice))) {LPWSTR id=nullptr;if(SUCCEEDED(defaultDevice->GetId(&id))) {defaultId=id;CoTaskMemFree(id);}}
    std::wstring defaultCaptureId;defaultDevice.Reset();
    if(SUCCEEDED(e->GetDefaultAudioEndpoint(eCapture,eConsole,&defaultDevice))) {LPWSTR id=nullptr;if(SUCCEEDED(defaultDevice->GetId(&id))) {defaultCaptureId=id;CoTaskMemFree(id);}}
    for(auto flow:{eRender,eCapture}) {ComPtr<IMMDeviceCollection> collection;require(e->EnumAudioEndpoints(flow,DEVICE_STATE_ACTIVE,&collection),"Enumerate endpoints");UINT count=0;require(collection->GetCount(&count),"Endpoint count");
        for(UINT i=0;i<count;++i) {ComPtr<IMMDevice> device;require(collection->Item(i,&device),"Endpoint");LPWSTR id=nullptr;require(device->GetId(&id),"Endpoint ID");Endpoint item;item.id=id;CoTaskMemFree(id);item.capture=flow==eCapture;
            ComPtr<IPropertyStore> properties;require(device->OpenPropertyStore(STGM_READ,&properties),"Properties");PROPVARIANT name;PropVariantInit(&name);auto hr=properties->GetValue(PKEY_Device_FriendlyName,&name);if(SUCCEEDED(hr)&&name.vt==VT_LPWSTR) item.name=name.pwszVal;PropVariantClear(&name);
            PropVariantInit(&name);if(SUCCEEDED(properties->GetValue(PKEY_DeviceInterface_FriendlyName,&name))&&name.vt==VT_LPWSTR) item.interfaceName=name.pwszVal;PropVariantClear(&name);
            item.defaultCapture=item.id==defaultCaptureId;
            item.defaultRender=item.id==defaultId;ComPtr<IAudioMeterInformation> meter;if(SUCCEEDED(device->Activate(__uuidof(IAudioMeterInformation),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(meter.GetAddressOf())))) meter->GetPeakValue(&item.peak);
            ComPtr<IAudioEndpointVolume> volume;if(SUCCEEDED(device->Activate(__uuidof(IAudioEndpointVolume),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(volume.GetAddressOf())))) {volume->GetMasterVolumeLevelScalar(&item.volume);BOOL muted=FALSE;volume->GetMute(&muted);item.muted=muted!=FALSE;}
            result.push_back(std::move(item));
        }
    }return result;
}
bool endpointKnown(const std::wstring& id) {
    if(id.empty()) return false;
    Com com;auto e=enumerator();ComPtr<IMMDevice> device;
    return SUCCEEDED(e->GetDevice(id.c_str(),&device))&&device;
}
void setEndpointVolume(const std::wstring& id,float scalar) {
    if(!std::isfinite(scalar)||scalar<0||scalar>1) throw std::runtime_error("Output volume must be between 0 and 100 percent");
    Com com;auto e=enumerator();ComPtr<IMMDevice> device;require(e->GetDevice(id.c_str(),&device),"Volume endpoint");
    ComPtr<IAudioEndpointVolume> volume;require(device->Activate(__uuidof(IAudioEndpointVolume),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(volume.GetAddressOf())),"Endpoint volume");
    require(volume->SetMasterVolumeLevelScalar(scalar,nullptr),"Set output volume");
    require(volume->SetMute(FALSE,nullptr),"Unmute output");
}
EndpointLevel endpointLevel(const std::wstring& id) {
    EndpointLevel level;
    if(id.empty()) return level;
    Com com;auto e=enumerator();ComPtr<IMMDevice> device;require(e->GetDevice(id.c_str(),&device),"Level endpoint");
    ComPtr<IAudioEndpointVolume> volume;
    // A device without an endpoint volume control is reported as unknown
    // rather than as zero: the window must not claim a level it never read.
    if(FAILED(device->Activate(__uuidof(IAudioEndpointVolume),CLSCTX_ALL,nullptr,reinterpret_cast<void**>(volume.GetAddressOf())))) return level;
    float scalar=-1;BOOL muted=FALSE;
    if(FAILED(volume->GetMasterVolumeLevelScalar(&scalar))) return level;
    if(SUCCEEDED(volume->GetMute(&muted))) level.muted=muted!=FALSE;
    level.volume=scalar;
    return level;
}
void runWasapi(const LiveOptions& options,LiveState& state) {
    static_assert(std::atomic<float>::is_always_lock_free && std::atomic<unsigned>::is_always_lock_free && std::atomic<bool>::is_always_lock_free);
    if(!options.tone&&options.inputId==options.outputId) throw std::runtime_error("Input and output endpoints must differ to avoid feedback");
    Com com;auto e=enumerator();Event renderEvent,captureEvent;
    LoopbackSilence silence;
    if(options.loopback&&!options.tone) silence.watch(e.Get(),options.inputId);
    auto render=client(e.Get(),options.outputId);initialize(render.Get(),renderEvent.handle,false,options.raw);
    ComPtr<IAudioRenderClient> writer;require(render->GetService(IID_PPV_ARGS(&writer)),"Render service");UINT32 capacity=0;require(render->GetBufferSize(&capacity),"Render buffer size");
    ComPtr<IAudioClient> capture;ComPtr<IAudioCaptureClient> reader;
    if(!options.tone) {capture=client(e.Get(),options.inputId);initialize(capture.Get(),captureEvent.handle,options.loopback,false);require(capture->GetService(IID_PPV_ARGS(&reader)),"Capture service");}
    std::vector<float> block(std::size_t(capacity)*2);StreamBuffer fifo;Engine engine;engine.prepare(48000,options.parameters);
    const std::size_t target=std::max<std::size_t>(960,capacity*2);if(target>=8192) throw std::runtime_error("Endpoint buffer too large for realtime FIFO");
    // Fixed part of the round trip: what each endpoint reports plus the render
    // buffer we have to keep filled. The FIFO adds a varying part, measured
    // on every pass below. Reported in milliseconds at 48 kHz.
    REFERENCE_TIME renderLatency=0,captureLatency=0;
    render->GetStreamLatency(&renderLatency);
    if(capture) capture->GetStreamLatency(&captureLatency);
    // REFERENCE_TIME counts 100 ns units.
    const float endpointMs=static_cast<float>(renderLatency+captureLatency)/10000.f;
    const float renderBufferMs=static_cast<float>(capacity)*1000.f/48000.f;
    BYTE* buffer=nullptr;require(writer->GetBuffer(capacity,&buffer),"Prime buffer");require(writer->ReleaseBuffer(capacity,AUDCLNT_BUFFERFLAGS_SILENT),"Prime release");
    Running renderRun{render.Get()},captureRun{capture.Get()};Priority priority;
    if(capture) captureRun.start();renderRun.start();
    bool primed=options.tone;double phase=0;float fade=0;
    auto volumeRevision=silence.snapshot();
    auto lastRender=std::chrono::steady_clock::now();
    HANDLE events[]{renderEvent.handle,captureEvent.handle};
    while(!state.stop.load(std::memory_order_relaxed)) {
        const auto wait=WaitForMultipleObjects(options.tone?1:2,events,FALSE,100);
        if(wait==WAIT_FAILED) throw std::runtime_error("Audio event wait failed");
        if(std::chrono::steady_clock::now()-lastRender>std::chrono::seconds(5))
            throw std::runtime_error("La salida de audio dejo de responder");
        const auto nextVolumeRevision=silence.snapshot();
        if(nextVolumeRevision!=volumeRevision) {
            volumeRevision=nextVolumeRevision;
            fifo.reset();engine.reset();primed=options.tone;fade=0;
        }
        if(reader) {UINT32 packet=0;require(reader->GetNextPacketSize(&packet),"Capture packet");
            // Limit work per wakeup: avoid starving render under unexpected backlog.
            for(unsigned packets=0;packet&&packets<64;++packets) {
                BYTE* source=nullptr;UINT32 frames=0;DWORD flags=0;require(reader->GetBuffer(&source,&frames,&flags,nullptr,nullptr),"Capture buffer");
                if(flags&AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) {state.discontinuities.fetch_add(1,std::memory_order_relaxed);fifo.reset();primed=false;fade=0;}
                const auto* samples=reinterpret_cast<const float*>(source);bool dropped=false;
                for(UINT32 i=0;i<frames;++i) {float l=0,r=0;if(!(flags&AUDCLNT_BUFFERFLAGS_SILENT)) {l=samples[2*i];r=samples[2*i+1];if(!std::isfinite(l)) l=0;if(!std::isfinite(r)) r=0;}if(!fifo.push(l,r)) dropped=true;}
                require(reader->ReleaseBuffer(frames),"Capture release");if(dropped) {state.overflows.fetch_add(1,std::memory_order_relaxed);fifo.reset();primed=false;fade=0;}
                require(reader->GetNextPacketSize(&packet),"Next packet");
            }
        }
        if(!primed&&fifo.size()>=target) primed=true;
        UINT32 padding=0;require(render->GetCurrentPadding(&padding),"Render padding");if(padding>capacity) throw std::runtime_error("Invalid render padding");const UINT32 available=capacity-padding;if(!available) continue;
        fifo.tune(target);bool underrun=false;
        for(UINT32 i=0;i<available;++i) {
            float l=0,r=0;bool valid=primed;
            if(options.tone) {l=r=0.06f*static_cast<float>(std::sin(phase)+0.4*std::sin(phase*3));phase+=2*std::numbers::pi*90/48000;if(phase>=2*std::numbers::pi) phase-=2*std::numbers::pi;}
            else if(primed&&!fifo.pop(l,r)) {valid=false;underrun=true;primed=false;}
            if(valid) fade=std::min(1.f,fade+1.f/240);else fade=0;
            block[2*i]=l*fade;block[2*i+1]=r*fade;
        }
        if(underrun) state.underruns.fetch_add(1,std::memory_order_relaxed);
        engine.setControls(state.bass.load(std::memory_order_relaxed),state.clarity.load(std::memory_order_relaxed),state.dynamics.load(std::memory_order_relaxed),state.spatial.load(std::memory_order_relaxed));
        engine.setEnabled(state.enabled.load(std::memory_order_relaxed));engine.setIntensity(state.intensity.load(std::memory_order_relaxed));engine.process(block.data(),block.data(),available);
        require(writer->GetBuffer(available,&buffer),"Render buffer");
        std::copy_n(block.data(),std::size_t(available)*2,reinterpret_cast<float*>(buffer));
        const bool silenced=silence.apply(reinterpret_cast<float*>(buffer),available);
        require(writer->ReleaseBuffer(available,silenced?AUDCLNT_BUFFERFLAGS_SILENT:0),"Render release");
        lastRender=std::chrono::steady_clock::now();
        const auto meters=engine.meters();state.transientLiftDb.store(meters.transientLiftDb,std::memory_order_relaxed);state.dynamicLiftDb.store(meters.dynamicLiftDb,std::memory_order_relaxed);state.inputPeak.store(meters.inputPeak,std::memory_order_relaxed);state.outputPeak.store(silenced?0.f:meters.outputPeak,std::memory_order_relaxed);state.reduction.store(meters.gainReductionDb,std::memory_order_relaxed);state.enhancement.store(meters.enhancement,std::memory_order_relaxed);state.queued.store(static_cast<unsigned>(fifo.size()),std::memory_order_relaxed);
        state.latencyMs.store(endpointMs+renderBufferMs+static_cast<float>(fifo.size())*1000.f/48000.f,std::memory_order_relaxed);
    }
}

std::vector<Endpoint> captureEndpoints() {
    std::vector<Endpoint> all=endpoints(),out;
    for(auto& e:all) if(e.capture) out.push_back(std::move(e));
    return out;
}
static CaptureResult captureMeasurementImpl(const std::wstring& outputId,const std::wstring& inputId,
                                 const std::vector<float>& tones,bool rawOutput) {
    if(tones.empty()||tones.size()%2||tones.size()>48000*2*30||
       !std::all_of(tones.begin(),tones.end(),[](float sample){return std::isfinite(sample)&&std::abs(sample)<=1;}))
        throw std::runtime_error("Senal de medicion invalida");
    if(outputId==inputId) throw std::runtime_error("La salida y el microfono no pueden ser el mismo dispositivo");
    Com com;auto e=enumerator();Event renderEvent,captureEvent;
    auto render=client(e.Get(),outputId);initialize(render.Get(),renderEvent.handle,false,rawOutput);
    ComPtr<IAudioRenderClient> writer;require(render->GetService(IID_PPV_ARGS(&writer)),"Render service");
    UINT32 capacity=0;require(render->GetBufferSize(&capacity),"Render buffer size");
    auto capture=client(e.Get(),inputId);initialize(capture.Get(),captureEvent.handle,false,false);
    ComPtr<IAudioCaptureClient> reader;require(capture->GetService(IID_PPV_ARGS(&reader)),"Capture service");

    CaptureResult result;
    result.gaps.reserve(128);
    const std::size_t toneFrames=tones.size()/2;
    Priority priority;
    Running captureRun{capture.Get()};captureRun.start();

    // The microphone has its own timing, so it is read on its own thread with
    // its own event. Draining it from the render loop loses packets: the
    // render event fires on a different schedule and by the time we look, the
    // captured audio has already been discarded.
    std::atomic<bool> playing{false},listenerFailed{false};
    std::exception_ptr listenerFailure;
    std::vector<float> room,tonesHeard;
    room.reserve(96000);tonesHeard.reserve(toneFrames+96000);
    std::jthread listener([&](std::stop_token stop){
      try {
        Com threadCom;Priority capturePriority;
        detail::CaptureTimeline timeline;bool timelineReset=false;
        while(!stop.stop_requested()) {
            const auto wait=WaitForSingleObject(captureEvent.handle,100);
            if(wait==WAIT_FAILED) throw std::runtime_error("Capture event wait failed");
            if(wait!=WAIT_OBJECT_0) continue;
            UINT32 available=0;
            require(reader->GetNextPacketSize(&available),"Measurement packet");
            while(available&&!stop.stop_requested()) {
                BYTE* data=nullptr;UINT32 frames=0;DWORD flags=0;UINT64 position=0;
                require(reader->GetBuffer(&data,&frames,&flags,&position,nullptr),"Measurement capture buffer");
                const bool inSweep=playing.load(std::memory_order_acquire);
                const auto gap=timeline.packet(position,frames,!(flags&AUDCLNT_BUFFERFLAGS_TIMESTAMP_ERROR));
                if(flags&AUDCLNT_BUFFERFLAGS_DATA_DISCONTINUITY) ++result.discontinuities;
                if(inSweep&&gap.reset) timelineReset=true;
                const auto* samples=reinterpret_cast<const float*>(data);
                auto& into=inSweep?tonesHeard:room;
                // Reserve before acquiring packets and reject excess backlog
                // rather than growing vectors while holding an audio buffer.
                if(gap.missing>into.capacity()-into.size()||
                   into.size()+static_cast<std::size_t>(gap.missing)+frames>into.capacity()) {
                    reader->ReleaseBuffer(frames);
                    throw std::runtime_error("Measurement capture exceeded its buffer");
                }
                // Preserve the clock if samples really went missing: simply
                // joining packets would shift all subsequent tone windows.
                if(inSweep&&gap.missing) {
                    result.gaps.push_back({into.size(),static_cast<std::size_t>(gap.missing)});
                    result.missingFrames+=static_cast<std::size_t>(gap.missing);
                }
                into.insert(into.end(),static_cast<std::size_t>(gap.missing),0.f);
                for(UINT32 i=0;i<frames;++i) {
                    float v=0;
                    if(!(flags&AUDCLNT_BUFFERFLAGS_SILENT)) v=0.5f*(samples[2*i]+samples[2*i+1]);
                    into.push_back(std::isfinite(v)?v:0.f);
                }
                require(reader->ReleaseBuffer(frames),"Measurement capture release");
                require(reader->GetNextPacketSize(&available),"Next measurement packet");
            }
        }
        if(timelineReset) throw std::runtime_error("El reloj del microfono cambio durante la medicion. Repite la prueba.");
      } catch(...) {
          listenerFailure=std::current_exception();listenerFailed.store(true,std::memory_order_release);
      }
    });

    // One second of room before anything plays: that is the noise floor the
    // measurement is judged against.
    std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    playing.store(true,std::memory_order_release);

    // Give Start an actual first block, rather than starting an empty stream
    // and hoping its first event arrives before the initial capture glitch.
    std::size_t played=std::min<std::size_t>(capacity,toneFrames);
    BYTE* first=nullptr;require(writer->GetBuffer(capacity,&first),"Measurement prime buffer");
    auto* firstSamples=reinterpret_cast<float*>(first);
    std::fill_n(firstSamples,std::size_t(capacity)*2,0.f);
    std::copy_n(tones.data(),played*2,firstSamples);
    require(writer->ReleaseBuffer(capacity,0),"Measurement prime release");
    Running running{render.Get()};
    running.start();
    const auto deadline=std::chrono::steady_clock::now()+std::chrono::milliseconds(
        static_cast<long long>(toneFrames*1000.0/48000.0)+1500);
    while(played<toneFrames&&std::chrono::steady_clock::now()<deadline&&!listenerFailed.load(std::memory_order_acquire)) {
        const auto wait=WaitForSingleObject(renderEvent.handle,200);
        if(wait==WAIT_FAILED) throw std::runtime_error("Measurement render event wait failed");
        if(wait!=WAIT_OBJECT_0) continue;
        UINT32 padding=0;
        require(render->GetCurrentPadding(&padding),"Measurement render padding");
        if(padding>capacity) throw std::runtime_error("Invalid measurement render padding");
        const UINT32 free=capacity-padding;
        if(!free) continue;
        BYTE* buffer=nullptr;
        require(writer->GetBuffer(free,&buffer),"Measurement render buffer");
        auto* out=reinterpret_cast<float*>(buffer);
        for(UINT32 i=0;i<free;++i) {
            const bool have=played<toneFrames;
            out[2*i]=have?tones[2*played]:0.f;
            out[2*i+1]=have?tones[2*played+1]:0.f;
            if(have) ++played;
        }
        require(writer->ReleaseBuffer(free,0),"Measurement render release");
    }
    // Let the tail of the last tone reach the microphone before stopping.
    std::this_thread::sleep_for(std::chrono::milliseconds(400));
    listener.request_stop();
    listener.join();
    if(listenerFailure) std::rethrow_exception(listenerFailure);
    if(played!=toneFrames) throw std::runtime_error("Measurement playback timed out");
    result.silence=std::move(room);
    result.recorded=std::move(tonesHeard);
    return result;
}
CaptureResult captureMeasurement(const std::wstring& outputId,const std::wstring& inputId,
                                 const std::vector<float>& tones,bool rawOutput) {
    // Both audio threads join the MTA. The UI may be STA after a file dialog;
    // interfaces created there cannot be shared with the microphone thread.
    return std::async(std::launch::async,[&]{return captureMeasurementImpl(outputId,inputId,tones,rawOutput);}).get();
}
}
