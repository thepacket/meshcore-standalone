#include "HomeLauncherScreen.h"
#include "UITask.h"
#include "UIStyle.h"
#include "TileIcons.h"
#include <stdio.h>
#include <string.h>

using namespace uistyle;

namespace {
const char* TILE_LABELS[HomeLauncherScreen::A_COUNT] = {
  "Chat", "Contacts", "Repeaters", "Finder",
  "Heard", "Map", "Advert", "Settings",
  "Trace", "Terminal", "Noise", "Signal",
};
const uint8_t* TILE_ICONS[HomeLauncherScreen::A_COUNT] = {
  icons::chat, icons::contacts, icons::repeaters, icons::finder,
  icons::heard, icons::map, icons::advertise, icons::settings,
  icons::trace, icons::terminal, icons::noise, icons::signal,
};
bool tileEnabled(int t) {
  switch (t) {
    case HomeLauncherScreen::A_CHAT:
    case HomeLauncherScreen::A_HEARD:
    case HomeLauncherScreen::A_ADVERTISE:
    case HomeLauncherScreen::A_SETTINGS:
    case HomeLauncherScreen::A_TRACE:
    case HomeLauncherScreen::A_TERMINAL:
    case HomeLauncherScreen::A_NOISE:
    case HomeLauncherScreen::A_SIGNAL:
      return true;
    default:
      return false;
  }
}
const char* MENU_ITEMS[] = {"Send advert", "Bluetooth", "Reboot", "Shutdown"};
const int MENU_COUNT = 4;
int bottomBarH(DisplayDriver& d) { return big(d) ? 16 : 12; }
}  // namespace

int HomeLauncherScreen::tileAt(int px, int py) {
  DisplayDriver* d = _task->getDisplay();
  if (!d) return -1;
  int W = d->width(), H = d->height();
  int gy = headerH(*d), gh = H - gy - bottomBarH(*d);
  if (py < gy || py >= gy + gh) return -1;
  int cw = W / 4, rh = gh / 3;
  int c = px / cw, r = (py - gy) / rh;
  if (c < 0 || c > 3 || r < 0 || r > 2) return -1;
  int t = r * 4 + c;
  return (t < A_COUNT) ? t : -1;
}

void HomeLauncherScreen::drawTopBar(DisplayDriver& d, const HomeStatus& s) {
  int W = d.width(), hH = headerH(d);
  col(d, C_HEADER);
  d.fillRect(0, 0, W, hH);
  int midY = (hH - 8) / 2;
  // hamburger menu glyph (left)
  col(d, C_ACCENT);
  for (int i = 0; i < 3; i++) d.fillRect(6, hH / 2 - 4 + i * 4, 14, 2);
  // channel (center)
  d.setTextSize(1);
  col(d, C_VALUE);
  d.drawTextCentered(W / 2, midY, s.channel ? s.channel : "MeshCore");
  // clock (right)
  char clk[8];
  if (s.epoch > 0) snprintf(clk, sizeof(clk), "%02u:%02u",
                            (unsigned)((s.epoch / 3600) % 24), (unsigned)((s.epoch / 60) % 60));
  else snprintf(clk, sizeof(clk), "--:--");
  col(d, C_LABEL);
  d.drawTextRightAlign(W - 6, midY, clk);
  // cyan accent underline
  col(d, C_ACCENT);
  d.fillRect(0, hH - 1, W, 1);
}

