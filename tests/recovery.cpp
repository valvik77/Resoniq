#include "dps/endpoint_selection.hpp"
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
    std::cout<<"PASS: saved output, disconnect, reconnect, reorder and first-use selection\n";
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
