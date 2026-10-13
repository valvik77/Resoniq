#include "dps/wav.hpp"
#include <fstream>
#include <iostream>
#include <limits>
#include <stdexcept>
#include <vector>
namespace {
void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}
std::vector<char> bytes(const std::filesystem::path& path) {
    std::ifstream input(path,std::ios::binary);
    return {std::istreambuf_iterator<char>(input),std::istreambuf_iterator<char>()};
}
}
int main() {
    const auto path=std::filesystem::temp_directory_path()/"resoniq-wav-validation.wav";
    try {
        const dps::Wav original{48000,{.1f,-.2f,.3f,-.4f}};
        dps::writeWav(path,original);const auto reference=bytes(path);
        check(dps::readWav(path).samples==original.samples,"Float WAV changes samples");
        for(const auto invalid:{dps::Wav{0,{0,0}},dps::Wav{48000,{0}},
                               dps::Wav{48000,{std::numeric_limits<float>::infinity(),0}}}) {
            bool failed=false;try {dps::writeWav(path,invalid);}catch(const std::exception&) {failed=true;}
            check(failed&&bytes(path)==reference,"Invalid WAV destroyed existing output");
        }
        // Truncation at any header or sample byte must be rejected.
        for(std::size_t length=0;length<reference.size();++length) {
            {std::ofstream output(path,std::ios::binary);output.write(reference.data(),static_cast<std::streamsize>(length));}
            bool failed=false;try {(void)dps::readWav(path);}catch(const std::exception&) {failed=true;}
            check(failed,"Truncated WAV accepted");
        }
        auto malformed=reference;malformed[22]=1; // mono format with stereo block align
        {std::ofstream output(path,std::ios::binary);output.write(malformed.data(),static_cast<std::streamsize>(malformed.size()));}
        bool failed=false;try {(void)dps::readWav(path);}catch(const std::exception&) {failed=true;}
        check(failed,"Invalid channel layout accepted");
        std::filesystem::remove(path);
        std::cout<<"PASS: exact float samples, invalid saves preserve output, all truncations and invalid channel layout\n";
    } catch(const std::exception& error) {std::filesystem::remove(path);std::cerr<<error.what()<<'\n';return 1;}
}
