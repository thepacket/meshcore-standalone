#pragma once

// Shared visual style for the standalone UI screens (settings, packet monitor,
// and future chat screens), so they look consistent. Colors are set via
// setColorRGB(), which degrades to the nearest palette color on mono displays.
#include <helpers/ui/DisplayDriver.h>

namespace uistyle {

struct RGB { uint8_t r, g, b; };
constexpr RGB C_HEADER  = {38, 48, 63};    // navy header bar
constexpr RGB C_CARD    = {42, 46, 53};    // row background
constexpr RGB C_CARDSEL = {56, 62, 74};    // selected row background
constexpr RGB C_DIV     = {64, 68, 78};    // dividers / outlines
constexpr RGB C_LABEL   = {150, 160, 172}; // secondary text
constexpr RGB C_VALUE   = {236, 238, 242}; // primary text
constexpr RGB C_ACCENT  = {91, 141, 239};  // blue accent
constexpr RGB C_MUTED   = {120, 128, 140}; // affordance glyphs

inline void col(DisplayDriver& d, const RGB& c) { d.setColorRGB(c.r, c.g, c.b); }
inline bool big(DisplayDriver& d) { return d.height() >= 160; }
inline int headerH(DisplayDriver& d) { return big(d) ? 28 : 14; }
inline int rowH(DisplayDriver& d) { return big(d) ? 30 : 13; }
inline int visibleRows(DisplayDriver& d) {
  int vr = (d.height() - headerH(d)) / rowH(d);
  return vr < 1 ? 1 : vr;
}
inline bool inRect(int x, int y, int rx, int ry, int rw, int rh) {
  return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}

// small affordance triangles
inline void triRight(DisplayDriver& d, int xLeft, int cy, int half) {
  for (int i = 0; i <= half; i++) d.fillRect(xLeft + i, cy - (half - i), 1, 2 * (half - i) + 1);
}
inline void triLeft(DisplayDriver& d, int xLeft, int cy, int half) {
  for (int i = 0; i <= half; i++) d.fillRect(xLeft + i, cy - i, 1, 2 * i + 1);
}
inline void triDown(DisplayDriver& d, int cx, int yTop, int half) {
  for (int i = 0; i <= half; i++) d.fillRect(cx - half + i, yTop + i, 2 * (half - i) + 1, 1);
}

inline void drawHeaderBar(DisplayDriver& d, const char* title, bool back) {
  int hH = headerH(d), W = d.width();
  col(d, C_HEADER);
  d.fillRect(0, 0, W, hH);
  int titleX = 4;
  if (back) {
    col(d, C_VALUE);
    triLeft(d, 5, hH / 2, big(d) ? 5 : 3);
    titleX = big(d) ? 20 : 12;
  }
  col(d, C_VALUE);
  d.setTextSize(big(d) ? 2 : 1);
  d.setCursor(titleX, (hH - (big(d) ? 14 : 8)) / 2);
  d.print(title);
  d.setTextSize(1);
}

}  // namespace uistyle
