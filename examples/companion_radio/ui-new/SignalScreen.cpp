#include "SignalScreen.h"
#include "UITask.h"
#include "UIStyle.h"
#include <stdio.h>

using namespace uistyle;

namespace {
void ageStr(uint32_t now, uint32_t t, char* b, size_t n) {
  if (t == 0) { snprintf(b, n, "stale"); return; }
  uint32_t s = (now >= t) ? now - t : 0;
  if (s < 60) snprintf(b, n, "%us", (unsigned)s);
  else if (s < 3600) snprintf(b, n, "%um", (unsigned)(s / 60));
  else if (s < 86400) snprintf(b, n, "%uh", (unsigned)(s / 3600));
  else snprintf(b, n, "%ud", (unsigned)(s / 86400));
}
int barsFromRssi(int8_t rssi, bool heard) {
  if (!heard || rssi == 0) return 0;
  if (rssi >= -70) return 4;
  if (rssi >= -85) return 3;
  if (rssi >= -100) return 2;
  if (rssi >= -112) return 1;
  return 0;
}
int entryH(DisplayDriver& d) { return big(d) ? 24 : 13; }
}  // namespace

int SignalScreen::render(DisplayDriver& d) {
  int W = d.width(), hH = headerH(d), eh = entryH(d);
  drawHeaderBar(d, "Signal", true);
  col(d, C_HEADER);
  d.fillRect(0, hH, W, d.height() - hH);

  if (_count == 0) {
    col(d, C_LABEL);
    d.drawTextCentered(W / 2, d.height() / 2, "No repeaters known");
    return 1000;
  }

  int vis = (d.height() - hH) / eh;
  if (vis < 1) vis = 1;
  if (_sel < 0) _sel = 0;
  if (_sel > _count - 1) _sel = _count - 1;
  if (_sel < _scroll_top) _scroll_top = _sel;
  if (_sel >= _scroll_top + vis) _scroll_top = _sel - vis + 1;
  int maxTop = (_count > vis) ? _count - vis : 0;
  if (_scroll_top > maxTop) _scroll_top = maxTop;
  if (_scroll_top < 0) _scroll_top = 0;

  bool overflow = _count > vis;
  int right = W - (overflow ? 6 : 0);

  // gauge geometry (right-aligned, before age text)
  int barW = big(d) ? 4 : 3, gap = 2, nbars = 4;
  int gaugeW = nbars * (barW + gap);

  char age[12];
  for (int row = 0; row < vis; row++) {
    int pos = _scroll_top + row;
    if (pos >= _count) break;
    const RepeaterSignal& rp = _list[pos];
    int y = hH + row * eh;
    bool selected = (pos == _sel);

    col(d, selected ? C_CARDSEL : C_CARD);
    d.fillRect(0, y, W, eh - 1);
    col(d, C_DIV);
    d.fillRect(0, y + eh - 1, W, 1);
    if (selected) { col(d, C_ACCENT); d.fillRect(0, y, 3, eh - 1); }

    int ty = y + (eh - 8) / 2;
    ageStr(_now, rp.recv_ts, age, sizeof(age));
    int ageW = d.getTextWidth(age);

    // age (far right)
    col(d, C_LABEL);
    d.drawTextRightAlign(right - 2, ty, age);

    // coverage gauge (left of age)
    int bars = barsFromRssi(rp.rssi, rp.heard);
    int gx = right - 2 - ageW - 6 - gaugeW;
    int baseY = y + eh - 5;
    for (int i = 0; i < nbars; i++) {
      int bh = 3 + i * 3;
      int bxi = gx + i * (barW + gap);
      col(d, (i < bars) ? C_ACCENT : C_MUTED);
      d.fillRect(bxi, baseY - bh, barW, bh);
    }

    // name (left, ellipsized up to the gauge)
    col(d, rp.heard ? C_VALUE : C_MUTED);
    d.drawTextEllipsized(5, ty, gx - 5 - 4, rp.name);
  }

  if (overflow) {
    int trackX = W - 4, trackY = hH, trackH = vis * eh;
    col(d, C_DIV);
    d.fillRect(trackX, trackY, 2, trackH);
    int thumbH = trackH * vis / _count;
    if (thumbH < 8) thumbH = 8;
    int denom = maxTop > 0 ? maxTop : 1;
    int thumbY = trackY + (trackH - thumbH) * _scroll_top / denom;
    col(d, C_ACCENT);
    d.fillRect(trackX, thumbY, 2, thumbH);
  }
  return 1000;
}

bool SignalScreen::handleInput(char c) {
  if (c == KEY_UP || c == KEY_PREV) { if (_sel > 0) _sel--; return true; }
  if (c == KEY_DOWN || c == KEY_NEXT) { if (_sel < _count - 1) _sel++; return true; }
  if (c == KEY_CANCEL || c == KEY_LEFT) { _task->gotoHomeScreen(); return true; }
  return false;
}

bool SignalScreen::handleTouch(int x, int y, TouchEvent ev) {
  DisplayDriver* d = _task->getDisplay();
  if (!d) return false;
  int hH = headerH(*d), eh = entryH(*d);
  if (ev == TouchEvent::press) { _last_y = y; _moved = false; _pressing = true; return true; }
  if (ev == TouchEvent::move) {
    if (!_pressing) return false;
    while (y - _last_y >= eh) { if (_scroll_top > 0) _scroll_top--; _last_y += eh; _moved = true; }
    while (_last_y - y >= eh) { _scroll_top++; _last_y -= eh; _moved = true; }
    return true;
  }
  if (ev == TouchEvent::release) {
    _pressing = false;
    if (_moved) return true;
    if (y < hH) { _task->gotoHomeScreen(); return true; }
    int pos = _scroll_top + (y - hH) / eh;
    if (pos >= 0 && pos < _count) _sel = pos;
    return true;
  }
  return false;
}
