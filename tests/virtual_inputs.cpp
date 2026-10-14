#include "dps/wasapi.hpp"
#include "dps/virtual_transport.hpp"
#include <iostream>
#include <stdexcept>
namespace {void check(bool value,const char* message) {if(!value) throw std::runtime_error(message);}}
int main() {
    try {
        dps::Endpoint own,cable,physical;
        own.id=L"own";own.name=L"Altavoces (DPS Audio Virtual Transport)";
        cable.id=L"cable";cable.name=L"CABLE Input (VB-Audio Virtual Cable)";cable.defaultRender=true;
        physical.id=L"speakers";physical.name=L"Altavoces (Realtek Audio)";
        check(dps::transportKind(own)==dps::TransportKind::Own,"Own transport not identified");
        check(dps::transportKind(cable)==dps::TransportKind::VbCable,"Cable not identified");
        check(dps::transportKind(physical)==dps::TransportKind::None,"Physical output offered as virtual input");
        std::vector<dps::Endpoint> inputs{cable,own};
        check(dps::selectTransport(inputs,L"own")==1,"Saved own input overridden by default cable");
        check(dps::selectTransport(inputs,L"missing")==-1,"Missing saved input silently substituted");
        check(dps::selectTransport(inputs,L"")==0,"Default virtual input not selected");
        cable.defaultRender=false;inputs={own,cable};
        check(dps::selectTransport(inputs,L"")==0,"Own input fallback lost");
        std::cout<<"PASS: virtual input recognition, saved selection and missing-device handling\n";
    } catch(const std::exception& error) {std::cerr<<error.what()<<'\n';return 1;}
}
