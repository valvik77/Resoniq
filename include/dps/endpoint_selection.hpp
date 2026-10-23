#pragma once
#include <string_view>
#include <vector>
namespace dps {
// Never substitute a different speaker for a saved destination while Windows
// still knows that endpoint: reconnecting headphones must not unexpectedly
// send their audio to another device, and unplugged headphones are still known.
// An endpoint Windows no longer knows at all has been replaced rather than
// unplugged, and waiting for it in silence helps nobody, so the default output
// is proposed instead. The caller decides which case it is through
// `preferredKnown`, and leaves it true whenever it cannot tell.
template<class Endpoint> int selectOutput(const std::vector<Endpoint>& outputs,std::wstring_view preferred,
                                          bool preferredKnown=true) {
    if(!preferred.empty()) {
        for(std::size_t i=0;i<outputs.size();++i) if(outputs[i].id==preferred) return static_cast<int>(i);
        if(preferredKnown) return -1;
    }
    for(std::size_t i=0;i<outputs.size();++i) if(outputs[i].defaultRender) return static_cast<int>(i);
    return outputs.empty()?-1:0;
}
}
