#include "gui_icons.hpp"
#include <objidl.h>
#include <gdiplus.h>
#include <algorithm>
#include <cmath>
namespace resoniq::ui {
void drawIcon(HDC dc,Icon icon,int x,int y,int size,COLORREF colour,float weight) {
    Gdiplus::Graphics g(dc);
    g.SetSmoothingMode(Gdiplus::SmoothingModeAntiAlias);
    Gdiplus::Pen pen(Gdiplus::Color(255,GetRValue(colour),GetGValue(colour),GetBValue(colour)),weight);
    pen.SetStartCap(Gdiplus::LineCapRound);pen.SetEndCap(Gdiplus::LineCapRound);
    pen.SetLineJoin(Gdiplus::LineJoinRound);
    Gdiplus::SolidBrush brush(Gdiplus::Color(255,GetRValue(colour),GetGValue(colour),GetBValue(colour)));
    const float fx=static_cast<float>(x),fy=static_cast<float>(y),s=static_cast<float>(size);
    auto px=[&](float u){return fx+u*s;};
    auto py=[&](float v){return fy+v*s;};
    switch(icon) {
    case Icon::Intensity: {
        // Rising bars: the amount of processing in the mix.
        const float h[4]{.30f,.50f,.72f,.92f};
        for(int i=0;i<4;++i) g.DrawLine(&pen,px(.16f+i*.22f),py(.88f),px(.16f+i*.22f),py(.88f-h[i]*.76f));
        break;}
    case Icon::Bass: {
        // A slow wave: low frequency body.
        Gdiplus::PointF wave[9];
        for(int i=0;i<9;++i) {
            const float t=i/8.0f;
            wave[i]=Gdiplus::PointF(px(.08f+t*.84f),py(.5f-0.30f*static_cast<float>(sin(t*6.2831853))));
        }
        g.DrawCurve(&pen,wave,9);
        break;}
    case Icon::Clarity: {
        // A spark: presence and detail.
        g.DrawLine(&pen,px(.5f),py(.08f),px(.5f),py(.30f));
        g.DrawLine(&pen,px(.5f),py(.70f),px(.5f),py(.92f));
        g.DrawLine(&pen,px(.08f),py(.5f),px(.30f),py(.5f));
        g.DrawLine(&pen,px(.70f),py(.5f),px(.92f),py(.5f));
        g.DrawLine(&pen,px(.20f),py(.20f),px(.35f),py(.35f));
        g.DrawLine(&pen,px(.65f),py(.65f),px(.80f),py(.80f));
        g.DrawLine(&pen,px(.80f),py(.20f),px(.65f),py(.35f));
        g.DrawLine(&pen,px(.35f),py(.65f),px(.20f),py(.80f));
        g.FillEllipse(&brush,px(.40f),py(.40f),s*.20f,s*.20f);
        break;}
    case Icon::Dynamics: {
        // A level meter with its needle: control over dynamics.
        // A tall signal squeezed between two limits: dynamic control.
        g.DrawLine(&pen,px(.10f),py(.22f),px(.90f),py(.22f));
        g.DrawLine(&pen,px(.10f),py(.78f),px(.90f),py(.78f));
        g.DrawLine(&pen,px(.30f),py(.34f),px(.30f),py(.66f));
        g.DrawLine(&pen,px(.50f),py(.26f),px(.50f),py(.74f));
        g.DrawLine(&pen,px(.70f),py(.38f),px(.70f),py(.62f));
        break;}
    case Icon::Spatial: {
        // Arrows outwards: stereo width.
        g.DrawLine(&pen,px(.06f),py(.5f),px(.94f),py(.5f));
        g.DrawLine(&pen,px(.06f),py(.5f),px(.24f),py(.30f));
        g.DrawLine(&pen,px(.06f),py(.5f),px(.24f),py(.70f));
        g.DrawLine(&pen,px(.94f),py(.5f),px(.76f),py(.30f));
        g.DrawLine(&pen,px(.94f),py(.5f),px(.76f),py(.70f));
        break;}
    case Icon::Restore: {
        // Arrow curving back: return to the saved setting.
        g.DrawArc(&pen,px(.16f),py(.16f),s*.68f,s*.68f,40.f,280.f);
        g.DrawLine(&pen,px(.84f),py(.42f),px(.84f),py(.18f));
        g.DrawLine(&pen,px(.84f),py(.42f),px(.60f),py(.42f));
        break;}
    case Icon::Pin: {
        // A bookmark: fix the current sound as the default.
        g.DrawLine(&pen,px(.24f),py(.12f),px(.76f),py(.12f));
        g.DrawLine(&pen,px(.24f),py(.12f),px(.24f),py(.88f));
        g.DrawLine(&pen,px(.76f),py(.12f),px(.76f),py(.88f));
        g.DrawLine(&pen,px(.24f),py(.88f),px(.50f),py(.62f));
        g.DrawLine(&pen,px(.76f),py(.88f),px(.50f),py(.62f));
        break;}
    case Icon::Speaker: {
        // Speaker with a wave: the Windows sound settings.
        g.DrawLine(&pen,px(.14f),py(.38f),px(.30f),py(.38f));
        g.DrawLine(&pen,px(.14f),py(.38f),px(.14f),py(.62f));
        g.DrawLine(&pen,px(.14f),py(.62f),px(.30f),py(.62f));
        g.DrawLine(&pen,px(.30f),py(.38f),px(.50f),py(.18f));
        g.DrawLine(&pen,px(.30f),py(.62f),px(.50f),py(.82f));
        g.DrawLine(&pen,px(.50f),py(.18f),px(.50f),py(.82f));
        g.DrawArc(&pen,px(.52f),py(.30f),s*.26f,s*.40f,-60.f,120.f);
        g.DrawArc(&pen,px(.58f),py(.18f),s*.34f,s*.64f,-60.f,120.f);
        break;}
    case Icon::Gear: {
        // Settings.
        // Cog outline: alternating radius, so it reads as a gear and not a sun.
        Gdiplus::PointF cog[48];
        for(int i=0;i<48;++i) {
            const float a=static_cast<float>(i)*0.1308997f;
            const float radius=((i/3)%2)?0.30f:0.46f;
            cog[i]=Gdiplus::PointF(px(.5f+static_cast<float>(cos(a))*radius),
                                   py(.5f+static_cast<float>(sin(a))*radius));
        }
        g.DrawPolygon(&pen,cog,48);
        g.DrawEllipse(&pen,px(.39f),py(.39f),s*.22f,s*.22f);
        break;}
    case Icon::Folder: {
        g.DrawLine(&pen,px(.10f),py(.30f),px(.42f),py(.30f));
        g.DrawLine(&pen,px(.42f),py(.30f),px(.50f),py(.42f));
        g.DrawLine(&pen,px(.50f),py(.42f),px(.90f),py(.42f));
        g.DrawLine(&pen,px(.10f),py(.30f),px(.10f),py(.78f));
        g.DrawLine(&pen,px(.90f),py(.42f),px(.90f),py(.78f));
        g.DrawLine(&pen,px(.10f),py(.78f),px(.90f),py(.78f));
        break;}
    case Icon::Save: {
        g.DrawLine(&pen,px(.14f),py(.16f),px(.72f),py(.16f));
        g.DrawLine(&pen,px(.72f),py(.16f),px(.86f),py(.30f));
        g.DrawLine(&pen,px(.86f),py(.30f),px(.86f),py(.84f));
        g.DrawLine(&pen,px(.14f),py(.16f),px(.14f),py(.84f));
        g.DrawLine(&pen,px(.14f),py(.84f),px(.86f),py(.84f));
        g.DrawLine(&pen,px(.32f),py(.16f),px(.32f),py(.42f));
        g.DrawLine(&pen,px(.68f),py(.16f),px(.68f),py(.42f));
        g.DrawLine(&pen,px(.32f),py(.42f),px(.68f),py(.42f));
        break;}
    case Icon::Refresh: {
        // Circular arrow: the arc stops short of a full turn and the head sits
        // on the open end, which is what makes it read as "again".
        g.DrawArc(&pen,px(.16f),py(.16f),s*.68f,s*.68f,-70.f,300.f);
        g.DrawLine(&pen,px(.62f),py(.10f),px(.80f),py(.20f));
        g.DrawLine(&pen,px(.80f),py(.20f),px(.64f),py(.33f));
        break;}
    case Icon::Chip: {
        // A chip with its pins: the machine itself, for hardware detection.
        g.DrawRectangle(&pen,px(.26f),py(.26f),s*.48f,s*.48f);
        g.DrawRectangle(&pen,px(.40f),py(.40f),s*.20f,s*.20f);
        for(int i=0;i<3;++i) {
            const float at=.34f+static_cast<float>(i)*.16f;
            g.DrawLine(&pen,px(at),py(.26f),px(at),py(.12f));
            g.DrawLine(&pen,px(at),py(.74f),px(at),py(.88f));
            g.DrawLine(&pen,px(.26f),py(at),px(.12f),py(at));
            g.DrawLine(&pen,px(.74f),py(at),px(.88f),py(at));
        }
        break;}
    case Icon::Mic: {
        // Microphone on its stand: the acoustic measurement.
        g.DrawArc(&pen,px(.38f),py(.08f),s*.24f,s*.44f,0.f,360.f);
        g.DrawArc(&pen,px(.26f),py(.34f),s*.48f,s*.40f,0.f,180.f);
        g.DrawLine(&pen,px(.50f),py(.74f),px(.50f),py(.88f));
        g.DrawLine(&pen,px(.34f),py(.88f),px(.66f),py(.88f));
        break;}
    }
}

}
