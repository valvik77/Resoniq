#include "dps/virtual_transport.hpp"
#include <iostream>
#include <stdexcept>
struct Endpoint {std::wstring id,name,interfaceName;bool defaultRender=false;};
void check(bool ok,const char* message){if(!ok)throw std::runtime_error(message);}
int main(){try {
    using dps::TransportKind;
    Endpoint own{L"own",L"Altavoces (DPS Audio Virtual Transport)",L""};
    Endpoint cable{L"cable",L"CABLE Input (VB-Audio Virtual Cable)",L""};
    Endpoint renamed{L"renamed",L"Mi entrada",L"VB-Audio Virtual Cable"};
    Endpoint physical{L"speaker",L"USB Cable Speakers",L"USB Audio"};
    check(dps::transportKind(own)==TransportKind::Own,"Own transport lost");
    check(dps::transportKind(cable)==TransportKind::VbCable,"VB-CABLE not recognised");
    check(dps::transportKind(renamed)==TransportKind::VbCable,"Renamed cable identity lost");
    check(dps::transportKind(physical)==TransportKind::None,"Physical cable misclassified");
    std::vector<Endpoint> inputs{cable,own};
    check(dps::selectTransport(inputs,L"")==1,"Existing own route changed");
    inputs[0].defaultRender=true;check(dps::selectTransport(inputs,L"")==0,"Default cable ignored");
    check(dps::selectTransport(inputs,L"own")==1,"Saved transport ignored");
    check(dps::selectTransport(inputs,L"gone")==-1,"Missing transport silently substituted");
    inputs={cable,renamed};check(dps::selectTransport(inputs,L"")==-1,"Ambiguous cable pins guessed");
    inputs={cable};check(dps::selectTransport(inputs,L"")==0,"Single cable not selected");
    std::cout<<"PASS: own/cable identity, renamed pins, coexistence, persisted IDs and ambiguity\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
