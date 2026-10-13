#include "dps/wav.hpp"
#include "atomic_file.hpp"
#include <bit>
#include <cmath>
#include <fstream>
#include <limits>
#include <stdexcept>
#include <string>
namespace dps {
namespace {
std::uint32_t read(std::istream& f,unsigned n) { std::uint32_t v=0;for(unsigned i=0;i<n;++i) {int c=f.get();if(c<0) throw std::runtime_error("Truncated WAV");v|=std::uint32_t(c)<<(8*i);}return v; }
void put(std::ostream& f,std::uint32_t v,unsigned n) {for(unsigned i=0;i<n;++i) f.put(static_cast<char>(v>>(8*i)));}
std::string tag(std::istream& f) {std::string s(4,' ');f.read(s.data(),4);if(!f) throw std::runtime_error("Truncated WAV");return s;}
}
Wav readWav(const std::filesystem::path& path) {
    std::ifstream f(path,std::ios::binary);if(!f) throw std::runtime_error("Cannot open input WAV");
    if(tag(f)!="RIFF") throw std::runtime_error("Expected RIFF WAV");
    const auto size=read(f,4);if(tag(f)!="WAVE") throw std::runtime_error("Expected WAVE");
    f.seekg(0,std::ios::end);const auto actual=f.tellg();if(actual<0||std::uint64_t(actual)<std::uint64_t(size)+8) throw std::runtime_error("Truncated RIFF");f.seekg(12);
    unsigned format=0,channels=0,bits=0,align=0;std::uint32_t rate=0,byteRate=0;
    std::vector<unsigned char> data;bool haveFmt=false,haveData=false;
    const std::uint64_t end=std::uint64_t(size)+8;
    while(std::uint64_t(f.tellg())+8<=end) {
        const auto id=tag(f);const auto n=read(f,4);const auto start=f.tellg();
        if(std::uint64_t(start)+n+(n&1)>end) throw std::runtime_error("Invalid WAV chunk size");
        if(id=="fmt ") {if(haveFmt||n<16) throw std::runtime_error("Invalid fmt chunk");format=read(f,2);channels=read(f,2);rate=read(f,4);byteRate=read(f,4);align=read(f,2);bits=read(f,2);haveFmt=true;}
        if(id=="data") {if(haveData||n>512*1024*1024) throw std::runtime_error("Invalid or oversized data chunk");data.resize(n);f.read(reinterpret_cast<char*>(data.data()),n);if(!f) throw std::runtime_error("Truncated audio data");haveData=true;}
        f.seekg(start+std::streamoff(n+(n&1)));
    }
    if(!haveFmt||!haveData||channels!=2||rate<16000||rate>192000||!((format==1&&(bits==16||bits==24||bits==32))||(format==3&&bits==32))||align!=channels*bits/8||byteRate!=rate*align||data.size()%align) throw std::runtime_error("Supported WAV: stereo PCM16/24/32 or float32, 16-192 kHz");
    Wav wav{rate,{}};wav.samples.reserve(data.size()/(bits/8));
    for(std::size_t i=0;i<data.size();i+=bits/8) {
        std::uint32_t v=0;for(unsigned j=0;j<bits/8;++j) v|=std::uint32_t(data[i+j])<<(8*j);
        float x;
        if(format==3) x=std::bit_cast<float>(v);
        else {if(bits<32&&(v&(1u<<(bits-1)))) v|=(~0u<<bits);x=static_cast<float>(static_cast<double>(std::bit_cast<std::int32_t>(v))/std::pow(2.,bits-1));}
        if(!std::isfinite(x)) throw std::runtime_error("Non-finite WAV sample");wav.samples.push_back(x);
    }
    return wav;
}
void writeWav(const std::filesystem::path& path,const Wav& wav) {
    if(wav.samples.size()%2||wav.samples.size()>(std::numeric_limits<std::uint32_t>::max()-50ull)/4||wav.sampleRate<16000||wav.sampleRate>192000) throw std::runtime_error("Invalid output WAV");
    for(float x:wav.samples) if(!std::isfinite(x)) throw std::runtime_error("Non-finite output sample");
    detail::AtomicFile file(path);auto& f=file.stream();
    const auto bytes=static_cast<std::uint32_t>(wav.samples.size()*4);
    f.write("RIFF",4);put(f,50+bytes,4);f.write("WAVEfmt ",8);put(f,18,4);put(f,3,2);put(f,2,2);put(f,wav.sampleRate,4);put(f,wav.sampleRate*8,4);put(f,8,2);put(f,32,2);put(f,0,2);
    f.write("fact",4);put(f,4,4);put(f,static_cast<std::uint32_t>(wav.samples.size()/2),4);f.write("data",4);put(f,bytes,4);
    for(float x:wav.samples) put(f,std::bit_cast<std::uint32_t>(x),4);
    file.commit();
}
}
