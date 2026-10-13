#pragma once
#include <windows.h>
namespace resoniq::ui {
// Line icons drawn with GDI+, in the current palette colour, so they follow the
// theme without a second set of assets. Each one draws inside a square box.
enum class Icon { Intensity, Bass, Clarity, Dynamics, Spatial, Restore, Pin, Speaker, Gear, Folder, Save, Refresh, Chip, Mic };
void drawIcon(HDC dc,Icon icon,int x,int y,int size,COLORREF colour,float weight);
}
