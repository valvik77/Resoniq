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
