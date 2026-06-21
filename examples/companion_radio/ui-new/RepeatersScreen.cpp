#include "RepeatersScreen.h"
#include "UITask.h"
#include "UIStyle.h"
#include <stdio.h>
#include <string.h>

#ifndef ADV_TYPE_ROOM
  #define ADV_TYPE_ROOM 3   // avoid pulling the Arduino-only AdvertDataHelpers.h into the sim
#endif

using namespace uistyle;

namespace {
int entryH(DisplayDriver& d) { return big(d) ? 22 : 13; }
int tabH(DisplayDriver& d)   { return big(d) ? 20 : 12; }
const char* typeTag(uint8_t t) { return t == ADV_TYPE_ROOM ? "ROOM" : "RPT"; }
}

void RepeatersScreen::addSaved(const uint8_t* pk, const char* name, uint8_t type, bool fav) {
  if (_nsaved >= MAXE) return;
  NodeEntry& e = _saved[_nsaved++];
  memcpy(e.pubkey, pk, 6); e.type = type; e.fav = fav;
  strncpy(e.name, name, sizeof(e.name) - 1); e.name[sizeof(e.name) - 1] = 0;
}
void RepeatersScreen::addScan(const uint8_t* pk, const char* name, uint8_t type) {
  if (_nscan >= MAXE) return;
  NodeEntry& e = _scan[_nscan++];
  memcpy(e.pubkey, pk, 6); e.type = type; e.fav = false;
  strncpy(e.name, name, sizeof(e.name) - 1); e.name[sizeof(e.name) - 1] = 0;
}

NodeEntry* RepeatersScreen::curList(int& n) {
  if (_tab == 0) { n = _nsaved; return _saved; }
  n = _nscan; return _scan;
}

void RepeatersScreen::activate(int pos) {
  int n; NodeEntry* list = curList(n);
  if (pos < 0 || pos >= n) return;
  NodeEntry& e = list[pos];
  if (_tab == 0) _task->openRepeater(e.pubkey, e.name, e.type);
  else _task->addCandidate(e.pubkey);
}

int RepeatersScreen::render(DisplayDriver& d) {
  int W = d.width(), hH = headerH(d), th = tabH(d), eh = entryH(d);
  drawHeaderBar(d, "Repeaters", true);

  const char* labels[2] = {"Saved", "Scan"};
  int ty = hH;
  for (int t = 0; t < 2; t++) {
    int tx = t * (W / 2);
    col(d, _tab == t ? C_CARDSEL : C_CARD);
    d.fillRect(tx, ty, W / 2, th);
    col(d, _tab == t ? C_ACCENT : C_LABEL);
    d.setTextSize(1);
    d.drawTextCentered(tx + W / 4, ty + (th - 8) / 2, labels[t]);
    if (_tab == t) { col(d, C_ACCENT); d.fillRect(tx, ty + th - 2, W / 2, 2); }
  }
  col(d, C_DIV); d.fillRect(0, ty + th, W, 1);

  int listY = hH + th + 1;
  col(d, C_HEADER); d.fillRect(0, listY, W, d.height() - listY);

  int n; NodeEntry* list = curList(n);
  int& sel = _sel[_tab]; int& top = _top[_tab];

  if (n == 0) {
    col(d, C_LABEL);
    d.drawTextCentered(W / 2, listY + (d.height() - listY) / 2 - 4,
                       _tab == 0 ? "No repeaters saved" : "Scanning... none heard");
    return 1000;
  }

  int vis = (d.height() - listY) / eh;
  if (vis < 1) vis = 1;
  if (sel < 0) sel = 0; if (sel > n - 1) sel = n - 1;
  if (sel < top) top = sel;
  if (sel >= top + vis) top = sel - vis + 1;
  if (top < 0) top = 0;

  for (int row = 0; row < vis; row++) {
    int pos = top + row;
    if (pos >= n) break;
    NodeEntry& e = list[pos];
    int y = listY + row * eh;
    bool seld = (pos == sel);
    col(d, seld ? C_CARDSEL : C_CARD);
    d.fillRect(0, y, W, eh - 1);
    col(d, C_DIV); d.fillRect(0, y + eh - 1, W, 1);
    if (seld) { col(d, C_ACCENT); d.fillRect(0, y, 3, eh - 1); }

    int ty2 = y + (eh - 8) / 2;
    // right: type tag, plus a star for favourites or "+add" hint for scan
    char right[16];
    if (_tab == 0) snprintf(right, sizeof(right), "%s%s", e.fav ? "* " : "", typeTag(e.type));
    else snprintf(right, sizeof(right), "+ %s", typeTag(e.type));
    int rw = d.getTextWidth(right);
    col(d, e.fav ? C_BADGE : C_LABEL);
    d.drawTextRightAlign(W - 6, ty2, right);
    col(d, C_VALUE);
    d.drawTextEllipsized(6, ty2, W - 12 - rw, e.name);
  }
  return 1000;
}

bool RepeatersScreen::handleInput(char c) {
  int n; NodeEntry* list = curList(n); (void)list;
  int& sel = _sel[_tab];
  if (c == KEY_LEFT) { if (_tab == 1) _tab = 0; else _task->gotoHomeScreen(); return true; }
  if (c == KEY_RIGHT) { _tab = 1; return true; }
  if (c == KEY_UP || c == KEY_PREV) { if (sel > 0) sel--; return true; }
  if (c == KEY_DOWN || c == KEY_NEXT) { if (sel < n - 1) sel++; return true; }
  if (c == KEY_ENTER || c == KEY_SELECT) { activate(sel); return true; }
  if (c == KEY_CANCEL) { _task->gotoHomeScreen(); return true; }
  return false;
}

bool RepeatersScreen::handleTouch(int x, int y, TouchEvent ev) {
  DisplayDriver* d = _task->getDisplay();
  if (!d) return false;
  int W = d->width(), hH = headerH(*d), th = tabH(*d), eh = entryH(*d);
  int listY = hH + th + 1;
  int& sel = _sel[_tab]; int& top = _top[_tab];

  if (ev == TouchEvent::press) { _last_y = y; _moved = false; _pressing = true; return true; }
  if (ev == TouchEvent::move) {
    if (!_pressing || y < listY) return _pressing;
    while (y - _last_y >= eh) { if (top > 0) top--; _last_y += eh; _moved = true; }
    while (_last_y - y >= eh) { top++; _last_y -= eh; _moved = true; }
    return true;
  }
  if (ev == TouchEvent::release) {
    _pressing = false;
    if (_moved) return true;
    if (y < hH) { _task->gotoHomeScreen(); return true; }
    if (y < hH + th) { _tab = (x < W / 2) ? 0 : 1; return true; }
    int n; curList(n);
    int pos = top + (y - listY) / eh;
    if (pos >= 0 && pos < n) { sel = pos; activate(pos); }
    return true;
  }
  return false;
}
