#pragma once
#include <windows.h>
namespace resoniq::ui {
// Palette. The whole interface draws from these, so switching theme is a
// matter of swapping the values and repainting; nothing else needs to know.
struct Palette {
    COLORREF background,card,text,mutedText,accent;
    COLORREF rail,border,meter,surface,surfaceHot,knobEdge,pillOff,pillEdge,panelEdge,warm;
    COLORREF cardEdge,divider,wave,button,buttonHot,buttonDown,buttonEdge,toggleOn,toggleHot,toggleOff,toggleEdge,accentDown,onAccent;
};
constexpr Palette darkPalette{
    RGB(13,16,22),RGB(23,28,37),RGB(231,238,246),RGB(152,168,185),RGB(45,129,255),
    RGB(62,72,89),RGB(57,64,78),RGB(44,56,72),RGB(29,35,45),RGB(39,46,59),
    RGB(68,81,102),RGB(54,61,72),RGB(74,84,101),RGB(45,53,66),RGB(243,183,79),
    RGB(40,46,58),RGB(42,49,62),RGB(26,52,87),RGB(30,36,47),RGB(43,50,64),RGB(24,29,38),RGB(56,64,80),
    RGB(20,48,88),RGB(28,69,124),RGB(30,36,47),RGB(66,74,91),RGB(32,96,200),RGB(255,255,255)
};
constexpr Palette lightPalette{
    RGB(243,245,249),RGB(255,255,255),RGB(26,32,44),RGB(95,110,130),RGB(0,95,204),
    RGB(205,212,224),RGB(199,206,219),RGB(214,221,232),RGB(255,255,255),RGB(236,240,246),
    RGB(176,186,202),RGB(198,205,217),RGB(176,186,202),RGB(222,228,238),RGB(176,110,10),
    RGB(222,228,238),RGB(226,231,240),RGB(214,230,250),RGB(255,255,255),RGB(238,243,250),RGB(226,232,241),RGB(186,195,210),
    RGB(214,232,252),RGB(198,222,250),RGB(240,243,248),RGB(170,184,204),RGB(0,78,170),RGB(255,255,255)
};
bool systemPrefersLight();
}
