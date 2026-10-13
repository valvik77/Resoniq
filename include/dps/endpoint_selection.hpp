#pragma once
#include <string_view>
#include <vector>
namespace dps {
// Never substitute a different speaker for a saved destination: reconnecting
// headphones must not unexpectedly send their audio to another device.
template<class Endpoint> int selectOutput(const std::vector<Endpoint>& outputs,std::wstring_view preferred) {
    if(!preferred.empty()) {
        for(std::size_t i=0;i<outputs.size();++i) if(outputs[i].id==preferred) return static_cast<int>(i);
        return -1;
    }
    for(std::size_t i=0;i<outputs.size();++i) if(outputs[i].defaultRender) return static_cast<int>(i);
    return outputs.empty()?-1:0;
}
}
