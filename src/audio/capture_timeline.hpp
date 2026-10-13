#pragma once
#include <cstdint>
namespace dps::detail {
struct CaptureGap {std::uint64_t missing=0;bool reset=false;};
// A discontinuity notification does not quantify lost frames. Compare packet
// positions, without trusting timestamps explicitly marked invalid by WASAPI.
class CaptureTimeline {
    std::uint64_t next_=0;
    bool known_=false;
public:
    CaptureGap packet(std::uint64_t position,unsigned frames,bool valid) noexcept {
        if(!valid) {known_=false;return {};}
        CaptureGap gap;
        if(known_) {
            if(position>=next_) gap.missing=position-next_;
            else gap.reset=true;
        }
        next_=position+frames;known_=true;return gap;
    }
};
}