void HomeLauncherScreen::drawBottomBar(DisplayDriver& d, const HomeStatus& s) {
  int W = d.width(), H = d.height(), bH = bottomBarH(d);
  int y0 = H - bH;
  col(d, C_HEADER);
  d.fillRect(0, y0, W, bH);
  col(d, C_ACCENT);
  d.fillRect(0, y0, W, 1);
  int ty = y0 + (bH - 8) / 2;
  // node + device (left)
  char left[48];
  snprintf(left, sizeof(left), "%s %s", s.node_name ? s.node_name : "node",
           s.device ? s.device : "");
  col(d, C_VALUE);
  d.drawTextLeftAlign(6, ty, left);
  // battery (far right)
  int bw = 18, bh = 9, bx = W - bw - 4, by = y0 + (bH - bh) / 2;
  bool low = (s.batt_pct >= 0 && s.batt_pct < 20);
  col(d, low ? C_WARN : C_VALUE);
  d.drawRect(bx, by, bw, bh);
  d.fillRect(bx + bw, by + 2, 2, bh - 4);  // terminal nub
  if (s.batt_pct >= 0) {
    int fillw = (bw - 2) * s.batt_pct / 100;
    if (fillw > 0) d.fillRect(bx + 1, by + 1, fillw, bh - 2);
  }
  // signal bars (left of battery)
  int barW = 3, gap = 1, n = 4;
  int sx = bx - 6 - (n * (barW + gap));
  for (int i = 0; i < n; i++) {
    int barH = 3 + i * 2;
    int bxi = sx + i * (barW + gap);
    int byi = (y0 + bH - 3) - barH;
    col(d, (i < s.bars) ? C_ACCENT : C_MUTED);
    d.fillRect(bxi, byi, barW, barH);
  }
}

void HomeLauncherScreen::drawMenu(DisplayDriver& d) {
  int W = d.width(), H = d.height();
  int itemH = big(d) ? 24 : 14;
  int boxW = W * 3 / 4, boxH = MENU_COUNT * itemH + 6;
  int bx = (W - boxW) / 2, by = (H - boxH) / 2;
  col(d, C_CARD);
  d.fillRect(bx, by, boxW, boxH);
  col(d, C_ACCENT);
  d.drawRect(bx, by, boxW, boxH);
  d.setTextSize(1);
  for (int i = 0; i < MENU_COUNT; i++) {
    int iy = by + 3 + i * itemH;
    if (i == _menu_sel) {
      col(d, C_CARDSEL);
      d.fillRect(bx + 2, iy, boxW - 4, itemH - 1);
      col(d, C_ACCENT);
      d.fillRect(bx + 2, iy, 3, itemH - 1);
    }
    col(d, C_VALUE);
    d.drawTextCentered(W / 2, iy + (itemH - 8) / 2, MENU_ITEMS[i]);
  }
}

int HomeLauncherScreen::render(DisplayDriver& d) {
  HomeStatus s;
  memset(&s, 0, sizeof(s));
  s.batt_pct = -1;
  _task->getHomeStatus(s);

  int W = d.width(), H = d.height();
  int gy = headerH(d), gh = H - gy - bottomBarH(d);
  int cw = W / 4, rh = gh / 3;

  // clear the grid area to black (C_HEADER is black in this theme)
  col(d, C_HEADER);
  d.fillRect(0, gy, W, gh);

  for (int t = 0; t < A_COUNT; t++) {
    int r = t / 4, c = t % 4;
    int x = c * cw, y = gy + r * rh;
    bool en = tileEnabled(t);
    bool selected = (t == _sel);
    if (selected) {
      col(d, C_CARDSEL);
      d.fillRect(x + 3, y + 3, cw - 6, rh - 6);
      col(d, C_ACCENT);
      d.drawRect(x + 2, y + 2, cw - 4, rh - 4);
    }
    int icx = x + (cw - icons::ICON_W) / 2;
    int icy = y + (rh - icons::ICON_H - 12) / 2;
    if (icy < y + 2) icy = y + 2;
    col(d, en ? C_ACCENT : C_MUTED);
    d.drawXbm(icx, icy, TILE_ICONS[t], icons::ICON_W, icons::ICON_H);
    col(d, en ? C_VALUE : C_MUTED);
    d.setTextSize(1);
    d.drawTextCentered(x + cw / 2, icy + icons::ICON_H + 2, TILE_LABELS[t]);
    // unread badge on Chat
    if (t == A_CHAT && s.msgcount > 0) {
      char cnt[6];
      snprintf(cnt, sizeof(cnt), "%d", s.msgcount > 99 ? 99 : s.msgcount);
      int br = 7, bcx = x + cw - 12, bcy = y + 12;
      col(d, C_BADGE);
      d.fillRect(bcx - br, bcy - br, 2 * br, 2 * br);
      col(d, C_VALUE);
      d.drawTextCentered(bcx, bcy - 4, cnt);
    }
  }

  drawTopBar(d, s);
  drawBottomBar(d, s);
  if (_menu_open) drawMenu(d);
  return 1000;  // refresh ~1s so the clock ticks
}

