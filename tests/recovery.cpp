#include "dps/endpoint_selection.hpp"
#include "dps/endpoint_level.hpp"
#include <stdexcept>
#include <iostream>
#include <string>
#include <algorithm>
struct Endpoint {std::wstring id;bool defaultRender=false;};
void check(bool value,const char* message){if(!value) throw std::runtime_error(message);}
int main(){try {
    std::vector<Endpoint> outputs{{L"speakers",false},{L"headphones",true}};
    check(dps::selectOutput(outputs,L"headphones")==1,"Saved output not selected");
    check(dps::selectOutput(outputs,L"")==1,"First use ignores physical default");
    outputs.pop_back();
    check(dps::selectOutput(outputs,L"headphones")==-1,"Missing headphones leak to speakers");
    outputs.push_back({L"headphones",false});
    check(dps::selectOutput(outputs,L"headphones")==1,"Reconnected output not restored");
    std::reverse(outputs.begin(),outputs.end());
    check(dps::selectOutput(outputs,L"headphones")==0,"Enumeration order changes destination");
    outputs.clear();check(dps::selectOutput(outputs,L"headphones")==-1,"Empty outputs accepted");
    // Unplugged headphones are still known to Windows and must be waited for.
    // A destination Windows has forgotten has been replaced, so a default is
    // proposed rather than leaving the application silent for good.
    std::vector<Endpoint> others{{L"speakers",false},{L"line-out",false}};
    check(dps::selectOutput(others,L"headphones",true)==-1,"Unplugged headphones leak to another speaker");
    check(dps::selectOutput(others,L"headphones",false)==0,"Replaced destination left the app silent");
    others[1].defaultRender=true;
    check(dps::selectOutput(others,L"headphones",false)==1,"Proposal ignores the Windows default");
    check(dps::selectOutput(others,L"speakers",false)==0,"Available saved output not kept");
    others.clear();check(dps::selectOutput(others,L"headphones",false)==-1,"Proposal invented an output");
    // The window reports the physical level, so a missing reading must not be
    // shown as silence and a muted endpoint must not look like a live one.
    check(dps::volumeCaption({})==L"Volumen de Windows sin dato","Unknown level reported as a figure");
    check(dps::volumeCaption({-1.f,true})==L"Volumen de Windows sin dato","Unread level reported as muted");
    check(dps::volumeCaption({0.f,false})==L"Volumen de Windows 0 %","Zero volume lost its reading");
    check(dps::volumeCaption({.42f,false})==L"Volumen de Windows 42 %","Volume percentage wrong");
    check(dps::volumeCaption({.425f,false})==L"Volumen de Windows 43 %","Percentage not rounded");
    check(dps::volumeCaption({1.f,false})==L"Volumen de Windows 100 %","Full volume lost its reading");
    check(dps::volumeCaption({.3f,true})==L"Volumen de Windows silenciado (30 %)","Mute not reported with its level");
    std::cout<<"PASS: saved output, disconnect, reconnect, reorder, first-use selection and physical level caption\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
