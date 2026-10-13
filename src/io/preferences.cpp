#include "dps/preferences.hpp"
#include "atomic_file.hpp"
#include <algorithm>
#include <locale>
#include <sstream>
#include <set>
#include <iomanip>

namespace dps {
namespace {
void validateVirtualId(const std::string& id) {
    if(id.size()>4096||id.find_first_of("\r\n")!=std::string::npos||id.find('\0')!=std::string::npos)
        throw std::runtime_error("Invalid virtual input ID");
}
}
Preferences loadPreferences(const std::filesystem::path& path) {
    if(!std::filesystem::exists(path)) return {};
    if(std::filesystem::file_size(path)>16384) throw std::runtime_error("Preferences too large");
    std::ifstream input(path);
    if(!input) throw std::runtime_error("Cannot read preferences");
    Preferences preferences;
    std::set<std::string> keys;
    std::string line;
    while(std::getline(input,line)) {
        std::istringstream row(line);row.imbue(std::locale::classic());
        std::string key;if(!(row>>key)||key.starts_with('#')) continue;
        // Unknown keys are ignored for forward compatibility, one line at a
        // time: their values must not hide subsequent recognised settings.
        const bool known=key=="start_with_windows"||key=="start_minimised"||
            key=="close_exits"||key=="auto_reconnect"||key=="dsp_at_start"||key=="theme_mode"||key=="virtual_input_id";
        if(!known) continue;
        if(key=="virtual_input_id") {
            if(!keys.insert(key).second||!(row>>std::quoted(preferences.virtualInputId))) throw std::runtime_error("Invalid virtual input ID");
            row>>std::ws;if(!row.eof()) throw std::runtime_error("Trailing virtual input ID");validateVirtualId(preferences.virtualInputId);continue;
        }
        int value=0;
        if(!keys.insert(key).second||!(row>>value)) throw std::runtime_error("Invalid preference: "+key);
        row>>std::ws;if(!row.eof()) throw std::runtime_error("Trailing preference data: "+key);
        if(key=="start_with_windows") preferences.startWithWindows=value!=0;
        else if(key=="start_minimised") preferences.startMinimised=value!=0;
        else if(key=="close_exits") preferences.closeExits=value!=0;
        else if(key=="auto_reconnect") preferences.autoReconnect=value!=0;
        else if(key=="dsp_at_start") preferences.dspAtStart=std::clamp(value,0,2);
        else if(key=="theme_mode") preferences.themeMode=std::clamp(value,0,2);
    }
    if(input.bad()) throw std::runtime_error("Cannot read preferences");
    return preferences;
}
void savePreferences(const std::filesystem::path& path,const Preferences& preferences) {
    validateVirtualId(preferences.virtualInputId);
    if(preferences.dspAtStart<0||preferences.dspAtStart>2||preferences.themeMode<0||preferences.themeMode>2)
        throw std::invalid_argument("Invalid preference mode");
    detail::AtomicFile file(path);auto& output=file.stream();output.imbue(std::locale::classic());
    output<<"start_with_windows "<<preferences.startWithWindows
          <<"\nstart_minimised "<<preferences.startMinimised
          <<"\nclose_exits "<<preferences.closeExits
          <<"\nauto_reconnect "<<preferences.autoReconnect
          <<"\ndsp_at_start "<<preferences.dspAtStart
          <<"\ntheme_mode "<<preferences.themeMode<<'\n';
    if(!preferences.virtualInputId.empty()) output<<"virtual_input_id "<<std::quoted(preferences.virtualInputId)<<'\n';
    file.commit();
}
}