void HomeLauncherScreen::activate(int tile) {
  switch (tile) {
    case A_CHAT:      _task->gotoChat(); break;
    case A_HEARD:     _task->gotoHeard(); break;
    case A_ADVERTISE: _task->doAdvertise(); break;
    case A_SETTINGS:  _task->gotoSettings(); break;
    case A_TRACE:     _task->gotoTrace(); break;
    case A_TERMINAL:  _task->gotoPacketMonitor(); break;
    case A_NOISE:     _task->gotoNoise(); break;
    case A_SIGNAL:    _task->gotoSignal(); break;
    default:          _task->showAlert("Coming soon", 1200); break;
  }
}

void HomeLauncherScreen::activateMenu(int item) {
  _menu_open = false;
  switch (item) {
    case 0: _task->doAdvertise(); break;
    case 1: _task->toggleBluetooth(); break;
    case 2: _task->shutdown(true); break;   // reboot
    case 3: _task->shutdown(false); break;  // shutdown
  }
}

bool HomeLauncherScreen::handleInput(char c) {
  if (_menu_open) {
    if (c == KEY_UP || c == KEY_PREV) { if (_menu_sel > 0) _menu_sel--; return true; }
    if (c == KEY_DOWN || c == KEY_NEXT) { if (_menu_sel < MENU_COUNT - 1) _menu_sel++; return true; }
    if (c == KEY_ENTER || c == KEY_SELECT) { activateMenu(_menu_sel); return true; }
    if (c == KEY_CANCEL || c == KEY_LEFT) { _menu_open = false; return true; }
    return true;
  }
  if (c == KEY_UP || c == KEY_PREV) { if (_sel >= 4) _sel -= 4; return true; }
  if (c == KEY_DOWN || c == KEY_NEXT) { if (_sel + 4 < A_COUNT) _sel += 4; return true; }
  if (c == KEY_LEFT) { if (_sel % 4 > 0) _sel--; return true; }
  if (c == KEY_RIGHT) { if (_sel % 4 < 3 && _sel + 1 < A_COUNT) _sel++; return true; }
  if (c == KEY_ENTER || c == KEY_SELECT) { activate(_sel); return true; }
  if (c == KEY_CONTEXT_MENU) { _menu_open = true; _menu_sel = 0; return true; }
  return false;
}

bool HomeLauncherScreen::handleTouch(int x, int y, TouchEvent ev) {
  DisplayDriver* d = _task->getDisplay();
  if (!d) return false;
  if (ev != TouchEvent::release) return true;  // act on tap completion

  if (_menu_open) {
    int W = d->width(), H = d->height();
    int itemH = big(*d) ? 24 : 14;
    int boxW = W * 3 / 4, boxH = MENU_COUNT * itemH + 6;
    int bx = (W - boxW) / 2, by = (H - boxH) / 2;
    if (!inRect(x, y, bx, by, boxW, boxH)) { _menu_open = false; return true; }
    int item = (y - (by + 3)) / itemH;
    if (item >= 0 && item < MENU_COUNT) { _menu_sel = item; activateMenu(item); }
    return true;
  }
  // tap the hamburger region (top-left of the header)
  if (y < headerH(*d) && x < 40) { _menu_open = true; _menu_sel = 0; return true; }
  int t = tileAt(x, y);
  if (t >= 0) { _sel = t; activate(t); }
  return true;
}
