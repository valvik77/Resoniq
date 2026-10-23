#include "gui_theme.hpp"
#include "gui_icons.hpp"
#include "windows_startup.hpp"
#include "resoniq_version.h"
#include "audio_session.hpp"
#include "dps/endpoint_selection.hpp"
#include "dps/virtual_transport.hpp"
#include "dps/wasapi.hpp"
#include "dps/profile.hpp"
#include "dps/preferences.hpp"
#include "dps/hardware.hpp"
#include "dps/measure.hpp"
#include "dps/wav.hpp"
#include <windows.h>
#include <commctrl.h>
#include <commdlg.h>
#include <shellapi.h>
#include <shlobj.h>
#include <dwmapi.h>
#include <uxtheme.h>
#include <windowsx.h>
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <filesystem>
#include <fstream>
#include <memory>
#include <thread>
#include <string>
#include <vector>
namespace {
using namespace resoniq::ui;
Palette theme=darkPalette;
void rounded(HDC dc,RECT r,COLORREF fill,COLORREF border,int radius) {
    if(r.right<=r.left||r.bottom<=r.top) return;
    radius=std::max(1,std::min(radius,static_cast<int>(std::min(r.right-r.left,r.bottom-r.top)/2)));
    Gdiplus::Graphics graphics(dc);graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::GraphicsPath path;const float d=static_cast<float>(radius*2);
    const float x=static_cast<float>(r.left)+.5f,y=static_cast<float>(r.top)+.5f;
    const float w=static_cast<float>(r.right-r.left)-1,h=static_cast<float>(r.bottom-r.top)-1;
    path.AddArc(x,y,d,d,180,90);path.AddArc(x+w-d,y,d,d,270,90);
    path.AddArc(x+w-d,y+h-d,d,d,0,90);path.AddArc(x,y+h-d,d,d,90,90);path.CloseFigure();
    auto color=[](COLORREF c){return Gdiplus::Color(255,GetRValue(c),GetGValue(c),GetBValue(c));};
    Gdiplus::SolidBrush brush(color(fill));graphics.FillPath(&brush,&path);
    Gdiplus::Pen pen(color(border),1.f);graphics.DrawPath(&pen,&path);
}
enum {Output=101,Device,Preset,Input,Toggle=105,Load,Save,Refresh,Raw,Reference=112,Route,SetDefault,Settings,Minimise,Close,Detect,Measure,About,Slider=200};
// Tray icon: keeps processing alive while the window is out of the way, the
// way a system-wide audio enhancer is normally used.
constexpr UINT TrayMessage=WM_APP+1;
enum {TrayShow=300,TrayToggle,TrayFavorite,TraySetDefault,TrayExit};
// Settings dialog controls. Kept in their own range so the Fluent painting
// recognises them: pill switches for the options, dark combo for the choice.
enum {SetStart=400,SetMinimised,SetCloseExits,SetReconnect,SetDspMode,SetTheme,SetOk,SetCancel,SetClose};
// Application preferences, separate from the sound profiles: these describe how
// the program behaves, not how it sounds, so they live in their own file.
using dps::Preferences;
const std::array<const char*,4> presets{"Natural","Enhanced","Powerful","Laptop Max"};
const std::array<const wchar_t*,4> presetLabels{L"RESONIQ Natural",L"RESONIQ Enhance",L"RESONIQ Power",L"RESONIQ Max"};
const std::array<const char*,7> devices{"Laptop","Headphones","External","Laptop Focus","Laptop Impact","Laptop Impact+","Measured"};
std::wstring wide(const std::string& s) {if(s.empty()) return {};int n=MultiByteToWideChar(CP_UTF8,MB_ERR_INVALID_CHARS,s.data(),static_cast<int>(s.size()),nullptr,0);if(!n) throw std::runtime_error("Invalid UTF-8 text");std::wstring w(n,0);MultiByteToWideChar(CP_UTF8,0,s.data(),static_cast<int>(s.size()),w.data(),n);return w;}
std::string utf8(const std::wstring& w) {if(w.empty()) return {};int n=WideCharToMultiByte(CP_UTF8,0,w.data(),static_cast<int>(w.size()),nullptr,0,nullptr,nullptr);std::string s(n,0);WideCharToMultiByte(CP_UTF8,0,w.data(),static_cast<int>(w.size()),s.data(),n,nullptr,nullptr);return s;}
using Session=resoniq::ui::AudioSession;
struct App {
    HWND window{},output{},input{},device{},preset{},toggle{},raw{};
    HWND hovered{};
    std::array<HWND,5> sliders{};
    std::array<float,96> activity{};
    HFONT font{},titleFont{};
    HBRUSH cardBrush=CreateSolidBrush(theme.card);
    std::filesystem::path root,profilePath,requestedProfile,dataRootOverride;
    dps::Profile profile;
    std::vector<dps::Endpoint> physical,virtualInputs;
    dps::EndpointLevel outputLevel;
    std::wstring virtualId,status=L"Preparado. Selecciona una salida e inicia el audio.";
    std::wstring startupWarning;
    Session session;
    std::ofstream telemetry;unsigned ticks=0;
    bool selfTest=false,audioTest=false,routed=false,tray=false,quitting=false,startTray=false,measurementDiagnostics=false;
    Preferences prefs{};HWND settings=nullptr,about=nullptr;unsigned reconnectAt=0;
    bool firstRun=false,powerSuspended=false,resumeAudio=false;
    ~App(){session.stop();DeleteObject(font);DeleteObject(titleFont);DeleteObject(cardBrush);}
    int scale(int n) const {return MulDiv(n,static_cast<int>(GetDpiForWindow(window)),96);}
    void paintChrome(HWND h,HDC dc) {
        RECT r;GetClientRect(h,&r);const int id=GetDlgCtrlID(h);const bool slider=id>=Slider&&id<Slider+5;
        const bool settingsPanel=id>=SetStart&&id<=SetTheme;
        auto base=CreateSolidBrush(slider||settingsPanel?theme.card:theme.background);FillRect(dc,&r,base);DeleteObject(base);
        const bool enabled=IsWindowEnabled(h)!=FALSE,hot=hovered==h,focused=GetFocus()==h;
        SetBkMode(dc,TRANSPARENT);SelectObject(dc,font);SetTextColor(dc,enabled?theme.text:theme.mutedText);
        if(slider) {
            RECT channel{},thumb{};SendMessageW(h,TBM_GETCHANNELRECT,0,reinterpret_cast<LPARAM>(&channel));
            SendMessageW(h,TBM_GETTHUMBRECT,0,reinterpret_cast<LPARAM>(&thumb));
            const int cy=(r.top+r.bottom)/2,cx=(thumb.left+thumb.right)/2;
            RECT rail{channel.left,cy-scale(2),channel.right,cy+scale(2)};
            rounded(dc,rail,theme.rail,theme.rail,scale(2));
            rail.right=std::clamp<LONG>(cx,rail.left,rail.right);rounded(dc,rail,theme.accent,theme.accent,scale(2));
            RECT knob{cx-scale(9),cy-scale(9),cx+scale(9),cy+scale(9)};
            rounded(dc,knob,theme.card,focused?theme.accent:theme.knobEdge,scale(9));
            const int radius=scale(hot?5:4);RECT core{cx-radius,cy-radius,cx+radius,cy+radius};
            rounded(dc,core,theme.accent,theme.accent,radius);return;
        }
        if(id==Raw||(id>=SetStart&&id<=SetReconnect)) {
            const bool on=SendMessageW(h,BM_GETCHECK,0,0)==BST_CHECKED;
            const int cy=(r.top+r.bottom)/2;RECT pill{scale(2),cy-scale(11),scale(44),cy+scale(11)};
            rounded(dc,pill,on&&enabled?theme.accent:theme.pillOff,focused?theme.accent:on&&enabled?theme.accent:theme.pillEdge,scale(11));
            const int cx=on?scale(32):scale(14);RECT knob{cx-scale(7),cy-scale(7),cx+scale(7),cy+scale(7)};
            const COLORREF knobFill=on&&enabled?theme.onAccent:theme.card;
            rounded(dc,knob,knobFill,knobFill,scale(7));
            wchar_t caption[128];GetWindowTextW(h,caption,128);r.left=scale(56);
            DrawTextW(dc,caption,-1,&r,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);return;
        }
        rounded(dc,r,enabled?hot?theme.surfaceHot:theme.surface:theme.card,focused?theme.accent:theme.border,scale(7));
        const auto index=SendMessageW(h,CB_GETCURSEL,0,0);
        if(index>=0) {
            const auto count=SendMessageW(h,CB_GETLBTEXTLEN,index,0);
            if(count>=0) {std::wstring value(static_cast<std::size_t>(count)+1,0);SendMessageW(h,CB_GETLBTEXT,index,reinterpret_cast<LPARAM>(value.data()));
                RECT label=r;label.left+=scale(13);label.right-=scale(35);DrawTextW(dc,value.c_str(),-1,&label,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}
        } else if(id==Input) {
            RECT label=r;label.left+=scale(13);label.right-=scale(35);
            DrawTextW(dc,L"Selecciona entrada",-1,&label,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
        }
        const int cx=r.right-scale(18),cy=(r.top+r.bottom)/2;
        POINT arrow[3]{{cx-scale(4),cy-scale(2)},{cx,cy+scale(2)},{cx+scale(4),cy-scale(2)}};
        auto pen=CreatePen(PS_SOLID,scale(1),enabled?theme.text:theme.mutedText);auto old=SelectObject(dc,pen);Polyline(dc,arrow,3);SelectObject(dc,old);DeleteObject(pen);
    }
    static LRESULT CALLBACK chromeControl(HWND h,UINT message,WPARAM w,LPARAM l,UINT_PTR,DWORD_PTR reference) {
        auto* app=reinterpret_cast<App*>(reference);
        if(message==WM_MOUSEMOVE) {
            if(app->hovered!=h) {const auto old=app->hovered;app->hovered=h;if(old) InvalidateRect(old,nullptr,FALSE);InvalidateRect(h,nullptr,FALSE);}
            TRACKMOUSEEVENT track{sizeof(track),TME_LEAVE,h,0};TrackMouseEvent(&track);
        }
        if(message==WM_MOUSELEAVE) {if(app->hovered==h) app->hovered=nullptr;InvalidateRect(h,nullptr,FALSE);}
        if(message==WM_SETFOCUS||message==WM_KILLFOCUS||message==WM_ENABLE) InvalidateRect(h,nullptr,FALSE);
        const int id=GetDlgCtrlID(h);
        const bool chrome=id==Output||id==Input||id==Device||id==Preset||id==Raw||(id>=Slider&&id<Slider+5)||(id>=SetStart&&id<=SetTheme);
        if(chrome&&message==WM_PAINT) {
            // Draw into an off-screen bitmap first: repaints are frequent now
            // and painting straight to the window would flicker during drags.
            PAINTSTRUCT p;auto dc=BeginPaint(h,&p);RECT cr;GetClientRect(h,&cr);
            if(auto mem=CreateCompatibleDC(dc)) {
                if(auto bmp=CreateCompatibleBitmap(dc,cr.right,cr.bottom)) {
                    auto old=SelectObject(mem,bmp);app->paintChrome(h,mem);
                    BitBlt(dc,0,0,cr.right,cr.bottom,mem,0,0,SRCCOPY);
                    SelectObject(mem,old);DeleteObject(bmp);
                } else app->paintChrome(h,dc);
                DeleteDC(mem);
            } else app->paintChrome(h,dc);
            EndPaint(h,&p);return 0;
        }
        if(chrome&&(message==WM_PRINT||message==WM_PRINTCLIENT)) {app->paintChrome(h,reinterpret_cast<HDC>(w));return 0;}
        if(chrome&&message==WM_ERASEBKGND) return 1;
        if(chrome&&message==WM_NCPAINT) return 0;
        if(message==WM_NCDESTROY) {if(app->hovered==h) app->hovered=nullptr;RemoveWindowSubclass(h,chromeControl,1);}
        // Native handlers can draw outside WM_PAINT. Mark only messages known
        // to change appearance dirty; broad filtering causes a repaint loop.
        const bool cosmetic=chrome&&(message==BM_SETCHECK||message==BM_SETSTATE||
            message==WM_SETFOCUS||message==WM_KILLFOCUS||message==WM_UPDATEUISTATE||
            message==WM_ENABLE||message==WM_LBUTTONDOWN||message==WM_LBUTTONUP||
            message==WM_KEYDOWN||message==WM_KEYUP||message==CB_SETCURSEL||
            message==BM_CLICK||message==WM_MOUSEMOVE||message==WM_MOUSELEAVE||
            message==WM_CAPTURECHANGED||message==TBM_SETPOS||message==TBM_SETRANGE||
            message==TBM_SETRANGEMIN||message==TBM_SETRANGEMAX||message==TBM_SETSEL||
            message==TBM_CLEARSEL||message==WM_TIMER||message==WM_LBUTTONDBLCLK||
            message==WM_MOUSEWHEEL||message==WM_SIZE||message==WM_CANCELMODE||
            // Added after classic thumbs still showed: these also make the
            // native trackbar draw. A broad "everything except queries" filter
            // was tried and rejected: messages the UI polls each tick kept
            // queueing repaints and the self-test stopped finishing.
            message==WM_SETTEXT||message==WM_STYLECHANGED||message==WM_THEMECHANGED||
            message==WM_SYSCOLORCHANGE||message==WM_SETTINGCHANGE||message==WM_DPICHANGED||
            message==WM_WINDOWPOSCHANGED||message==WM_SHOWWINDOW||message==WM_MOVE||
            message==WM_RBUTTONDOWN||message==WM_RBUTTONUP||message==WM_SETREDRAW||
            message==TBM_SETTICFREQ||message==TBM_SETPAGESIZE||message==TBM_SETLINESIZE||
            message==TBM_SETTHUMBLENGTH||message==TBM_SETPOSNOTIFY||message==WM_SYSKEYDOWN||
            message==WM_SYSKEYUP||message==WM_CHAR||message==WM_HSCROLL||message==WM_VSCROLL);
        const auto result=DefSubclassProc(h,message,w,l);
        // Invalidate only: forcing the paint here would run the native handler
        // again from inside this call and repeat the whole cycle. The queued
        // WM_PAINT coalesces, so a burst of messages still costs one redraw.
        if(cosmetic&&IsWindow(h)) InvalidateRect(h,nullptr,FALSE);
        return result;
    }
    HWND control(const wchar_t* cls,const wchar_t* caption,DWORD style,int id) {
        auto h=CreateWindowExW(0,cls,caption,WS_CHILD|WS_VISIBLE|style,0,0,0,0,window,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
        if(!h) throw std::runtime_error("No se pudo crear un control");SendMessageW(h,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
        // Trackbars have no owner-draw style: the themed handler always paints
        // its own classic thumb, which leaked over the Fluent slider. Removing
        // the visual theme stops that drawing at the source instead of
        // repainting over it afterwards; keyboard and mouse logic are intact.
        if(id>=Slider&&id<Slider+5) SetWindowTheme(h,L" ",L" ");
        // The dropped-down list is a window of its own; without stripping its
        // theme Windows frames it with a light border that we never draw.
        if(!lstrcmpiW(cls,WC_COMBOBOXW)) {
            COMBOBOXINFO info{sizeof(info)};
            if(GetComboBoxInfo(h,&info)&&info.hwndList) SetWindowTheme(info.hwndList,L" ",L" ");
        }
        if(!SetWindowSubclass(h,chromeControl,1,reinterpret_cast<DWORD_PTR>(this))) throw std::runtime_error("No se pudo preparar un control");return h;
    }
    void refresh(bool preserveSelection=true) {
        std::wstring previous;const auto selection=static_cast<int>(SendMessageW(output,CB_GETCURSEL,0,0));
        if(preserveSelection&&selection>=0&&static_cast<std::size_t>(selection)<physical.size()) previous=physical[selection].id;
        // Enumeration may fail; keep the existing list until it succeeds.
        const auto available=dps::endpoints();
        physical.clear();virtualInputs.clear();virtualId.clear();routed=false;SendMessageW(output,CB_RESETCONTENT,0,0);SendMessageW(input,CB_RESETCONTENT,0,0);
        for(const auto& d:available) if(!d.capture) {
            if(dps::transportKind(d)!=dps::TransportKind::None||(!prefs.virtualInputId.empty()&&d.id==wide(prefs.virtualInputId))) {virtualInputs.push_back(d);SendMessageW(input,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(d.name.c_str()));}
            else {physical.push_back(d);SendMessageW(output,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(d.name.c_str()));}
        }
        const int transport=dps::selectTransport(virtualInputs,wide(prefs.virtualInputId));
        SendMessageW(input,CB_SETCURSEL,transport,0);
        if(transport>=0) {virtualId=virtualInputs[transport].id;routed=virtualInputs[transport].defaultRender;}
        const int choice=dps::selectOutput(physical,previous.empty()?wide(profile.outputId):previous);
        SendMessageW(output,CB_SETCURSEL,choice,0);
        // The enumeration already read every endpoint's volume, so the new
        // selection starts from a real figure instead of a blank one.
        outputLevel=choice>=0&&static_cast<std::size_t>(choice)<physical.size()
            ?dps::EndpointLevel{physical[choice].volume,physical[choice].muted}:dps::EndpointLevel{};
    }
    // The physical level changes outside this application and nothing here
    // matches volumes, so the window re-reads the selected endpoint instead of
    // showing what the last enumeration happened to see.
    void readOutputLevel() {
        const auto index=static_cast<int>(SendMessageW(output,CB_GETCURSEL,0,0));
        if(index<0||static_cast<std::size_t>(index)>=physical.size()) {outputLevel={};return;}
        try {outputLevel=dps::endpointLevel(physical[index].id);}
        catch(const std::exception& error) {outputLevel={};if(telemetry) telemetry<<"output_level_error="<<error.what()<<'\n';}
    }
    void sync() {
        const auto& p=profile.parameters;const std::array<float,5> values{p.bass,p.clarity,p.dynamics,p.spatial,p.intensity};
        for(std::size_t i=0;i<5;++i) SendMessageW(sliders[i],TBM_SETPOS,TRUE,static_cast<LPARAM>(std::lround(values[i]*100)));
        int d=7;for(int i=0;i<7;++i) if(profile.deviceName==devices[i]) d=i;SendMessageW(device,CB_SETCURSEL,d,0);
        for(int i=0;i<4;++i) if(profile.presetName==presets[i]||wide(profile.presetName)==presetLabels[i]) SendMessageW(preset,CB_SETCURSEL,i,0);
        SendMessageW(raw,BM_SETCHECK,profile.raw?BST_CHECKED:BST_UNCHECKED,0);
        session.state.enabled=p.enabled;publish();buttons();
    }
    // UI meter only; no gain, limiter or processing is added to the audio path.
    float displayGainDb() const {
        const auto in=session.state.inputPeak.load(),out=session.state.outputPeak.load();
        return in>1e-4f&&out>1e-4f?20*std::log10(out/in):0.f;
    }
    void publish() {
        auto& p=profile.parameters;
        p.bass=static_cast<float>(SendMessageW(sliders[0],TBM_GETPOS,0,0))/100;
        p.clarity=static_cast<float>(SendMessageW(sliders[1],TBM_GETPOS,0,0))/100;
        p.dynamics=static_cast<float>(SendMessageW(sliders[2],TBM_GETPOS,0,0))/100;
        p.spatial=static_cast<float>(SendMessageW(sliders[3],TBM_GETPOS,0,0))/100;
        p.intensity=static_cast<float>(SendMessageW(sliders[4],TBM_GETPOS,0,0))/100;
        session.state.bass=p.bass;session.state.clarity=p.clarity;session.state.dynamics=p.dynamics;session.state.spatial=p.spatial;session.state.intensity=p.intensity;
        InvalidateRect(window,nullptr,FALSE);
    }
    void buttons() {
        trayTip();
        const bool active=session.running();
        SetWindowTextW(toggle,!active&&!selfTest?L"Conectar audio":profile.parameters.enabled?L"DSP activado":L"DSP desactivado");
        for(HWND h:{output,device,raw,GetDlgItem(window,Refresh),GetDlgItem(window,Load)}) EnableWindow(h,TRUE);
        InvalidateRect(window,nullptr,FALSE);
    }
    void layout() {
        auto move=[&](HWND h,int x,int y,int w,int height){MoveWindow(h,scale(x),scale(y),scale(w),scale(height),TRUE);};
        move(toggle,688,64,152,118);
        move(output,30,225,405,240);move(input,450,225,255,240);move(GetDlgItem(window,Refresh),720,225,130,36);
        move(device,30,300,240,240);move(preset,290,300,220,240);
        move(GetDlgItem(window,Load),535,300,145,36);move(GetDlgItem(window,Save),695,300,155,36);
        // Window buttons in the top-right corner, where they are expected.
        move(GetDlgItem(window,Minimise),768,24,42,30);move(GetDlgItem(window,Close),812,24,42,30);
        move(GetDlgItem(window,Reference),30,355,170,36);move(GetDlgItem(window,SetDefault),208,355,116,36);move(GetDlgItem(window,Detect),332,355,114,36);move(GetDlgItem(window,Measure),454,355,106,36);move(GetDlgItem(window,Route),568,355,170,36);move(GetDlgItem(window,Settings),746,355,94,36);
        move(sliders[4],260,435,495,32);
        for(int i=0;i<4;++i) move(sliders[i],260,495+i*55,495,32);
        move(raw,30,806,420,28);
        move(GetDlgItem(window,About),720,877,130,30);
    }
    void init() {
        font=CreateFontW(-scale(16),0,0,0,FW_NORMAL,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI Variable Text");
        titleFont=CreateFontW(-scale(40),0,0,0,FW_BOLD,FALSE,FALSE,FALSE,DEFAULT_CHARSET,0,0,CLEARTYPE_QUALITY,0,L"Segoe UI");
        output=control(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_TABSTOP|WS_VSCROLL,Output);
        input=control(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_TABSTOP|WS_VSCROLL,Input);
        device=control(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_TABSTOP,Device);
        preset=control(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_TABSTOP,Preset);
        for(const auto* label:{L"Portátil",L"Auriculares",L"Altavoces externos",L"Portátil: cuerpo y detalle",L"Portátil: impacto",L"Portátil: impacto máximo",L"Medido en este equipo",L"Personalizado"}) SendMessageW(device,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
        for(const auto* label:presetLabels) SendMessageW(preset,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
        for(const auto& pair:std::array<std::pair<int,const wchar_t*>,4>{{{Toggle,L"DSP activado"},{Load,L"Cargar perfil"},{Save,L"Guardar perfil"},{Refresh,L"Actualizar"}}}) {
            const auto h=control(WC_BUTTONW,pair.second,BS_OWNERDRAW|WS_TABSTOP,pair.first);if(pair.first==Toggle) toggle=h;
        }
        control(WC_BUTTONW,L"Ajuste por defecto",BS_OWNERDRAW|WS_TABSTOP,Reference);control(WC_BUTTONW,L"Fijar actual",BS_OWNERDRAW|WS_TABSTOP,SetDefault);control(WC_BUTTONW,L"Detectar",BS_OWNERDRAW|WS_TABSTOP,Detect);control(WC_BUTTONW,L"Medir",BS_OWNERDRAW|WS_TABSTOP,Measure);control(WC_BUTTONW,L"Ajustes",BS_OWNERDRAW|WS_TABSTOP,Settings);
        control(WC_BUTTONW,L"Acerca de",BS_OWNERDRAW|WS_TABSTOP,About);
        // Our own window buttons, since the system title bar is gone.
        control(WC_BUTTONW,L"",BS_OWNERDRAW,Minimise);control(WC_BUTTONW,L"",BS_OWNERDRAW,Close);control(WC_BUTTONW,L"Sonido Windows",BS_OWNERDRAW|WS_TABSTOP,Route);
        raw=control(WC_BUTTONW,L"Omitir efectos adicionales de Windows",BS_AUTOCHECKBOX|WS_TABSTOP,Raw);
        for(int i=0;i<5;++i) {sliders[i]=control(TRACKBAR_CLASSW,L"",TBS_HORZ|TBS_NOTICKS|WS_TABSTOP,Slider+i);SendMessageW(sliders[i],TBM_SETRANGE,TRUE,MAKELPARAM(0,100));SendMessageW(sliders[i],TBM_SETPAGESIZE,0,10);}
        wchar_t path[32768];const DWORD n=GetModuleFileNameW(nullptr,path,32768);if(!n||n==32768) throw std::runtime_error("No se pudo localizar la aplicacion");
        // Support nested build directories and a future installed layout.
        root=std::filesystem::path(path).parent_path();
        // A requested data directory wins wherever the application runs from.
        // It used to be honoured only in the installed layout, so a build run
        // for a test silently read and wrote the real profiles instead.
        if(std::filesystem::exists(root/L"installed.marker")||!dataRootOverride.empty()) {
            const auto assets=root;
            if(dataRootOverride.empty()) {
                PWSTR local=nullptr;
                if(FAILED(SHGetKnownFolderPath(FOLDERID_LocalAppData,0,nullptr,&local)))
                    throw std::runtime_error("No se pudo localizar la carpeta de usuario");
                root=std::filesystem::path(local)/L"RESONIQ";CoTaskMemFree(local);
            }else root=std::filesystem::absolute(dataRootOverride);
            std::filesystem::create_directories(root/L"config");
            std::filesystem::create_directories(root/L"profiles");
            if(std::filesystem::is_directory(assets/L"profiles"))
                for(const auto& entry:std::filesystem::directory_iterator(assets/L"profiles"))
                    if(entry.is_regular_file()&&entry.path().extension()==L".dps")
                        std::filesystem::copy_file(entry.path(),root/L"profiles"/entry.path().filename(),
                                                   std::filesystem::copy_options::skip_existing);
        }
        for(auto candidate=root;!candidate.empty();) {
            if(std::filesystem::is_directory(candidate/L"profiles")&&std::filesystem::is_directory(candidate/L"config")) {
                root=candidate;break;
            }
            const auto parent=candidate.parent_path();if(parent==candidate) break;candidate=parent;
        }
        profile.name="RESONIQ Enhance";profile.presetName="RESONIQ Enhance";profile.parameters=dps::preset("RESONIQ Enhance");
        if(!requestedProfile.empty()) {profilePath=requestedProfile;profile=dps::loadProfile(profilePath);}
        else for(const auto* candidate:{L"config/session.user.dps",L"config/current.user.dps",L"config/lenovo.user.dps",L"config/favorite.user.dps"}) if(std::filesystem::exists(root/candidate)) {
            try {auto loaded=dps::loadProfile(root/candidate);profile=loaded;profilePath=root/candidate;break;}
            catch(const std::exception&) {
                startupWarning=L"Se recupero un perfil de respaldo; el archivo invalido se conserva como copia.";
                // Preserve a recovery copy before a later session save.
                if(!selfTest&&!audioTest) {
                    auto backup=root/candidate;backup+=L".invalid-"+std::to_wstring(GetTickCount64());
                    std::filesystem::copy_file(root/candidate,backup);
                }
            }
        }
        // The appearance applies in every mode, so a test run shows what the
        // user would see. The behaviour preferences stay out of the tests.
        loadPreferences();applyTheme();
        if(!selfTest&&!audioTest) {
            reconcileStartup();
            prefs.startWithWindows=prefs.startWithWindows||autoStartEnabled();
            if(prefs.dspAtStart==1) profile.parameters.enabled=true;
            else if(prefs.dspAtStart==2) profile.parameters.enabled=false;
        }
        // First run: nothing saved yet, so start from what the machine looks
        // like instead of a profile meant for a different laptop.
        if(!selfTest&&!audioTest&&profilePath.empty()&&requestedProfile.empty()) firstRun=true;
        refresh();sync();layout();
        // The log is opened before the first run decides anything, so what a
        // new installation started from is on record from the first line.
        std::filesystem::create_directories(root/L"build");
        if(!selfTest&&!audioTest) telemetry.open(root/L"build/gui-telemetry.log");
        if(telemetry&&!startupWarning.empty()) telemetry<<"startup_recovery="<<utf8(startupWarning)<<'\n';
        if(firstRun) applyDetected(true);
        SetTimer(window,1,100,nullptr);
        if(selfTest) SetTimer(window,2,400,nullptr);
        if(audioTest) {SetTimer(window,4,200,nullptr);SetTimer(window,3,1600,nullptr);}
        else if(!selfTest) SetTimer(window,4,200,nullptr);
    }
    void ensureAudio() {
        if(powerSuspended) return;
        if(session.running()&&session.finished.load(std::memory_order_acquire)) session.stop();
        if(session.running()) return;
        const auto index=static_cast<int>(SendMessageW(output,CB_GETCURSEL,0,0));
        if(index<0||static_cast<std::size_t>(index)>=physical.size()) throw std::runtime_error("La salida guardada no esta disponible. Conectala o selecciona otra salida.");
        if(virtualId.empty()) throw std::runtime_error("Selecciona una entrada virtual. Si falta VB-CABLE, instala su paquete oficial y reinicia Windows.");
        publish();profile.outputId=utf8(physical[index].id);profile.raw=SendMessageW(raw,BM_GETCHECK,0,0)==BST_CHECKED;
        dps::LiveOptions options;options.inputId=virtualId;options.outputId=physical[index].id;options.loopback=true;options.raw=profile.raw;options.parameters=profile.parameters;
        session.start(options);status=routed?L"Ruta virtual activa > RESONIQ > "+physical[index].name:L"Selecciona la entrada virtual elegida como salida de Windows para procesar.";buttons();
        if(!startupWarning.empty()) {status=startupWarning;startupWarning.clear();}
    }
    void saveTo(const std::filesystem::path& destination) {
        publish();auto candidate=profile;candidate.raw=SendMessageW(raw,BM_GETCHECK,0,0)==BST_CHECKED;
        const auto index=static_cast<int>(SendMessageW(output,CB_GETCURSEL,0,0));
        if(index>=0&&static_cast<std::size_t>(index)<physical.size()) candidate.outputId=utf8(physical[index].id);
        candidate.name=utf8(destination.stem().wstring());dps::saveProfile(destination,candidate);
        profile=candidate;profilePath=destination;status=L"Perfil guardado: "+destination.filename().wstring();
    }
    void loadFrom(const std::filesystem::path& destination) {
        auto loaded=dps::loadProfile(destination);const bool active=session.running();
        session.stop();profile=loaded;profilePath=destination;refresh(false);sync();
        if(active) ensureAudio();else status=L"Perfil cargado: "+wide(profile.name);
    }
    void file(bool saving,bool testDialog=false) {
        // lpstrFile starts with a filename, never a relative "config/..."
        // path: common dialogs can reject those forward-slash paths outright.
        std::array<wchar_t,32768> path{};
        if(saving) {
            const auto value=profile.deviceName=="Measured"?std::wstring(L"Medido.dps"):
                profilePath.empty()?std::wstring(L"Mi perfil.dps"):profilePath.filename().wstring();
            std::copy_n(value.c_str(),std::min(value.size(),path.size()-1),path.data());
        }
        const auto initial=(root/L"config").wstring();OPENFILENAMEW dialog{};dialog.lStructSize=sizeof(dialog);dialog.hwndOwner=window;dialog.lpstrFilter=L"Perfiles RESONIQ (*.dps)\0*.dps\0\0";dialog.lpstrFile=path.data();dialog.nMaxFile=static_cast<DWORD>(path.size());dialog.lpstrInitialDir=initial.c_str();dialog.lpstrDefExt=L"dps";dialog.Flags=OFN_NOCHANGEDIR|OFN_PATHMUSTEXIST|(saving?OFN_OVERWRITEPROMPT:OFN_FILEMUSTEXIST);
        if(testDialog) {
            dialog.Flags|=OFN_ENABLEHOOK|OFN_EXPLORER;
            dialog.lpfnHook=[](HWND h,UINT message,WPARAM,LPARAM)->UINT_PTR {
                if(message==WM_INITDIALOG) PostMessageW(GetParent(h),WM_COMMAND,IDCANCEL,0);
                return 0;
            };
        }
        if(!(saving?GetSaveFileNameW(&dialog):GetOpenFileNameW(&dialog))) {
            if(const auto error=CommDlgExtendedError()) {
                if(telemetry) {telemetry<<"file_dialog_error="<<error<<" saving="<<saving<<'\n';telemetry.flush();}
                throw std::runtime_error("No se pudo abrir el selector de archivos (codigo "+std::to_string(error)+")");
            }
            return;
        }
        const std::filesystem::path destination(path.data());
        if(saving) saveTo(destination);else loadFrom(destination);
        InvalidateRect(window,nullptr,FALSE);
    }
    // Tray icon. Audio keeps running while hidden; the window is only a view.
    void trayAdd() {
        if(tray) return;
        NOTIFYICONDATAW data{};data.cbSize=sizeof(data);data.hWnd=window;data.uID=1;
        data.uFlags=NIF_ICON|NIF_MESSAGE|NIF_TIP;data.uCallbackMessage=TrayMessage;
        data.hIcon=static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(2),IMAGE_ICON,
            GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_DEFAULTCOLOR|LR_SHARED));
        if(!data.hIcon) data.hIcon=LoadIconW(nullptr,IDI_APPLICATION);
        wcscpy_s(data.szTip,L"RESONIQ");
        tray=Shell_NotifyIconW(NIM_ADD,&data);
    }
    void trayRemove() {
        if(!tray) return;
        NOTIFYICONDATAW data{};data.cbSize=sizeof(data);data.hWnd=window;data.uID=1;
        Shell_NotifyIconW(NIM_DELETE,&data);tray=false;
    }
    void trayTip() {
        if(!tray) return;
        NOTIFYICONDATAW data{};data.cbSize=sizeof(data);data.hWnd=window;data.uID=1;data.uFlags=NIF_TIP|NIF_ICON;
        const int icon=session.running()?(profile.parameters.enabled?2:3):4;
        data.hIcon=static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(icon),IMAGE_ICON,
            GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_DEFAULTCOLOR|LR_SHARED));
        const std::wstring tip=std::wstring(L"RESONIQ - ")+(session.running()?(profile.parameters.enabled?L"procesado":L"original"):L"sin conectar");
        wcscpy_s(data.szTip,tip.c_str());Shell_NotifyIconW(NIM_MODIFY,&data);
    }
    void showWindow() {
        ShowWindow(window,SW_SHOW);ShowWindow(window,SW_RESTORE);
        SetForegroundWindow(window);InvalidateRect(window,nullptr,FALSE);
    }
    void trayMenu() {
        POINT cursor;GetCursorPos(&cursor);
        auto menu=CreatePopupMenu();if(!menu) return;
        AppendMenuW(menu,MF_STRING,TrayShow,L"Mostrar RESONIQ");
        AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
        AppendMenuW(menu,MF_STRING,TrayToggle,profile.parameters.enabled?L"Desactivar DSP":L"Activar DSP");
        AppendMenuW(menu,MF_STRING,TrayFavorite,L"Ajuste por defecto");
        AppendMenuW(menu,MF_STRING,TraySetDefault,L"Fijar lo actual como ajuste por defecto");
        AppendMenuW(menu,MF_SEPARATOR,0,nullptr);
        AppendMenuW(menu,MF_STRING,TrayExit,L"Salir");
        // Required so the menu closes when clicking elsewhere.
        SetForegroundWindow(window);
        TrackPopupMenu(menu,TPM_RIGHTBUTTON,cursor.x,cursor.y,0,window,nullptr);
        PostMessageW(window,WM_NULL,0,0);DestroyMenu(menu);
    }
    void setAsDefault() {
        publish();profile.raw=SendMessageW(raw,BM_GETCHECK,0,0)==BST_CHECKED;
        const auto index=static_cast<int>(SendMessageW(output,CB_GETCURSEL,0,0));
        if(index>=0&&static_cast<std::size_t>(index)<physical.size()) profile.outputId=utf8(physical[index].id);
        const auto reference=root/L"config/favorite.user.dps";
        if(MessageBoxW(window,L"¿Fijar el sonido actual como ajuste por defecto?\n\nEl anterior se guardará como copia.",
            L"RESONIQ",MB_OKCANCEL|MB_ICONQUESTION)!=IDOK) return;
        // Never overwrite the previous default without leaving a copy behind.
        if(std::filesystem::exists(reference)) {
            std::error_code ec;
            std::filesystem::copy_file(reference,root/L"config/favorite-previous.user.dps",
                std::filesystem::copy_options::overwrite_existing,ec);
            if(ec) throw std::runtime_error("No se pudo conservar el ajuste anterior");
        }
        dps::saveProfile(reference,profile);
        status=L"Ajuste por defecto actualizado.";InvalidateRect(window,nullptr,FALSE);
    }
    // Settings window. Built from plain controls so it follows the system
    // dialog conventions (Tab order, Esc, Enter) without extra drawing code.
    static LRESULT CALLBACK settingsProc(HWND h,UINT message,WPARAM w,LPARAM l) {
        auto* app=reinterpret_cast<App*>(GetWindowLongPtrW(h,GWLP_USERDATA));
        switch(message) {
        case WM_CREATE:{
            auto* create=reinterpret_cast<CREATESTRUCTW*>(l);
            SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(create->lpCreateParams));
            return 0;}
        case WM_NCHITTEST:{
            POINT pt{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(h,&pt);
            if(pt.y>=0&&pt.y<(app?app->scale(50):50)) return HTCAPTION;
            return HTCLIENT;}
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:{
            if(!app) break;
            PAINTSTRUCT p;HDC dc=BeginPaint(h,&p);RECT r;GetClientRect(h,&r);
            HDC mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,r.right,r.bottom);
            auto old=SelectObject(mem,bmp);
            app->paintSettings(mem,r);
            BitBlt(dc,0,0,r.right,r.bottom,mem,0,0,SRCCOPY);
            SelectObject(mem,old);DeleteObject(bmp);DeleteDC(mem);EndPaint(h,&p);return 0;}
        case WM_MEASUREITEM:{
            if(!app) break;
            auto* item=reinterpret_cast<MEASUREITEMSTRUCT*>(l);
            item->itemHeight=static_cast<UINT>(app->scale(26));return TRUE;}
        case WM_DRAWITEM:{
            if(!app) break;
            auto* item=reinterpret_cast<DRAWITEMSTRUCT*>(l);
            // Without this the dropped-down list is drawn by Windows: centred
            // text, white edges and a focus box that do not match the rest.
            if(item->CtlType==ODT_COMBOBOX) {
                const bool selected=(item->itemState&ODS_SELECTED)!=0;
                auto b=CreateSolidBrush(selected?theme.surfaceHot:theme.card);
                FillRect(item->hDC,&item->rcItem,b);DeleteObject(b);
                if(item->itemID!=static_cast<UINT>(-1)) {
                    const auto count=SendMessageW(item->hwndItem,CB_GETLBTEXTLEN,item->itemID,0);
                    if(count>=0) {
                        std::wstring value(static_cast<std::size_t>(count)+1,0);
                        SendMessageW(item->hwndItem,CB_GETLBTEXT,item->itemID,reinterpret_cast<LPARAM>(value.data()));
                        SetBkMode(item->hDC,TRANSPARENT);SetTextColor(item->hDC,theme.text);
                        SelectObject(item->hDC,app->font);
                        RECT r=item->rcItem;r.left+=app->scale(13);
                        DrawTextW(item->hDC,value.c_str(),-1,&r,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);
                    }
                }
                return TRUE;
            }
            if(item->CtlID==SetClose) {
                // Window close button, same look as the main window's.
                const bool over=app->hovered==item->hwndItem;
                const bool down=(item->itemState&ODS_SELECTED)!=0;
                const COLORREF fill=down?RGB(170,30,38):over?RGB(196,43,28):theme.background;
                auto back=CreateSolidBrush(fill);FillRect(item->hDC,&item->rcItem,back);DeleteObject(back);
                const COLORREF ink=(over||down)?RGB(255,255,255):theme.text;
                Gdiplus::Graphics g(item->hDC);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                Gdiplus::Pen pen(Gdiplus::Color(255,GetRValue(ink),GetGValue(ink),GetBValue(ink)),app->scale(1)*1.2f);
                pen.SetStartCap(Gdiplus::LineCapRound);pen.SetEndCap(Gdiplus::LineCapRound);
                const float cx=(item->rcItem.left+item->rcItem.right)/2.f;
                const float cy=(item->rcItem.top+item->rcItem.bottom)/2.f;
                const float arm=app->scale(5)*1.f;
                g.DrawLine(&pen,cx-arm,cy-arm,cx+arm,cy+arm);
                g.DrawLine(&pen,cx-arm,cy+arm,cx+arm,cy-arm);
                return TRUE;
            }
            const bool primary=item->CtlID==SetOk;
            const bool pressed=(item->itemState&ODS_SELECTED)!=0,focus=(item->itemState&ODS_FOCUS)!=0;
            RECT box=item->rcItem;
            // Clear the whole item first: the rounded shape leaves its corners
            // untouched, and whatever was there would show through.
            auto back=CreateSolidBrush(theme.background);FillRect(item->hDC,&box,back);DeleteObject(back);
            rounded(item->hDC,box,primary?(pressed?theme.accentDown:theme.accent):(pressed?theme.surfaceHot:theme.surface),
                focus?theme.text:(primary?theme.accent:theme.border),app->scale(7));
            SetBkMode(item->hDC,TRANSPARENT);SelectObject(item->hDC,app->font);
            SetTextColor(item->hDC,primary?theme.onAccent:theme.text);
            wchar_t caption[64];GetWindowTextW(item->hwndItem,caption,64);
            DrawTextW(item->hDC,caption,-1,&box,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
            return TRUE;}
        case WM_COMMAND:{
            const int id=LOWORD(w);
            if(id==SetOk) {
                if(!app) return 0;
                auto p=app->prefs;
                p.startWithWindows=IsDlgButtonChecked(h,SetStart)==BST_CHECKED;
                p.startMinimised=IsDlgButtonChecked(h,SetMinimised)==BST_CHECKED;
                p.closeExits=IsDlgButtonChecked(h,SetCloseExits)==BST_CHECKED;
                p.autoReconnect=IsDlgButtonChecked(h,SetReconnect)==BST_CHECKED;
                const auto mode=SendMessageW(GetDlgItem(h,SetDspMode),CB_GETCURSEL,0,0);
                p.dspAtStart=mode>=0?static_cast<int>(mode):0;
                const auto themeChoice=SendMessageW(GetDlgItem(h,SetTheme),CB_GETCURSEL,0,0);
                p.themeMode=themeChoice>=0?static_cast<int>(themeChoice):0;
                const bool previousAutoStart=app->autoStartEnabled();
                try {
                    app->applyAutoStart(p.startWithWindows);
                    try {dps::savePreferences(app->root/L"config/settings.user.txt",p);}
                    catch(...) {app->applyAutoStart(previousAutoStart);throw;}
                    app->prefs=p;app->applyTheme();app->status=L"Ajustes guardados.";
                }
                catch(const std::exception& e) {MessageBoxW(h,wide(e.what()).c_str(),L"RESONIQ",MB_OK|MB_ICONWARNING);return 0;}
                InvalidateRect(app->window,nullptr,FALSE);DestroyWindow(h);return 0;
            }
            if(id==SetCancel||id==SetClose||id==IDCANCEL) {DestroyWindow(h);return 0;}
            return 0;}
        case WM_CLOSE:DestroyWindow(h);return 0;
        case WM_DESTROY:if(app) app->settings=nullptr;return 0;
        }
        return DefWindowProcW(h,message,w,l);
    }
    void paintSettings(HDC dc,RECT r) {
        auto base=CreateSolidBrush(theme.background);FillRect(dc,&r,base);DeleteObject(base);
        // Our own title, since the window has no system caption.
        SetBkMode(dc,TRANSPARENT);SelectObject(dc,font);SetTextColor(dc,theme.text);
        RECT title{scale(24),scale(14),r.right-scale(60),scale(44)};
        DrawTextW(dc,L"Ajustes de RESONIQ",-1,&title,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
        RECT panel{scale(14),scale(52),r.right-scale(14),scale(296)};
        rounded(dc,panel,theme.card,theme.panelEdge,scale(12));
        SetTextColor(dc,theme.mutedText);
        RECT caption{scale(30),scale(214),scale(205),scale(240)};
        DrawTextW(dc,L"Al arrancar, el DSP:",-1,&caption,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
        RECT themeCaption{scale(30),scale(258),scale(205),scale(284)};
        DrawTextW(dc,L"Apariencia:",-1,&themeCaption,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
    }
    void openSettings() {
        if(settings) {SetForegroundWindow(settings);return;}
        static bool registered=false;
        if(!registered) {
            WNDCLASSW wc{};wc.lpfnWndProc=settingsProc;wc.hInstance=GetModuleHandleW(nullptr);
            wc.lpszClassName=L"Resoniq.Settings";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
            // Without an explicit brush Windows paints the class background
            // white, which showed through the rounded corners of the buttons.
            wc.hbrBackground=CreateSolidBrush(theme.background);
            if(!RegisterClassW(&wc)) throw std::runtime_error("No se pudo preparar los ajustes");
            registered=true;
        }
        RECT owner;GetWindowRect(window,&owner);
        // Frameless like the main window: own title text and close button.
        const int width=scale(520),height=scale(388);
        settings=CreateWindowExW(0,L"Resoniq.Settings",L"Ajustes de RESONIQ",
            WS_POPUP|WS_CLIPCHILDREN,
            owner.left+((owner.right-owner.left)-width)/2,owner.top+scale(140),width,height,
            window,nullptr,GetModuleHandleW(nullptr),this);
        if(!settings) throw std::runtime_error("No se pudo abrir los ajustes");
        // Same dark frame and rounded corners as the main window.
        BOOL dark=TRUE;DwmSetWindowAttribute(settings,DWMWA_USE_IMMERSIVE_DARK_MODE,&dark,sizeof(dark));
        const DWM_WINDOW_CORNER_PREFERENCE corners=DWMWCP_ROUND;
        DwmSetWindowAttribute(settings,DWMWA_WINDOW_CORNER_PREFERENCE,&corners,sizeof(corners));
        auto add=[&](const wchar_t* cls,const wchar_t* caption,DWORD style,int x,int y,int cx,int cy,int id) {
            auto c=CreateWindowExW(0,cls,caption,WS_CHILD|WS_VISIBLE|style,scale(x),scale(y),scale(cx),scale(cy),
                settings,reinterpret_cast<HMENU>(static_cast<INT_PTR>(id)),GetModuleHandleW(nullptr),nullptr);
            if(!c) throw std::runtime_error("No se pudo crear un ajuste");
            SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
            SetWindowSubclass(c,chromeControl,1,reinterpret_cast<DWORD_PTR>(this));
            return c;
        };
        add(WC_BUTTONW,L"Iniciar RESONIQ con Windows",BS_AUTOCHECKBOX|WS_TABSTOP,30,70,460,30,SetStart);
        add(WC_BUTTONW,L"Arrancar minimizado en la bandeja",BS_AUTOCHECKBOX|WS_TABSTOP,30,106,460,30,SetMinimised);
        add(WC_BUTTONW,L"Al cerrar la ventana, salir del todo",BS_AUTOCHECKBOX|WS_TABSTOP,30,142,460,30,SetCloseExits);
        add(WC_BUTTONW,L"Reconectar el audio automaticamente",BS_AUTOCHECKBOX|WS_TABSTOP,30,178,460,30,SetReconnect);
        auto combo=add(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_TABSTOP,
            210,210,280,200,SetDspMode);
        for(const auto* label:{L"como estaba la ultima vez",L"siempre activado",L"siempre desactivado"})
            SendMessageW(combo,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
        SendMessageW(combo,CB_SETCURSEL,prefs.dspAtStart,0);
        auto themeBox=add(WC_COMBOBOXW,L"",CBS_DROPDOWNLIST|CBS_OWNERDRAWFIXED|CBS_HASSTRINGS|WS_TABSTOP,
            210,254,280,200,SetTheme);
        for(const auto* label:{L"la del sistema",L"siempre oscura",L"siempre clara"})
            SendMessageW(themeBox,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(label));
        SendMessageW(themeBox,CB_SETCURSEL,prefs.themeMode,0);
        add(WC_BUTTONW,L"",BS_OWNERDRAW,440,12,36,28,SetClose);
        add(WC_BUTTONW,L"Guardar",BS_OWNERDRAW|WS_TABSTOP,270,318,105,38,SetOk);
        add(WC_BUTTONW,L"Cancelar",BS_OWNERDRAW|WS_TABSTOP,385,318,105,38,SetCancel);
        // The saved request comes first: when Windows has dropped the
        // registration the box must still show what the user asked for, or
        // saving any other setting would silently cancel the auto-start.
        CheckDlgButton(settings,SetStart,prefs.startWithWindows||autoStartEnabled()?BST_CHECKED:BST_UNCHECKED);
        CheckDlgButton(settings,SetMinimised,prefs.startMinimised?BST_CHECKED:BST_UNCHECKED);
        CheckDlgButton(settings,SetCloseExits,prefs.closeExits?BST_CHECKED:BST_UNCHECKED);
        CheckDlgButton(settings,SetReconnect,prefs.autoReconnect?BST_CHECKED:BST_UNCHECKED);
        ShowWindow(settings,SW_SHOW);SetForegroundWindow(settings);
    }
    // Swap the palette and repaint everything, including the frame, so the
    // title bar matches the content rather than lagging a theme behind.
    void applyTheme() {
        const bool light=prefs.themeMode==2||(prefs.themeMode==0&&systemPrefersLight());
        theme=light?lightPalette:darkPalette;
        if(auto replacement=CreateSolidBrush(theme.card)) {DeleteObject(cardBrush);cardBrush=replacement;}
        BOOL dark=light?FALSE:TRUE;
        for(HWND frame:{window,settings,about}) if(frame) {
            DwmSetWindowAttribute(frame,DWMWA_USE_IMMERSIVE_DARK_MODE,&dark,sizeof(dark));
            RedrawWindow(frame,nullptr,nullptr,RDW_INVALIDATE|RDW_ALLCHILDREN|RDW_UPDATENOW|RDW_FRAME);
        }
    }
    void loadPreferences() {
        const auto path=root/L"config/settings.user.txt";
        try {prefs=dps::loadPreferences(path);}
        catch(const std::exception&) {
            if(!selfTest&&!audioTest) {
                auto backup=path;backup+=L".invalid-"+std::to_wstring(GetTickCount64());
                std::filesystem::copy_file(path,backup);
            }
            prefs={};startupWarning=L"Preferencias recuperadas; se conservan los archivos invalidos como copia.";
        }
    }
    static std::wstring executablePath() {
        wchar_t path[32768];const DWORD n=GetModuleFileNameW(nullptr,path,32768);
        if(!n||n==32768) throw std::runtime_error("No se pudo localizar la aplicacion");
        return {path,n};
    }
    // What Windows should start: the installed application when there is one,
    // so a build run from the source tree never takes sign-in over.
    static std::wstring startupTarget() {return resoniq::startup::preferredCopy(executablePath());}
    // Registered if either anchor names that copy: a missing Run value no
    // longer means the user asked for no auto-start.
    bool autoStartEnabled() const {
        const auto executable=startupTarget();
        if(resoniq::startup::matches(executable)) return true;
        try {
            return resoniq::startup::sameText(resoniq::startup::registeredShortcut(resoniq::startup::startupFolder()),
                                              resoniq::startup::command(executable));
        }catch(const std::exception&) {return false;}
    }
    void applyAutoStart(bool enable) {
        const auto executable=startupTarget();
        resoniq::startup::apply(enable,executable);
        resoniq::startup::applyShortcut(enable,executable,resoniq::startup::startupFolder());
    }
    // Windows has lost the Run value on at least one machine while the app was
    // closed, which leaves no trace and no way back in by itself. Every start
    // restores what the saved request asks for and writes down what it found,
    // so a registration that keeps disappearing becomes visible.
    void reconcileStartup() {
        using resoniq::startup::Repair;
        const auto describe=[](Repair outcome) {
            switch(outcome) {
            case Repair::Registered:return "restored";
            case Repair::Retargeted:return "retargeted";
            case Repair::Foreign:return "other_copy";
            default:return "ok";
            }
        };
        std::string audit="requested="+std::to_string(prefs.startWithWindows)+" by_windows="+std::to_string(startTray)+" ";
        bool repaired=false,foreign=false;std::wstring failure;
        const auto anchor=[&](const char* name,auto&& reconcile) {
            try {
                const auto outcome=reconcile();
                audit+=std::string(name)+"="+describe(outcome)+" ";
                repaired=repaired||outcome==Repair::Registered||outcome==Repair::Retargeted;
                foreign=foreign||outcome==Repair::Foreign;
            }catch(const std::exception& e) {
                audit+=std::string(name)+"=failed ";
                if(failure.empty()) failure=wide(e.what());
            }
        };
        std::wstring executable;
        try {executable=startupTarget();}
        catch(const std::exception& e) {startupWarning=L"No se pudo asegurar el inicio con Windows: "+wide(e.what());return;}
        anchor("run",[&]{return resoniq::startup::reconcile(prefs.startWithWindows,executable);});
        anchor("shortcut",[&]{return resoniq::startup::reconcileShortcut(prefs.startWithWindows,executable,
                                                                        resoniq::startup::startupFolder());});
        if(!failure.empty()) {startupWarning=L"No se pudo asegurar el inicio con Windows: "+failure;audit+="detail="+utf8(failure);}
        else if(repaired) startupWarning=L"Se ha reparado el inicio con Windows.";
        else if(foreign) startupWarning=L"El inicio con Windows apunta a otra copia de RESONIQ.";
        recordStartupAudit(audit);
    }
    void recordStartupAudit(const std::string& detail) {
        try {
            SYSTEMTIME now{};GetLocalTime(&now);
            char stamp[32]{};
            std::snprintf(stamp,sizeof(stamp),"%04u-%02u-%02u %02u:%02u:%02u ",static_cast<unsigned>(now.wYear),
                static_cast<unsigned>(now.wMonth),static_cast<unsigned>(now.wDay),static_cast<unsigned>(now.wHour),
                static_cast<unsigned>(now.wMinute),static_cast<unsigned>(now.wSecond));
            std::filesystem::create_directories(root/L"build");
            std::ofstream log(root/L"build/startup-audit.log",std::ios::app);
            if(log) log<<stamp<<detail<<'\n';
        }catch(const std::exception&) {}
    }
    // Pick a calibration from what the machine reports. Only touches the
    // device calibration: the listening controls stay as the user left them.
    dps::Suggestion detectCalibration() const {
        std::wstring endpointName;
        const auto index=static_cast<int>(SendMessageW(output,CB_GETCURSEL,0,0));
        if(index>=0&&static_cast<std::size_t>(index)<physical.size()) endpointName=physical[index].name;
        return dps::suggestCalibration(dps::detectHardware(endpointName));
    }
    bool portableMeasurement() const {
        if(profile.deviceName=="Headphones"||profile.deviceName=="External") return false;
        if(profile.deviceName.rfind("Laptop",0)==0) return true;
        const auto index=static_cast<int>(SendMessageW(output,CB_GETCURSEL,0,0));
        const auto hardware=dps::detectHardware(index>=0&&static_cast<std::size_t>(index)<physical.size()?physical[index].name:L"");
        return hardware.portable&&!hardware.headphones;
    }
    void applyDetected(bool announce) {
        const auto suggestion=detectCalibration();
        profile.deviceName=suggestion.deviceName;
        profile.parameters.device=suggestion.calibration;
        if(announce) status=wide(suggestion.reason);
        sync();
        if(session.running()) {session.stop();ensureAudio();}
        InvalidateRect(window,nullptr,FALSE);
    }
    // Acoustic measurement. Nothing here touches the microphone until the user
    // has read what it does and agreed to it, and nothing is applied to the
    // sound until they have seen the result and chosen to apply it.
    void runMeasurement() {
        std::wstring micId,micName;bool micMuted=false;float micVolume=-1;
        for(const auto& endpoint:dps::captureEndpoints()) {
            // Prefer a real microphone over a loopback or virtual input.
            const std::wstring name=endpoint.name;
            if(name.find(L"DPS")!=std::wstring::npos) continue;
            if(micId.empty()||name.find(L"icr")!=std::wstring::npos) {
                micId=endpoint.id;micName=name;micVolume=endpoint.volume;
                micMuted=endpoint.muted;
            }
        }
        if(micId.empty()) {
            MessageBoxW(window,L"No se encontro ningun microfono con el que medir.",
                L"RESONIQ",MB_OK|MB_ICONINFORMATION);
            return;
        }
        const auto index=static_cast<int>(SendMessageW(output,CB_GETCURSEL,0,0));
        if(index<0||static_cast<std::size_t>(index)>=physical.size()) {
            MessageBoxW(window,L"Selecciona primero la salida de audio que quieres medir.",
                L"RESONIQ",MB_OK|MB_ICONINFORMATION);
            return;
        }
        // A muted microphone records digital silence, which would come back as
        // "too much background noise" and send the user chasing the wrong
        // problem. Say what is actually wrong instead.
        if(micMuted) {
            MessageBoxW(window,
                (L"El microfono esta silenciado en Windows:\n\n    "+micName+
                 L"\n\nQuita el silencio y vuelve a intentarlo. En muchos portatiles"
                 L" hay una tecla de funcion que silencia el microfono.").c_str(),
                L"RESONIQ - no se puede medir",MB_OK|MB_ICONWARNING);
            status=L"Microfono silenciado: quita el silencio para poder medir.";
            InvalidateRect(window,nullptr,FALSE);return;
        }
        if(micVolume>=0&&micVolume<0.2f) {
            MessageBoxW(window,
                (L"El volumen del microfono esta muy bajo:\n\n    "+micName+
                 L"\n\nSubelo antes de medir, o la medicion no servira.").c_str(),
                L"RESONIQ - no se puede medir",MB_OK|MB_ICONWARNING);
            status=L"Volumen del microfono demasiado bajo para medir.";
            InvalidateRect(window,nullptr,FALSE);return;
        }
        const std::wstring outputIdToMeasure=physical[index].id;

        std::wstring consent=
            L"RESONIQ va a medir como suenan tus altavoces.\n"
            L"\n"
            L"Para hacerlo necesita:\n"
            L"  - usar el microfono (" + micName + L")\n"
            L"  - reproducir una serie de tonos durante unos 5 segundos\n"
            L"\n"
            L"La grabacion se analiza en este equipo, no se envia a ningun sitio.\n";
        consent+=measurementDiagnostics
            ? L"Modo diagnostico: se guarda la grabacion en build/measure-recorded.wav.\n"
            : L"La grabacion se descarta al terminar.\n";
        consent+=L"\n"
            L"Necesita silencio en la habitacion. Puede sonar alto.\n"
            L"\n"
            L"Quieres medir ahora?";
        if(MessageBoxW(window,consent.c_str(),L"Medir los altavoces - permiso",
            MB_OKCANCEL|MB_ICONQUESTION|MB_DEFBUTTON2)!=IDOK) {
            status=L"Medicion cancelada.";InvalidateRect(window,nullptr,FALSE);return;
        }

        // The live route owns the output endpoint, so it has to stand aside.
        const bool wasRunning=session.running();
        if(wasRunning) session.stop();
        status=L"Midiendo... silencio, por favor.";
        InvalidateRect(window,nullptr,FALSE);UpdateWindow(window);

        dps::Measurement measurement;
        try {
            const auto tones=dps::sweepSignal(48000,0.35f);
            const auto captured=dps::captureMeasurement(outputIdToMeasure,micId,tones,profile.raw);
            measurement=dps::analyseSweep(captured.recorded.data(),captured.recorded.size(),
                captured.rate,captured.silence.data(),captured.silence.size(),captured.gaps);
            // Keep what was captured so a rejected measurement can be looked at
            // instead of guessed about.
            try {
                std::ofstream dump(root/L"build/measure-debug.txt");
                auto peak=[](const std::vector<float>& v){float m=0;for(float x:v) m=std::max(m,std::abs(x));return m;};
                dump<<"recorded_frames="<<captured.recorded.size()
                    <<" silence_frames="<<captured.silence.size()
                    <<" recorded_peak="<<peak(captured.recorded)
                    <<" silence_peak="<<peak(captured.silence)
                    <<" signal_db="<<measurement.signalDb
                    <<" noise_db="<<measurement.noiseFloorDb
                    <<" headroom_db="<<(measurement.signalDb-measurement.noiseFloorDb)
                    <<" usable="<<measurement.usable
                    <<" alignment_ms="<<measurement.alignmentMs
                    <<" reliable_bands="<<measurement.reliableBands
                    <<" output_raw="<<profile.raw
                    <<" discontinuities="<<captured.discontinuities
                    <<" missing_frames="<<captured.missingFrames
                    <<" output_id="<<utf8(outputIdToMeasure)
                    <<" mic="<<utf8(micName)<<char(10);
                for(std::size_t band=0;band<dps::measureBands.size();++band)
                    dump<<"band_hz="<<dps::measureBands[band]<<" level_db="<<measurement.levelDb[band]<<'\n';
                for(const auto& gap:captured.gaps)
                    dump<<"gap_begin="<<gap.begin<<" gap_frames="<<gap.frames<<'\n';
                if(measurement.usable) {
                    const auto proposal=dps::calibrationFrom(measurement,true);
                    dump<<"highpass_hz="<<proposal.highpassHz<<" body_db="<<proposal.bodyDb
                        <<" presence_db="<<proposal.presenceDb<<'\n';
                }
                if(measurementDiagnostics) {
                    // WAV expects interleaved stereo, while capture is mono.
                    dps::Wav recording{static_cast<std::uint32_t>(captured.rate),{}};
                    recording.samples.reserve(captured.recorded.size()*2);
                    for(float sample:captured.recorded) {
                        recording.samples.push_back(sample);recording.samples.push_back(sample);
                    }
                    dps::writeWav(root/L"build/measure-recorded.wav",recording);
                }
            } catch(...) {}
        } catch(const std::exception& e) {
            try {std::ofstream(root/L"build/measure-debug.txt")<<"capture_error="<<e.what()<<'\n'
                <<"output_raw="<<profile.raw<<" output_id="<<utf8(outputIdToMeasure)<<'\n';}catch(...) {}
            std::wstring restoreError;
            if(wasRunning) try {ensureAudio();}catch(const std::exception& error) {
                restoreError=L"\nNo se pudo reconectar audio: "+wide(error.what());
            }
            status=L"No se pudo medir: "+wide(e.what());
            status+=restoreError;
            InvalidateRect(window,nullptr,FALSE);
            MessageBoxW(window,status.c_str(),L"RESONIQ",MB_OK|MB_ICONWARNING);
            return;
        }
        if(wasRunning) ensureAudio();
        showMeasurement(measurement);
    }
    void showMeasurement(const dps::Measurement& measurement) {
        if(!measurement.usable) {
            std::wstring message=L"La medicion no es fiable.\n\n";
            message+=wide(measurement.warning);
            message+=L"\n\nNo se ha cambiado nada.";
            status=L"Medicion descartada.";InvalidateRect(window,nullptr,FALSE);
            MessageBoxW(window,message.c_str(),L"RESONIQ",MB_OK|MB_ICONWARNING);
            return;
        }
        const auto proposal=dps::calibrationFrom(measurement,true);
        std::wstring message=L"Respuesta medida de tus altavoces:\n\n";
        for(std::size_t b=0;b<dps::measureBands.size();++b) {
            wchar_t line[96];
            const float level=measurement.levelDb[b];
            // A short bar makes the shape readable without a chart.
            const int bars=std::clamp(static_cast<int>(std::lround(14+level/2)),0,24);
            std::wstring bar(static_cast<std::size_t>(bars),L'#');
            swprintf_s(line,L"%6.0f Hz  %-24s %+6.1f dB\n",
                static_cast<double>(dps::measureBands[b]),bar.c_str(),level);
            message+=line;
        }
        wchar_t summary[256];
        swprintf_s(summary,
            L"\nPropuesta: corte %.0f Hz, cuerpo +%.1f dB, presencia +%.1f dB."
            L"\n\n",
            static_cast<double>(proposal.highpassHz),static_cast<double>(proposal.bodyDb),
            static_cast<double>(proposal.presenceDb));
        message+=summary;
        if(!measurement.warning.empty()) message+=wide(measurement.warning)+L"\n\n";
        message+=L"Medido con el microfono del portatil, que no es de medicion: "
              L"es un punto de partida para escuchar, no una calibracion exacta."
              L"\n\nAplicar esta calibracion?";
        if(MessageBoxW(window,message.c_str(),L"Resultado de la medicion",
            MB_OKCANCEL|MB_ICONINFORMATION|MB_DEFBUTTON2)!=IDOK) {
            status=L"Medicion descartada; no se cambio nada.";
            InvalidateRect(window,nullptr,FALSE);return;
        }
        // The device name must stay one the profile format accepts, or saving
        // the session afterwards would fail. The measured values ride on top.
        profile.deviceName="Measured";
        profilePath.clear(); // A measurement is a new profile, not the loaded file.
        profile.parameters.device=proposal;
        sync();
        if(session.running()) {session.stop();ensureAudio();}
        status=L"Calibracion medida aplicada. Escuchala antes de fijarla.";
        InvalidateRect(window,nullptr,FALSE);
    }
    void recoverFavorite() {
        publish();
        const auto reference=root/L"config/favorite.user.dps";
        if(!std::filesystem::exists(reference)) throw std::runtime_error("No hay ajuste por defecto guardado todavia");
        auto candidate=dps::loadProfile(reference);
        const auto selection=SendMessageW(output,CB_GETCURSEL,0,0);
        if(selection>=0&&static_cast<std::size_t>(selection)<physical.size()) candidate.outputId=utf8(physical[selection].id);
        candidate.parameters.enabled=true;
        // Validate before disturbing the live stream. Switching calibration
        // stops/prepares/restarts the worker; no filter reconfiguration on RT.
        dps::Engine validator;validator.prepare(48000,candidate.parameters);
        const bool active=session.running();session.stop();profile=candidate;profilePath.clear();sync();if(active) ensureAudio();
        if(!active||routed) status=L"Ajuste: "+wide(profile.name);
    }
    static LRESULT CALLBACK aboutProc(HWND h,UINT message,WPARAM w,LPARAM l) {
        auto* app=reinterpret_cast<App*>(GetWindowLongPtrW(h,GWLP_USERDATA));
        if(message==WM_CREATE) {
            SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams));
            return 0;
        }
        if(!app) return DefWindowProcW(h,message,w,l);
        switch(message) {
        case WM_NCHITTEST: {
            POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(h,&p);
            return p.y<app->scale(100)?HTCAPTION:HTCLIENT;
        }
        case WM_ERASEBKGND:return 1;
        case WM_PAINT: {
            PAINTSTRUCT p;const auto dc=BeginPaint(h,&p);RECT r;GetClientRect(h,&r);
            app->paintAbout(dc,r);EndPaint(h,&p);return 0;
        }
        case WM_DRAWITEM: {
            auto* item=reinterpret_cast<DRAWITEMSTRUCT*>(l);
            const bool focus=(item->itemState&ODS_FOCUS)!=0,down=(item->itemState&ODS_SELECTED)!=0;
            auto brush=CreateSolidBrush(theme.background);FillRect(item->hDC,&item->rcItem,brush);DeleteObject(brush);
            rounded(item->hDC,item->rcItem,down?theme.surfaceHot:theme.surface,focus?theme.accent:theme.border,app->scale(7));
            SetBkMode(item->hDC,TRANSPARENT);SetTextColor(item->hDC,theme.text);SelectObject(item->hDC,app->font);
            wchar_t caption[64];GetWindowTextW(item->hwndItem,caption,64);
            DrawTextW(item->hDC,caption,-1,&item->rcItem,DT_CENTER|DT_VCENTER|DT_SINGLELINE);return TRUE;
        }
        case WM_COMMAND:
            if(LOWORD(w)==IDOK||LOWORD(w)==IDCANCEL) {DestroyWindow(h);return 0;}
            if(LOWORD(w)==501) {
                if(reinterpret_cast<INT_PTR>(ShellExecuteW(h,L"open",L"https://vb-audio.com/Cable/",nullptr,nullptr,SW_SHOWNORMAL))<=32)
                    MessageBoxW(h,L"No se pudo abrir la web de VB-Audio.",L"Acerca de",MB_OK|MB_ICONWARNING);
                return 0;
            }
            break;
        case WM_CLOSE:DestroyWindow(h);return 0;
        case WM_DESTROY:app->about=nullptr;return 0;
        }
        return DefWindowProcW(h,message,w,l);
    }
    void paintAbout(HDC dc,RECT r) {
        auto brush=CreateSolidBrush(theme.background);FillRect(dc,&r,brush);DeleteObject(brush);
        SetBkMode(dc,TRANSPARENT);SetTextColor(dc,theme.text);
        const auto logo=static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(1),IMAGE_ICON,scale(64),scale(64),LR_SHARED));
        DrawIconEx(dc,scale(24),scale(24),logo,scale(64),scale(64),0,nullptr,DI_NORMAL);
        SelectObject(dc,titleFont);TextOutW(dc,scale(104),scale(22),L"Resoniq",7);
        SIZE nameSize{};GetTextExtentPoint32W(dc,L"Resoniq",7,&nameSize);
        SelectObject(dc,font);TextOutW(dc,scale(104)+nameSize.cx+scale(10),scale(46),L"by Valvik",9);
        RECT version{scale(104),scale(72),scale(580),scale(96)};
        DrawTextW(dc,L"Versión " RESONIQ_VERSION_WTEXT L" · RESONIQ DSP Engine",-1,&version,DT_SINGLELINE);
        rounded(dc,{scale(20),scale(112),r.right-scale(20),scale(402)},theme.card,theme.cardEdge,scale(12));
        RECT text{scale(36),scale(130),r.right-scale(36),scale(390)};
        DrawTextW(dc,
            L"Este software ha sido desarrollado por Víctor Manuel Valenzuela.\n"
            L"© 2026 Víctor Manuel Valenzuela (Valvik).\n\n"
            L"Créditos a VB-CABLE\n"
            L"VB-CABLE es un producto de VB-Audio Software.\n"
            L"Copyright © Vincent Burel. Todos los derechos reservados.\n"
            L"Programación: Vincent Burel y Jean-Sylvain Loezic.\n"
            L"Gracias por el transporte de audio virtual. Se instala por separado y se rige por la licencia de VB-Audio.\n\n"
            L"RESONIQ: software original bajo licencia MIT.\n"
            L"Código y actualizaciones: github.com/valvik77/Resoniq",-1,&text,DT_LEFT|DT_WORDBREAK|DT_NOPREFIX);
    }
    void openAbout() {
        if(about) {ShowWindow(about,SW_SHOW);SetForegroundWindow(about);return;}
        static bool registered=false;
        if(!registered) {
            WNDCLASSW wc{};wc.lpfnWndProc=aboutProc;wc.hInstance=GetModuleHandleW(nullptr);
            wc.lpszClassName=L"Resoniq.About";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);
            wc.hIcon=LoadIconW(wc.hInstance,MAKEINTRESOURCEW(1));
            if(!RegisterClassW(&wc)) throw std::runtime_error("No se pudo preparar Acerca de");
            registered=true;
        }
        RECT owner;GetWindowRect(window,&owner);
        const int width=scale(640),height=scale(480);
        about=CreateWindowExW(0,L"Resoniq.About",L"Acerca de Resoniq by Valvik",WS_POPUP|WS_CLIPCHILDREN,
            owner.left+((owner.right-owner.left)-width)/2,owner.top+scale(150),width,height,
            window,nullptr,GetModuleHandleW(nullptr),this);
        if(!about) throw std::runtime_error("No se pudo abrir Acerca de");
        for(const auto& button:std::array<std::pair<int,const wchar_t*>,2>{{{501,L"Web de VB-CABLE"},{IDOK,L"Cerrar"}}}) {
            const auto c=CreateWindowExW(0,WC_BUTTONW,button.second,WS_CHILD|WS_VISIBLE|WS_TABSTOP|BS_OWNERDRAW,
                scale(button.first==IDOK?498:24),scale(432),scale(button.first==IDOK?118:190),scale(34),
                about,reinterpret_cast<HMENU>(static_cast<INT_PTR>(button.first)),GetModuleHandleW(nullptr),nullptr);
            SendMessageW(c,WM_SETFONT,reinterpret_cast<WPARAM>(font),TRUE);
        }
        BOOL dark=theme.background==darkPalette.background;
        DwmSetWindowAttribute(about,DWMWA_USE_IMMERSIVE_DARK_MODE,&dark,sizeof(dark));
        const DWM_WINDOW_CORNER_PREFERENCE corners=DWMWCP_ROUND;
        DwmSetWindowAttribute(about,DWMWA_WINDOW_CORNER_PREFERENCE,&corners,sizeof(corners));
        ShowWindow(about,SW_SHOW);SetForegroundWindow(about);SetFocus(GetDlgItem(about,IDOK));
    }
    void command(int id,int notification) {
        if(id==Reference) recoverFavorite();
        else if(id==SetDefault) setAsDefault();
        else if(id==Settings) openSettings();
        else if(id==About) openAbout();
        else if(id==Detect) applyDetected(true);
        else if(id==Measure) runMeasurement();
        else if(id==Minimise) ShowWindow(window,SW_MINIMIZE);
        else if(id==Close) PostMessageW(window,WM_CLOSE,0,0);
        else if(id==Route) {if(reinterpret_cast<INT_PTR>(ShellExecuteW(window,L"open",L"ms-settings:sound",nullptr,nullptr,SW_SHOWNORMAL))<=32) throw std::runtime_error("No se pudo abrir la configuracion de sonido");}
        else if(id==Toggle) {
            if(!selfTest&&(!session.running()||session.finished.load(std::memory_order_acquire))) {
                profile.parameters.enabled=true;session.state.enabled=true;ensureAudio();
            }else {profile.parameters.enabled=!profile.parameters.enabled;session.state.enabled=profile.parameters.enabled;}
            buttons();
        }
        else if(id==Refresh) {const bool active=session.running();refresh();if(active) {session.stop();ensureAudio();}else status=L"Dispositivos actualizados.";}
        else if(id==Load) file(false);
        else if(id==Save) file(true);
        else if(id==Raw) {profile.raw=SendMessageW(raw,BM_GETCHECK,0,0)==BST_CHECKED;if(session.running()) {session.stop();ensureAudio();}}
        else if(id==Output&&notification==CBN_SELCHANGE) {if(session.running()) {session.stop();ensureAudio();}}
        else if(id==Input&&notification==CBN_SELCHANGE) {
            const auto selected=static_cast<int>(SendMessageW(input,CB_GETCURSEL,0,0));
            if(selected>=0&&static_cast<std::size_t>(selected)<virtualInputs.size()) {
                const bool active=session.running();session.stop();virtualId=virtualInputs[selected].id;
                prefs.virtualInputId=utf8(virtualId);routed=virtualInputs[selected].defaultRender;
                if(!selfTest&&!audioTest) dps::savePreferences(root/L"config/settings.user.txt",prefs);
                if(active) ensureAudio();else status=L"Entrada virtual seleccionada.";
            }
        }
        else if(id==Device&&notification==CBN_SELCHANGE) {const auto n=SendMessageW(device,CB_GETCURSEL,0,0);if(n>=0&&n<7) {
            // "Measured" has no fixed calibration: its values live in the
            // profile itself, so re-selecting it must not overwrite them.
            const std::string chosen=devices[n];
            if(chosen=="Measured") {
                status=profile.deviceName=="Measured"?L"Calibracion medida en uso.":
                    L"Usa Medir o carga un perfil medido para elegir esta opcion.";
                sync();InvalidateRect(window,nullptr,FALSE);return;
            }
            profile.deviceName=chosen;profile.parameters.device=dps::deviceCalibration(profile.deviceName);status=L"Perfil aplicado: "+wide(profile.deviceName);if(session.running()) {session.stop();ensureAudio();}}}
        else if(id==Preset&&notification==CBN_SELCHANGE) {const auto n=SendMessageW(preset,CB_GETCURSEL,0,0);if(n>=0&&n<4) {profile.presetName=utf8(presetLabels[n]);const auto p=dps::preset(profile.presetName);profile.parameters.bass=p.bass;profile.parameters.clarity=p.clarity;profile.parameters.dynamics=p.dynamics;profile.parameters.spatial=p.spatial;sync();status=L"Preset: "+wide(profile.presetName);}}
        InvalidateRect(window,nullptr,FALSE);
    }
    void paint(HDC dc) {
        RECT client;GetClientRect(window,&client);HBRUSH brush=CreateSolidBrush(theme.background);FillRect(dc,&client,brush);DeleteObject(brush);
        SetBkMode(dc,TRANSPARENT);SelectObject(dc,font);
        auto label=[&](int x,int y,const std::wstring& value,COLORREF color=theme.text){SetTextColor(dc,color);TextOutW(dc,scale(x),scale(y),value.c_str(),static_cast<int>(value.size()));};
        auto box=[&](int x,int y,int w,int h,COLORREF color) {RECT r{scale(x),scale(y),scale(x+w),scale(y+h)};auto b=CreateSolidBrush(color);FillRect(dc,&r,b);DeleteObject(b);};
        rounded(dc,{scale(20),scale(18),scale(860),scale(193)},theme.card,theme.cardEdge,scale(12));
        const auto logo=static_cast<HICON>(LoadImageW(GetModuleHandleW(nullptr),MAKEINTRESOURCEW(1),IMAGE_ICON,scale(64),scale(64),LR_SHARED));
        DrawIconEx(dc,scale(35),scale(26),logo,scale(64),scale(64),0,nullptr,DI_NORMAL);
        SelectObject(dc,titleFont);label(112,26,L"Resoniq");SelectObject(dc,font);
        label(300,49,L"by Valvik",theme.mutedText);
        label(112,78,L"RESONIQ DSP Engine",theme.mutedText);
        label(37,105,L"Tu sonido. Con mas cuerpo.",theme.text);
        label(430,105,L"Actividad DSP",theme.mutedText);
        // Real peak history from the audio worker, rendered on the UI thread.
        std::array<POINT,192> wave{};
        for(std::size_t i=0;i<activity.size();++i) {
            const int x=scale(38+static_cast<int>(i)*6);
            const int height=scale(static_cast<int>(std::sqrt(std::clamp(activity[i],0.f,1.f))*23));
            wave[i]={x,scale(155)-height};wave[191-i]={x,scale(155)+height};
        }
        auto waveBrush=CreateSolidBrush(theme.wave);auto wavePen=CreatePen(PS_SOLID,scale(1),theme.accent);
        auto oldBrush=SelectObject(dc,waveBrush),oldPen=SelectObject(dc,wavePen);
        Polygon(dc,wave.data(),static_cast<int>(wave.size()));
        SelectObject(dc,oldBrush);SelectObject(dc,oldPen);DeleteObject(waveBrush);DeleteObject(wavePen);
        label(30,200,L"Salida de audio",theme.mutedText);label(450,200,L"Entrada virtual",theme.mutedText);label(30,277,L"Perfil del dispositivo",theme.mutedText);label(290,277,L"Perfil acustico",theme.mutedText);
        {
            // Windows' own reading for the selected output, right-aligned with
            // the list below it. Shown with the DSP comparison in view: the
            // application never equalises loudness, so the user can check that
            // the physical level is the same on both sides of the comparison.
            const auto caption=dps::volumeCaption(outputLevel);
            SIZE extent{};GetTextExtentPoint32W(dc,caption.c_str(),static_cast<int>(caption.size()),&extent);
            SetTextColor(dc,outputLevel.muted?theme.warm:theme.mutedText);
            TextOutW(dc,scale(435)-extent.cx,scale(200),caption.c_str(),static_cast<int>(caption.size()));
        }
        rounded(dc,{scale(20),scale(410),scale(860),scale(715)},theme.card,theme.cardEdge,scale(12));
        drawIcon(dc,Icon::Intensity,scale(35),scale(424),scale(22),theme.accent,scale(2)*1.f);
        label(67,425,L"INTENSIDAD",theme.text);label(67,449,L"Mezcla del procesamiento",theme.mutedText);
        label(785,437,std::to_wstring(SendMessageW(sliders[4],TBM_GETPOS,0,0))+L"%",theme.accent);
        box(35,482,810,1,theme.divider);
        const std::array<const wchar_t*,4> names{L"Bass",L"Clarity",L"Dynamics",L"Spatial"};
        const std::array<const wchar_t*,4> descriptions{L"Cuerpo y graves virtuales",L"Presencia y detalle",L"Control de dinamica",L"Amplitud estereo"};
        const std::array<Icon,4> rowIcons{Icon::Bass,Icon::Clarity,Icon::Dynamics,Icon::Spatial};
        for(int i=0;i<4;++i) {drawIcon(dc,rowIcons[i],scale(35),scale(496+i*55),scale(20),theme.mutedText,scale(2)*1.f);
            label(67,493+i*55,names[i]);label(67,513+i*55,descriptions[i],theme.mutedText);label(785,501+i*55,std::to_wstring(SendMessageW(sliders[i],TBM_GETPOS,0,0))+L"%",theme.accent);}
        auto meter=[&](int x,const wchar_t* name,float value,bool reduction) {
            float db=reduction?value:(value>0?20*std::log10(value):-60.f);
            wchar_t number[64];if(!reduction&&db<=-60) wcscpy_s(number,L" < -60 dBFS");else swprintf_s(number,L" %.1f %s",db,reduction?L"dB":L"dBFS");
            label(x,735,std::wstring(name)+number,theme.mutedText);box(x,765,250,7,theme.meter);
            const float amount=std::clamp(reduction?db/18:(db+60)/60,0.f,1.f);box(x,765,static_cast<int>(250*amount),7,reduction?theme.warm:theme.accent);
        };
        meter(30,L"Entrada",session.state.inputPeak.load(),false);meter(315,L"Salida",session.state.outputPeak.load(),false);
        // The old "Reducción" meter only moved when the compressor or limiter
        // pulled gain down, which at normal listening level almost never
        // happens: it read 0.0 dB permanently and told the user nothing. Show
        // the gain the engine is actually adding, and note compression when
        // there is any, so the meter reflects the work being done.
        {
            const float in=session.state.inputPeak.load();
            const float gain=displayGainDb();
            const float cut=session.state.reduction.load();
            wchar_t number[96];
            if(in<=1e-4f) wcscpy_s(number,L" sin señal");
            else swprintf_s(number,L" %+.1f dB",gain);
            label(600,735,std::wstring(L"Realce DSP")+number,theme.mutedText);box(600,765,250,7,theme.meter);
            const float amount=std::clamp(gain/18,0.f,1.f);
            box(600,765,static_cast<int>(250*amount),7,theme.warm);
            if(cut>.1f) {swprintf_s(number,L"Compresión: %.1f dB",cut);label(600,784,number,theme.mutedText);}
        }
        if(profile.deviceName=="Laptop Focus"||profile.deviceName.rfind("Laptop Impact",0)==0) {
            wchar_t value[128];swprintf_s(value,L"Detalle +%.1f dB | Ataque +%.1f dB",session.state.dynamicLiftDb.load(),session.state.transientLiftDb.load());label(490,806,value,theme.mutedText);
        }
        label(30,855,status,theme.mutedText);
        label(30,887,L"Resoniq " RESONIQ_VERSION_WTEXT L"  /  Audio en tiempo real",theme.mutedText);
    }
    // Capture any window of ours, used to review the settings dialog too.
    // Grab from the screen, so a dropped-down list (its own window) is included.
    void screenshotScreen(HWND target,const std::filesystem::path& path) {
        RECT b;GetWindowRect(target,&b);
        const int w=b.right-b.left,h=b.bottom-b.top+scale(120);
        HDC screen=GetDC(nullptr),mem=CreateCompatibleDC(screen);
        HBITMAP bmp=CreateCompatibleBitmap(screen,w,h);auto old=SelectObject(mem,bmp);
        BitBlt(mem,0,0,w,h,screen,b.left,b.top,SRCCOPY);
        SelectObject(mem,old);
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=w;
        info.bmiHeader.biHeight=-h;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        std::vector<unsigned char> pixels(static_cast<std::size_t>(w)*h*4);
        GetDIBits(screen,bmp,0,h,pixels.data(),&info,DIB_RGB_COLORS);
        BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);
        header.bfSize=header.bfOffBits+static_cast<DWORD>(pixels.size());
        std::ofstream out(path,std::ios::binary);
        out.write(reinterpret_cast<char*>(&header),sizeof(header));
        out.write(reinterpret_cast<char*>(&info.bmiHeader),sizeof(info.bmiHeader));
        out.write(reinterpret_cast<char*>(pixels.data()),static_cast<std::streamsize>(pixels.size()));
        DeleteObject(bmp);DeleteDC(mem);ReleaseDC(nullptr,screen);
        if(!out) throw std::runtime_error("Dropdown screenshot write failed");
    }
    void screenshot2(HWND target,const std::filesystem::path& path) {
        UpdateWindow(target);
        RECT r;GetClientRect(target,&r);
        HDC dc=GetDC(target),mem=CreateCompatibleDC(dc);
        HBITMAP bmp=CreateCompatibleBitmap(dc,r.right,r.bottom);auto old=SelectObject(mem,bmp);
        if(target==about) paintAbout(mem,r);else paintSettings(mem,r);
        for(HWND child=GetWindow(target,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)) {
            RECT b;GetWindowRect(child,&b);MapWindowPoints(HWND_DESKTOP,target,reinterpret_cast<POINT*>(&b),2);
            const int saved=SaveDC(mem);SetViewportOrgEx(mem,b.left,b.top,nullptr);
            SendMessageW(child,WM_PRINT,reinterpret_cast<WPARAM>(mem),PRF_CLIENT|PRF_NONCLIENT|PRF_ERASEBKGND);
            RestoreDC(mem,saved);
        }
        SelectObject(mem,old);
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=r.right;
        info.bmiHeader.biHeight=-r.bottom;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        std::vector<unsigned char> pixels(static_cast<std::size_t>(r.right)*r.bottom*4);
        GetDIBits(dc,bmp,0,r.bottom,pixels.data(),&info,DIB_RGB_COLORS);
        BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);
        header.bfSize=header.bfOffBits+static_cast<DWORD>(pixels.size());
        std::ofstream out(path,std::ios::binary);
        out.write(reinterpret_cast<char*>(&header),sizeof(header));
        out.write(reinterpret_cast<char*>(&info.bmiHeader),sizeof(info.bmiHeader));
        out.write(reinterpret_cast<char*>(pixels.data()),static_cast<std::streamsize>(pixels.size()));
        DeleteObject(bmp);DeleteDC(mem);ReleaseDC(target,dc);
        if(!out) throw std::runtime_error("Settings screenshot write failed");
    }
    void screenshot(const std::filesystem::path& path) {
        RECT r;GetClientRect(window,&r);HDC dc=GetDC(window),mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,r.right,r.bottom);auto old=SelectObject(mem,bmp);
        paint(mem);
        for(HWND child=GetWindow(window,GW_CHILD);child;child=GetWindow(child,GW_HWNDNEXT)) {
            RECT bounds;GetWindowRect(child,&bounds);MapWindowPoints(HWND_DESKTOP,window,reinterpret_cast<POINT*>(&bounds),2);
            const int saved=SaveDC(mem);SetViewportOrgEx(mem,bounds.left,bounds.top,nullptr);
            SendMessageW(child,WM_PRINT,reinterpret_cast<WPARAM>(mem),PRF_CLIENT|PRF_NONCLIENT|PRF_ERASEBKGND);RestoreDC(mem,saved);
        }
        SelectObject(mem,old);
        BITMAPINFO info{};info.bmiHeader.biSize=sizeof(BITMAPINFOHEADER);info.bmiHeader.biWidth=r.right;info.bmiHeader.biHeight=-r.bottom;info.bmiHeader.biPlanes=1;info.bmiHeader.biBitCount=32;info.bmiHeader.biCompression=BI_RGB;
        std::vector<unsigned char> pixels(static_cast<std::size_t>(r.right)*r.bottom*4);GetDIBits(dc,bmp,0,r.bottom,pixels.data(),&info,DIB_RGB_COLORS);
        BITMAPFILEHEADER header{};header.bfType=0x4d42;header.bfOffBits=sizeof(header)+sizeof(BITMAPINFOHEADER);header.bfSize=header.bfOffBits+static_cast<DWORD>(pixels.size());
        std::ofstream out(path,std::ios::binary);out.write(reinterpret_cast<char*>(&header),sizeof(header));out.write(reinterpret_cast<char*>(&info.bmiHeader),sizeof(info.bmiHeader));out.write(reinterpret_cast<char*>(pixels.data()),static_cast<std::streamsize>(pixels.size()));
        DeleteObject(bmp);DeleteDC(mem);ReleaseDC(window,dc);if(!out) throw std::runtime_error("Screenshot write failed");
    }
    void test() {
        if(!SendMessageW(window,WM_QUERYENDSESSION,0,ENDSESSION_CLOSEAPP))
            throw std::runtime_error("Restart Manager shutdown request rejected");
        SendMessageW(window,WM_ENDSESSION,FALSE,ENDSESSION_CLOSEAPP);
        if(quitting||!IsWindow(window)) throw std::runtime_error("Cancelled shutdown closes application");
        openAbout();
        screenshot2(about,root/L"build/about-preview.bmp");
        SendMessageW(about,WM_COMMAND,IDCANCEL,0);
        if(about) throw std::runtime_error("Acerca de did not close");
        {
            const auto previousId=prefs.virtualInputId;
            dps::Endpoint simulated;simulated.id=L"test-vb-cable";simulated.name=L"CABLE Input (VB-Audio Virtual Cable)";
            virtualInputs.push_back(simulated);
            SendMessageW(input,CB_ADDSTRING,0,reinterpret_cast<LPARAM>(simulated.name.c_str()));
            SendMessageW(input,CB_SETCURSEL,virtualInputs.size()-1,0);command(Input,CBN_SELCHANGE);
            if(virtualId!=simulated.id||prefs.virtualInputId!="test-vb-cable") throw std::runtime_error("Virtual input control lost selection");
            prefs.virtualInputId="missing-input";refresh();
            if(!virtualId.empty()||SendMessageW(input,CB_GETCURSEL,0,0)!=CB_ERR) throw std::runtime_error("Missing virtual input substituted");
            prefs.virtualInputId=previousId;refresh();
        }
        SendMessageW(sliders[4],TBM_SETPOS,TRUE,37);publish();if(session.state.intensity.load()!=.37f) throw std::runtime_error("Slider did not reach audio state");
        SendMessageW(sliders[4],WM_KEYDOWN,VK_RIGHT,0);
        if(SendMessageW(sliders[4],TBM_GETPOS,0,0)!=38||session.state.intensity.load()!=.38f) throw std::runtime_error("Styled slider keyboard interaction failed");
        SendMessageW(sliders[4],TBM_SETPOS,TRUE,37);publish();
        const bool previousRaw=profile.raw;SendMessageW(raw,BM_CLICK,0,0);
        if(profile.raw==previousRaw) throw std::runtime_error("Styled switch interaction failed");
        SendMessageW(raw,BM_CLICK,0,0);if(profile.raw!=previousRaw) throw std::runtime_error("Styled switch restore failed");
        SendMessageW(preset,CB_SETCURSEL,1,0);command(Preset,CBN_SELCHANGE);if(session.state.bass.load()!=.3f) throw std::runtime_error("Preset control failed");
        if(profile.presetName!="RESONIQ Enhance") throw std::runtime_error("Branded preset selection failed");
        const auto path=root/L"build/gui-test.dps";dps::saveProfile(path,profile);const auto loaded=dps::loadProfile(path);if(loaded.parameters.intensity!=.37f) throw std::runtime_error("GUI profile roundtrip failed");
        const auto originalProfile=profile;const auto originalPath=profilePath;
        profilePath=L"config/session.user.dps";file(true,true);file(false,true);
        profile.deviceName="Measured";profile.parameters.device.highpassHz=162;
        profile.parameters.device.bodyDb=7.3f;profile.parameters.device.presenceDb=6.2f;
        profilePath.clear();sync();file(true,true);
        const auto measuredPath=root/L"build/gui-medido-test.dps";saveTo(measuredPath);
        profile=originalProfile;sync();loadFrom(measuredPath);
        if(profile.deviceName!="Measured"||profile.parameters.device.highpassHz!=162||
           profile.parameters.device.bodyDb!=7.3f||profile.parameters.device.presenceDb!=6.2f)
            throw std::runtime_error("Measured profile values lost on GUI save/reload");
        const auto savedName=profile.name;bool saveFailed=false;
        try {saveTo(root/L"build");}catch(const std::exception&) {saveFailed=true;}
        if(!saveFailed||profile.name!=savedName||profilePath!=measuredPath)
            throw std::runtime_error("Failed save alters active profile metadata");
        profile=originalProfile;profilePath=originalPath;sync();
        profile.parameters.enabled=true;session.state.enabled=true;
        command(Toggle,BN_CLICKED);if(session.state.enabled.load()) throw std::runtime_error("DSP bypass publication failed");
        command(Toggle,BN_CLICKED);if(!session.state.enabled.load()) throw std::runtime_error("DSP enable publication failed");
        const auto controls=profile.parameters;
        SendMessageW(device,CB_SETCURSEL,3,0);command(Device,CBN_SELCHANGE);
        if(profile.deviceName!="Laptop Focus"||profile.parameters.device.highpassHz!=dps::deviceCalibration("Laptop Focus").highpassHz)
            throw std::runtime_error("Automatic device profile failed");
        if(profile.parameters.bass!=controls.bass||profile.parameters.clarity!=controls.clarity||profile.parameters.intensity!=controls.intensity)
            throw std::runtime_error("Device profile overwrites independent controls");
        SendMessageW(device,CB_SETCURSEL,6,0);command(Device,CBN_SELCHANGE);
        if(profile.deviceName!="Laptop Focus") throw std::runtime_error("Unmeasured profile claims to be measured");
        {
            const auto previousProfile=profile;
            profile.deviceName="Measured";profile.parameters.device.highpassHz=162;
            profile.parameters.device.bodyDb=7.3f;sync();command(Device,CBN_SELCHANGE);
            if(profile.parameters.device.highpassHz!=162||profile.parameters.device.bodyDb!=7.3f)
                throw std::runtime_error("Measured calibration overwritten by selection");
            profile=previousProfile;sync();
        }
        // Hardware detection must recognise this machine, and must never pick
        // a speaker correction for a headset.
        {
            const auto detected=detectCalibration();
            std::ofstream report(root/L"build/hardware-detect.txt");
            const auto hw=dps::detectHardware(L"Altavoces (Realtek(R) Audio)");
            report<<"manufacturer="<<hw.manufacturer<<char(10)<<"model="<<hw.model
                  <<char(10)<<"family="<<hw.family<<char(10)<<"portable="<<hw.portable
                  <<char(10)<<"profile="<<detected.deviceName
                  <<char(10)<<"basis="<<static_cast<int>(detected.basis)
                  <<char(10)<<"reason="<<detected.reason<<char(10);
            if(detected.deviceName.empty()) throw std::runtime_error("Detection returned no profile");
            (void)dps::deviceCalibration(detected.deviceName);
            const auto head=dps::suggestCalibration(dps::detectHardware(L"Auriculares (Realtek)"));
            if(head.deviceName!="Headphones") throw std::runtime_error("Headset endpoint got a speaker profile");
            if(head.calibration.highpassHz>60) throw std::runtime_error("Headset correction cuts bass");
        }
        // Frameless window: no system caption or resizing frame, the top strip
        // acts as the title bar, and our own buttons must be clickable.
        {
            const LONG style=GetWindowLongW(window,GWL_STYLE);
            if(style&WS_CAPTION) throw std::runtime_error("Window still has a system title bar");
            if(style&WS_THICKFRAME) throw std::runtime_error("Window is still resizable");
            if(!GetDlgItem(window,Minimise)||!GetDlgItem(window,Close))
                throw std::runtime_error("Window buttons missing");
            RECT wr;GetWindowRect(window,&wr);
            const LRESULT hit=SendMessageW(window,WM_NCHITTEST,0,
                MAKELPARAM((wr.left+wr.right)/2,wr.top+scale(20)));
            if(hit!=HTCAPTION) throw std::runtime_error("Top strip does not drag the window");
            // A point over the sliders must stay client area, or nothing below
            // the header would be clickable.
            RECT slider;GetWindowRect(sliders[0],&slider);
            if(SendMessageW(window,WM_NCHITTEST,0,MAKELPARAM(slider.left+4,slider.top+4))!=HTCLIENT)
                throw std::runtime_error("Controls area treated as caption");
        }
        // Both themes must render the main window, settings and About without
        // changing sound or saving a temporary appearance preference.
        {
            const int previousTheme=prefs.themeMode;
            for(const int mode:{1,2}) {
                prefs.themeMode=mode;applyTheme();
                screenshot(root/(mode==1?L"build/gui-dark-preview.bmp":L"build/gui-light-preview.bmp"));
                openSettings();
                screenshot2(settings,root/(mode==1?L"build/settings-dark-preview.bmp":L"build/settings-light-preview.bmp"));
                SendMessageW(settings,WM_COMMAND,SetCancel,0);
                openAbout();
                screenshot2(about,root/(mode==1?L"build/about-dark-preview.bmp":L"build/about-light-preview.bmp"));
                SendMessageW(about,WM_COMMAND,IDCANCEL,0);
            }
            prefs.themeMode=previousTheme;applyTheme();
        }
        // Settings window: it must open, carry its controls, and close without
        // disturbing the main window or the saved preferences.
        {
            const Preferences before=prefs;
            openSettings();
            if(!settings) throw std::runtime_error("Settings window did not open");
            if(GetWindowLongW(settings,GWL_STYLE)&WS_CAPTION)
                throw std::runtime_error("Settings window still has a system title bar");
            RECT sr;GetWindowRect(settings,&sr);
            if(SendMessageW(settings,WM_NCHITTEST,0,MAKELPARAM(sr.left+scale(120),sr.top+scale(20)))!=HTCAPTION)
                throw std::runtime_error("Settings title strip does not drag");
            for(const int id:{SetStart,SetMinimised,SetCloseExits,SetReconnect,SetDspMode,SetTheme,SetOk,SetCancel,SetClose})
                if(!GetDlgItem(settings,id)) throw std::runtime_error("Settings control missing");
            if(SendMessageW(GetDlgItem(settings,SetDspMode),CB_GETCOUNT,0,0)!=3)
                throw std::runtime_error("Settings DSP choices missing");
            screenshot2(settings,root/L"build/settings-preview.bmp");
            // Capture the list open too: its items are drawn by us now, and
            // that is exactly the part that looked wrong.
            SendMessageW(GetDlgItem(settings,SetDspMode),CB_SHOWDROPDOWN,TRUE,0);
            UpdateWindow(settings);
            screenshotScreen(settings,root/L"build/settings-open.bmp");
            SendMessageW(GetDlgItem(settings,SetDspMode),CB_SHOWDROPDOWN,FALSE,0);
            SendMessageW(settings,WM_COMMAND,SetCancel,0);
            if(settings) throw std::runtime_error("Settings window did not close");
            if(prefs.startWithWindows!=before.startWithWindows||prefs.closeExits!=before.closeExits)
                throw std::runtime_error("Cancel changed the preferences");
        }
        status=L"Comprobacion de controles, presets y perfiles completada.";InvalidateRect(window,nullptr,FALSE);UpdateWindow(window);screenshot(root/L"build/gui-preview.bmp");
        std::ofstream(root/L"build/gui-test-result.txt")<<"PASS: styled slider keyboard, switch clicks, DSP toggle, preset selection, profile roundtrip, window rendering\n";PostMessageW(window,WM_CLOSE,0,0);
    }
};
LRESULT CALLBACK procedure(HWND h,UINT message,WPARAM w,LPARAM l) {
    auto* a=reinterpret_cast<App*>(GetWindowLongPtrW(h,GWLP_USERDATA));
    if(message==WM_NCCREATE) {a=static_cast<App*>(reinterpret_cast<CREATESTRUCTW*>(l)->lpCreateParams);a->window=h;SetWindowLongPtrW(h,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(a));}
    if(!a) return DefWindowProcW(h,message,w,l);
    try {
        switch(message) {
        case WM_CREATE:a->init();return 0;
        case WM_COMMAND:{
            // Tray menu entries arrive with no control window (lParam null).
            const int id=LOWORD(w);
            if(!l&&id>=TrayShow&&id<=TrayExit) {
                if(id==TrayShow) a->showWindow();
                else if(id==TrayToggle) {a->command(Toggle,BN_CLICKED);a->trayTip();}
                else if(id==TrayFavorite) {a->recoverFavorite();a->trayTip();}
                else if(id==TraySetDefault) a->setAsDefault();
                else {a->quitting=true;PostMessageW(h,WM_CLOSE,0,0);}
                return 0;
            }
            a->command(id,HIWORD(w));return 0;}
        case WM_HSCROLL:a->publish();return 0;
        case WM_POWERBROADCAST:
            if(w==PBT_APMSUSPEND) {
                a->resumeAudio=a->resumeAudio||a->session.running()||a->reconnectAt!=0;
                a->powerSuspended=true;a->reconnectAt=0;a->session.stop();
                a->status=L"Audio en espera mientras Windows esta suspendido.";a->buttons();
            } else if(w==PBT_APMRESUMEAUTOMATIC||w==PBT_APMRESUMESUSPEND) {
                a->powerSuspended=false;
                if(a->resumeAudio&&a->prefs.autoReconnect) a->reconnectAt=a->ticks+10;
                a->resumeAudio=false;
            }
            return TRUE;
        case WM_TIMER:
            if(w==4) {
                KillTimer(h,4);
                try {a->ensureAudio();}
                catch(const std::exception& e) {
                    if(a->audioTest) throw;
                    a->status=L"Audio pendiente: "+wide(e.what());
                    if(a->prefs.autoReconnect) a->reconnectAt=a->ticks+30;
                    a->buttons();
                }
                return 0;
            }
            if(w==5) {
                KillTimer(h,5);
                if(!a->session.running()||a->session.finished.load(std::memory_order_acquire))
                    throw std::runtime_error("Audio restart failed: "+a->session.failure);
                a->session.stop();std::ofstream(a->root/L"build/gui-audio-result.txt")<<"PASS: WASAPI startup, DSP toggle, simulated suspend/resume and fresh stream restart\n";
                PostMessageW(h,WM_CLOSE,0,0);return 0;
            }
            if(w==3) {
                KillTimer(h,3);
                if(!a->session.running()||a->session.finished.load(std::memory_order_acquire)) throw std::runtime_error("Audio smoke test failed: "+a->session.failure);
                const auto worker=a->session.thread.get_id();const bool enabled=a->session.state.enabled.load();
                a->command(Toggle,BN_CLICKED);
                if(a->session.state.enabled.load()==enabled||!a->session.running()||a->session.thread.get_id()!=worker)
                    throw std::runtime_error("DSP toggle interrupts the audio route");
                a->command(Toggle,BN_CLICKED);
                if(a->session.state.enabled.load()!=enabled||a->session.thread.get_id()!=worker)
                    throw std::runtime_error("DSP toggle fails to restore processing state");
                SendMessageW(h,WM_POWERBROADCAST,PBT_APMSUSPEND,0);
                if(a->session.running()||!a->powerSuspended) throw std::runtime_error("Suspend leaves audio running");
                SendMessageW(h,WM_POWERBROADCAST,PBT_APMRESUMEAUTOMATIC,0);
                a->reconnectAt=0;a->refresh();a->ensureAudio();
                SetTimer(h,5,1200,nullptr);return 0;
            }
            if(w==2) {KillTimer(h,2);a->test();return 0;}
            if(a->session.running()&&a->session.finished.load(std::memory_order_acquire)) {
                const auto error=a->session.failure;a->session.stop();
                a->status=error.empty()?L"Audio detenido":L"Error de audio: "+wide(error);a->buttons();
                // Device changes (headphones in or out, a driver reset) end the
                // worker. Retry on a timer rather than leaving it silent, but
                // space the attempts so a device that stays gone is not hammered.
                if(a->prefs.autoReconnect&&!a->selfTest&&!a->audioTest) {a->reconnectAt=a->ticks+30;a->trayTip();}
            }
            if(a->reconnectAt&&!a->prefs.autoReconnect) a->reconnectAt=0;
            if(!a->powerSuspended&&a->reconnectAt&&a->ticks>=a->reconnectAt&&!a->session.running()) {
                a->reconnectAt=0;
                try {a->refresh();a->ensureAudio();a->status=L"Audio reconectado.";a->trayTip();}
                catch(const std::exception& e) {
                    a->status=L"Reintentando conexion: "+wide(e.what());
                    a->reconnectAt=a->ticks+50;
                }
            }
            if(a->ticks++%10==0) {
                a->readOutputLevel();
                if(a->session.running()) {
                    try {
                        bool routed=false;for(const auto& endpoint:dps::endpoints()) if(endpoint.defaultRender&&endpoint.id==a->virtualId) routed=true;
                        if(!routed) a->status=L"Selecciona la entrada virtual elegida como salida de Windows para procesar.";
                        else if(!a->routed) a->status=L"Ruta del sistema activa: entrada virtual > RESONIQ > salida fisica.";
                        a->routed=routed;
                    }catch(const std::exception& e) {if(a->telemetry) a->telemetry<<"routing_check_error="<<e.what()<<'\n';}
                }
                if(a->telemetry) {
                const auto& calibration=a->profile.parameters.device;
                a->telemetry<<"device="<<a->profile.deviceName<<" virtual_input="<<utf8(a->virtualId)
                    <<" output_volume="<<a->outputLevel.volume<<" output_muted="<<a->outputLevel.muted
                    <<" processed_output="<<a->session.state.outputPeak.load()<<" dsp_gain_db="<<a->displayGainDb()
                    <<" highpass_hz="<<calibration.highpassHz<<" body_db="<<calibration.bodyDb<<" presence_db="<<calibration.presenceDb
                    <<" loud_density="<<calibration.loudDensity<<" band_density="<<calibration.bandDensity<<' ';

                a->telemetry<<"running="<<a->session.running()<<" finished="<<a->session.finished.load()<<" enabled="<<a->session.state.enabled.load()<<" intensity="<<a->session.state.intensity.load()<<" uptime_ms="<<GetTickCount64()<<" transient_db="<<a->session.state.transientLiftDb.load()<<" detail_db="<<a->session.state.dynamicLiftDb.load()<<" bass="<<a->session.state.bass.load()<<" clarity="<<a->session.state.clarity.load()<<" dynamics="<<a->session.state.dynamics.load()<<" spatial="<<a->session.state.spatial.load()<<" input="<<a->session.state.inputPeak.load()<<" output="<<a->session.state.outputPeak.load()<<" reduction="<<a->session.state.reduction.load()<<" under="<<a->session.state.underruns.load()<<" over="<<a->session.state.overflows.load()<<" latency_ms="<<a->session.state.latencyMs.load()<<'\n';a->telemetry.flush();
                }
            }
            std::move(a->activity.begin()+1,a->activity.end(),a->activity.begin());
            a->activity.back()=a->session.state.outputPeak.load();
            InvalidateRect(h,nullptr,FALSE);return 0;
        case WM_MEASUREITEM:{auto* item=reinterpret_cast<MEASUREITEMSTRUCT*>(l);item->itemHeight=static_cast<UINT>(a->scale(24));return TRUE;}
        case WM_DRAWITEM: {
            const auto* item=reinterpret_cast<DRAWITEMSTRUCT*>(l);
            if(item->CtlType==ODT_COMBOBOX) {
                auto b=CreateSolidBrush(theme.card);FillRect(item->hDC,&item->rcItem,b);DeleteObject(b);
                if(item->itemID!=static_cast<UINT>(-1)) {
                    const auto count=SendMessageW(item->hwndItem,CB_GETLBTEXTLEN,item->itemID,0);
                    if(count>=0) {std::wstring value(static_cast<std::size_t>(count)+1,0);SendMessageW(item->hwndItem,CB_GETLBTEXT,item->itemID,reinterpret_cast<LPARAM>(value.data()));
                        SetBkMode(item->hDC,TRANSPARENT);SetTextColor(item->hDC,theme.text);SelectObject(item->hDC,a->font);RECT r=item->rcItem;r.left+=a->scale(8);DrawTextW(item->hDC,value.c_str(),-1,&r,DT_LEFT|DT_VCENTER|DT_SINGLELINE|DT_END_ELLIPSIS);}
                }
                if(item->itemState&ODS_FOCUS) DrawFocusRect(item->hDC,&item->rcItem);return TRUE;
            }
            if(item->CtlID==Toggle) {
                const bool on=a->profile.parameters.enabled;
                auto base=CreateSolidBrush(theme.card);FillRect(item->hDC,&item->rcItem,base);DeleteObject(base);
                rounded(item->hDC,item->rcItem,on?(a->hovered==item->hwndItem?theme.toggleHot:theme.toggleOn):theme.button,on?theme.accent:theme.toggleEdge,a->scale(12));
                const int cx=(item->rcItem.left+item->rcItem.right)/2,cy=item->rcItem.top+a->scale(38),radius=a->scale(19);
                {
                    Gdiplus::Graphics graphics(item->hDC);graphics.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                    const auto color=on?theme.accent:theme.mutedText;Gdiplus::Pen pen(Gdiplus::Color(255,GetRValue(color),GetGValue(color),GetBValue(color)),static_cast<float>(a->scale(3)));
                    pen.SetStartCap(Gdiplus::LineCapRound);pen.SetEndCap(Gdiplus::LineCapRound);
                    graphics.DrawArc(&pen,cx-radius,cy-radius,radius*2,radius*2,-45.f,270.f);
                    graphics.DrawLine(&pen,cx,cy-a->scale(24),cx,cy+a->scale(1));
                }
                SetBkMode(item->hDC,TRANSPARENT);SetTextColor(item->hDC,on?theme.text:theme.mutedText);SelectObject(item->hDC,a->font);
                RECT caption=item->rcItem;caption.top+=a->scale(70);caption.bottom=caption.top+a->scale(24);
                wchar_t captionText[64];GetWindowTextW(item->hwndItem,captionText,64);DrawTextW(item->hDC,captionText,-1,&caption,DT_CENTER|DT_VCENTER|DT_SINGLELINE);
                caption.top+=a->scale(26);caption.bottom+=a->scale(26);SetTextColor(item->hDC,theme.mutedText);
                DrawTextW(item->hDC,!a->session.running()&&!a->selfTest?L"Pulsa para conectar":on?L"Procesado":L"Original",-1,&caption,DT_CENTER|DT_SINGLELINE);
                return TRUE;
            }
            if(item->CtlID==Minimise||item->CtlID==Close) {
                // Title-bar buttons: flat, square, highlighted on hover, with
                // the close one turning red the way Windows does.
                const bool closing=item->CtlID==Close;
                const bool over=a->hovered==item->hwndItem;
                const bool down=(item->itemState&ODS_SELECTED)!=0;
                COLORREF fill=theme.card; // matches the header card behind them
                if(down) fill=closing?RGB(170,30,38):theme.buttonDown;
                else if(over) fill=closing?RGB(196,43,28):theme.buttonHot;
                auto back=CreateSolidBrush(fill);FillRect(item->hDC,&item->rcItem,back);DeleteObject(back);
                const COLORREF ink=(over||down)&&closing?RGB(255,255,255):theme.text;
                Gdiplus::Graphics g(item->hDC);g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
                Gdiplus::Pen pen(Gdiplus::Color(255,GetRValue(ink),GetGValue(ink),GetBValue(ink)),a->scale(1)*1.2f);
                pen.SetStartCap(Gdiplus::LineCapRound);pen.SetEndCap(Gdiplus::LineCapRound);
                const float cx=(item->rcItem.left+item->rcItem.right)/2.f;
                const float cy=(item->rcItem.top+item->rcItem.bottom)/2.f;
                const float arm=a->scale(5)*1.f;
                if(closing) {
                    g.DrawLine(&pen,cx-arm,cy-arm,cx+arm,cy+arm);
                    g.DrawLine(&pen,cx-arm,cy+arm,cx+arm,cy-arm);
                }else g.DrawLine(&pen,cx-arm,cy,cx+arm,cy);
                return TRUE;
            }
            const bool disabled=(item->itemState&ODS_DISABLED)!=0;
            auto base=CreateSolidBrush(theme.background);FillRect(item->hDC,&item->rcItem,base);DeleteObject(base);
            const bool hot=a->hovered==item->hwndItem;
            const bool pressed=(item->itemState&ODS_SELECTED)!=0;
            const bool focused=(item->itemState&ODS_FOCUS)!=0;
            const COLORREF fill=pressed?theme.buttonDown:hot?theme.buttonHot:theme.button;
            rounded(item->hDC,item->rcItem,fill,focused?theme.accent:theme.buttonEdge,a->scale(7));
            SetBkMode(item->hDC,TRANSPARENT);SetTextColor(item->hDC,disabled?theme.mutedText:theme.text);SelectObject(item->hDC,a->font);
            wchar_t caption[128];GetWindowTextW(item->hwndItem,caption,128);RECT r=item->rcItem;
            // Icon plus label, centred together: measure the text first so the
            // pair sits in the middle instead of the text alone.
            Icon glyph{};bool hasIcon=true;
            switch(item->CtlID) {
            case Reference: glyph=Icon::Restore;break;
            case SetDefault: glyph=Icon::Pin;break;
            case Route: glyph=Icon::Speaker;break;
            case Settings: glyph=Icon::Gear;break;
            case Detect: glyph=Icon::Chip;break;
            case Measure: glyph=Icon::Mic;break;
            case Load: glyph=Icon::Folder;break;
            case Save: glyph=Icon::Save;break;
            case Refresh: glyph=Icon::Refresh;break;
            default: hasIcon=false;break;
            }
            if(hasIcon) {
                RECT measure=r;DrawTextW(item->hDC,caption,-1,&measure,DT_CALCRECT|DT_SINGLELINE);
                const int glyphSize=a->scale(17),gap=a->scale(8);
                const int total=(measure.right-measure.left)+glyphSize+gap;
                const int left=item->rcItem.left+((item->rcItem.right-item->rcItem.left)-total)/2;
                const int cy=(item->rcItem.top+item->rcItem.bottom)/2;
                drawIcon(item->hDC,glyph,left,cy-glyphSize/2,glyphSize,disabled?theme.mutedText:theme.accent,a->scale(2)*1.f);
                r.left=left+glyphSize+gap;
                DrawTextW(item->hDC,caption,-1,&r,DT_LEFT|DT_VCENTER|DT_SINGLELINE);
                return TRUE;
            }
            DrawTextW(item->hDC,caption,-1,&r,DT_CENTER|DT_VCENTER|DT_SINGLELINE);return TRUE;
        }
        case WM_CTLCOLORSTATIC:case WM_CTLCOLORBTN:case WM_CTLCOLORLISTBOX:SetTextColor(reinterpret_cast<HDC>(w),theme.text);SetBkColor(reinterpret_cast<HDC>(w),theme.card);return reinterpret_cast<LRESULT>(a->cardBrush);
        case WM_SETTINGCHANGE:
            // Windows announces an app-theme switch through this; only follow
            // it when the user asked us to track the system.
            if(l&&!a->prefs.themeMode&&!lstrcmpiW(reinterpret_cast<const wchar_t*>(l),L"ImmersiveColorSet")) a->applyTheme();
            return 0;
        case WM_ERASEBKGND:return 1;
        case WM_PAINT:{PAINTSTRUCT p;HDC dc=BeginPaint(h,&p);RECT r;GetClientRect(h,&r);HDC mem=CreateCompatibleDC(dc);HBITMAP bmp=CreateCompatibleBitmap(dc,r.right,r.bottom);auto old=SelectObject(mem,bmp);a->paint(mem);BitBlt(dc,0,0,r.right,r.bottom,mem,0,0,SRCCOPY);SelectObject(mem,old);DeleteObject(bmp);DeleteDC(mem);EndPaint(h,&p);return 0;}
        case WM_SIZE:if(a->font) a->layout();return 0;
        case WM_DPICHANGED:{const auto* r=reinterpret_cast<RECT*>(l);SetWindowPos(h,nullptr,r->left,r->top,r->right-r->left,r->bottom-r->top,SWP_NOZORDER|SWP_NOACTIVATE);return 0;}
        case WM_NCHITTEST:{
            // No title bar, so the empty strip along the top acts as one:
            // dragging it moves the window. The buttons sit above it as real
            // children, so they get their clicks first.
            POINT p{GET_X_LPARAM(l),GET_Y_LPARAM(l)};ScreenToClient(h,&p);
            RECT r;GetClientRect(h,&r);
            if(p.y>=0&&p.y<a->scale(46)&&p.x>=0&&p.x<r.right) return HTCAPTION;
            return HTCLIENT;}
        case WM_NCCALCSIZE:if(w) return 0;break;
        case TrayMessage:
            if(l==WM_LBUTTONUP||l==WM_LBUTTONDBLCLK) a->showWindow();
            else if(l==WM_RBUTTONUP||l==WM_CONTEXTMENU) a->trayMenu();
            return 0;
        case WM_CLOSE:
            // Closing the window parks the app in the tray and keeps audio
            // running; leaving for good goes through the tray menu or --quit.
            if(!a->selfTest&&!a->audioTest&&!a->quitting&&!a->prefs.closeExits) {
                a->trayAdd();a->trayTip();ShowWindow(h,SW_HIDE);
                try {
                    a->publish();a->profile.raw=SendMessageW(a->raw,BM_GETCHECK,0,0)==BST_CHECKED;
                    dps::saveProfile(a->root/L"config/session.user.dps",a->profile);
                }catch(const std::exception& e) {if(a->telemetry) a->telemetry<<"session_save_error="<<e.what()<<'\n';}
                return 0;
            }
            if(!a->selfTest&&!a->audioTest) {
                try {
                    a->publish();a->profile.raw=SendMessageW(a->raw,BM_GETCHECK,0,0)==BST_CHECKED;
                    const auto index=SendMessageW(a->output,CB_GETCURSEL,0,0);
                    if(index>=0&&static_cast<std::size_t>(index)<a->physical.size()) a->profile.outputId=utf8(a->physical[index].id);
                    dps::saveProfile(a->root/L"config/session.user.dps",a->profile);

                }catch(const std::exception& e) {if(a->telemetry) a->telemetry<<"session_save_error="<<e.what()<<'\n';}
            }
            a->trayRemove();a->session.stop();DestroyWindow(h);return 0;
        case WM_QUERYENDSESSION:
            // A query may be cancelled. Keep the live session until confirmed.
            return TRUE;
        case WM_ENDSESSION:
            if(w) {a->quitting=true;SendMessageW(h,WM_CLOSE,0,0);}
            return 0;
        case WM_DESTROY:PostQuitMessage(0);return 0;
        }
    }catch(const std::exception& e) {a->status=L"Error: "+wide(e.what());if(a->selfTest||a->audioTest) {if(!a->root.empty()) std::ofstream(a->root/L"build/gui-test-result.txt")<<"FAIL: "<<e.what()<<'\n';PostQuitMessage(1);}else MessageBoxW(h,wide(e.what()).c_str(),L"RESONIQ",MB_OK|MB_ICONERROR);}
    return DefWindowProcW(h,message,w,l);
}
}
int WINAPI wWinMain(HINSTANCE instance,HINSTANCE,PWSTR,int show) {
    SetProcessDpiAwarenessContext(DPI_AWARENESS_CONTEXT_PER_MONITOR_AWARE_V2);
    INITCOMMONCONTROLSEX controls{sizeof(controls),ICC_BAR_CLASSES};InitCommonControlsEx(&controls);
    struct GraphicsRuntime {
        ULONG_PTR token=0;
        GraphicsRuntime(){Gdiplus::GdiplusStartupInput input;Gdiplus::GdiplusStartup(&token,&input,nullptr);}
        ~GraphicsRuntime(){if(token) Gdiplus::GdiplusShutdown(token);}
    } graphicsRuntime;
    if(!graphicsRuntime.token) return 1;
    App app;int argc=0;auto argv=CommandLineToArgvW(GetCommandLineW(),&argc);if(argv) {for(int i=1;i<argc;++i) {if(std::wstring_view(argv[i])==L"--self-test") app.selfTest=true;else if(std::wstring_view(argv[i])==L"--audio-test") app.audioTest=true;else if(std::wstring_view(argv[i])==L"--profile"&&i+1<argc) app.requestedProfile=argv[++i];else if(std::wstring_view(argv[i])==L"--data-dir"&&i+1<argc) app.dataRootOverride=argv[++i];else if(std::wstring_view(argv[i])==L"--measurement-diagnostics") app.measurementDiagnostics=true;else if(std::wstring_view(argv[i])==L"--tray") app.startTray=true;}LocalFree(argv);}
    // One copy at a time. Windows may now start the app from either anchor,
    // and two engines on the same device would fight over it. Test runs keep
    // their own data directory, so they stay free to run beside a live copy.
    struct Single {HANDLE handle=nullptr;~Single(){if(handle) CloseHandle(handle);}} single;
    if(!app.selfTest&&!app.audioTest&&app.dataRootOverride.empty()) {
        single.handle=CreateMutexW(nullptr,TRUE,L"Local\\Resoniq.SingleInstance");
        const bool taken=!single.handle||GetLastError()==ERROR_ALREADY_EXISTS;
        if(taken) {
            // Opened by hand while a copy is already running: show that one
            // instead of a second window. A sign-in start stays quiet.
            if(!app.startTray) if(HWND existing=FindWindowW(L"Resoniq.Gui",nullptr)) {
                PostMessageW(existing,WM_COMMAND,TrayShow,0);SetForegroundWindow(existing);
            }
            return 0;
        }
    }
    WNDCLASSW wc{};wc.lpfnWndProc=procedure;wc.hInstance=instance;wc.lpszClassName=L"Resoniq.Gui";wc.hCursor=LoadCursorW(nullptr,IDC_ARROW);wc.hIcon=LoadIconW(instance,MAKEINTRESOURCEW(1));RegisterClassW(&wc);
    // No system title bar and no resizing: the layout is fixed, and the close
    // and minimise buttons are drawn with the rest of the interface.
    // WS_POPUP keeps the frame off; WS_MINIMIZEBOX is still needed so the
    // taskbar entry and the minimise animation behave normally.
    const UINT dpi=GetDpiForSystem();
    const int clientWidth=MulDiv(880,dpi,96),clientHeight=MulDiv(920,dpi,96);
    HWND h=CreateWindowExW(WS_EX_APPWINDOW,wc.lpszClassName,L"Resoniq by Valvik",
        WS_POPUP|WS_MINIMIZEBOX|WS_CLIPCHILDREN,CW_USEDEFAULT,CW_USEDEFAULT,
        clientWidth,clientHeight,nullptr,nullptr,instance,&app);if(!h) return 1;
    SendMessageW(h,WM_SETICON,ICON_SMALL,reinterpret_cast<LPARAM>(LoadImageW(instance,MAKEINTRESOURCEW(1),IMAGE_ICON,GetSystemMetrics(SM_CXSMICON),GetSystemMetrics(SM_CYSMICON),LR_SHARED)));
    BOOL dark=TRUE;DwmSetWindowAttribute(h,DWMWA_USE_IMMERSIVE_DARK_MODE,&dark,sizeof(dark));
    const DWM_WINDOW_CORNER_PREFERENCE corners=DWMWCP_ROUND;
    const auto cornerResult=DwmSetWindowAttribute(h,DWMWA_WINDOW_CORNER_PREFERENCE,&corners,sizeof(corners));
    const DWM_SYSTEMBACKDROP_TYPE backdrop=DWMSBT_MAINWINDOW;
    const auto backdropResult=DwmSetWindowAttribute(h,DWMWA_SYSTEMBACKDROP_TYPE,&backdrop,sizeof(backdrop));
    // Client surfaces remain opaque; system material is visible in the frame.
    std::ofstream(app.root/L"build/gui-platform-result.txt")<<"rounded_window="<<SUCCEEDED(cornerResult)<<" system_backdrop="<<SUCCEEDED(backdropResult)<<'\n';
    // Start parked in the tray when asked for it, or when the preference says
    // so and Windows launched us at sign-in.
    if(!app.selfTest&&!app.audioTest&&(app.startTray||app.prefs.startMinimised)) {
        app.trayAdd();app.trayTip();ShowWindow(h,SW_HIDE);
    }else {ShowWindow(h,show);UpdateWindow(h);}
    MSG msg{};int result=0;
    while((result=static_cast<int>(GetMessageW(&msg,nullptr,0,0)))>0) {
        if(app.settings&&IsDialogMessageW(app.settings,&msg)) continue;
        if(app.about&&IsDialogMessageW(app.about,&msg)) continue;
        if(!IsDialogMessageW(h,&msg)) {TranslateMessage(&msg);DispatchMessageW(&msg);}
    }
    if(result<0) return 1;
    return static_cast<int>(msg.wParam);
}
