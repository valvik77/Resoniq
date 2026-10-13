#pragma once
#include <cwctype>
#include <string>
#include <vector>
namespace dps {
enum class TransportKind {None,Own,VbCable};
inline std::wstring transportText(std::wstring value) {
    for(auto& c:value) c=static_cast<wchar_t>(std::towlower(c));return value;
}
template<class Endpoint> TransportKind transportKind(const Endpoint& endpoint) {
    const auto name=transportText(endpoint.name),interfaceName=transportText(endpoint.interfaceName);
    if(name.find(L"dps audio")!=std::wstring::npos) return TransportKind::Own;
    if(name.find(L"vb-audio virtual cable")!=std::wstring::npos||interfaceName.find(L"vb-audio virtual cable")!=std::wstring::npos) return TransportKind::VbCable;
    return TransportKind::None;
}
template<class Endpoint> int selectTransport(const std::vector<Endpoint>& inputs,const std::wstring& saved) {
    if(!saved.empty()) {for(std::size_t i=0;i<inputs.size();++i) if(inputs[i].id==saved) return static_cast<int>(i);return -1;}
    for(std::size_t i=0;i<inputs.size();++i) if(inputs[i].defaultRender) return static_cast<int>(i);
    int own=-1;for(std::size_t i=0;i<inputs.size();++i) if(transportKind(inputs[i])==TransportKind::Own) {
        if(own!=-1) return -1;own=static_cast<int>(i);
    }
    return own!=-1?own:inputs.size()==1?0:-1;
}
}
