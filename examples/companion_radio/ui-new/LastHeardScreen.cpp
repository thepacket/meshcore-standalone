#include "LastHeardScreen.h"
#include "UITask.h"
#include "UIStyle.h"
#include <stdio.h>
#include <string.h>

using namespace uistyle;

namespace {
void ageStr(uint32_t now, uint32_t t, char* b, size_t n) {
  uint32_t s = (now >= t && t > 0) ? now - t : 0;
  if (t == 0) snprintf(b, n, "-");
  else if (s < 60) snprintf(b, n, "%us", (unsigned)s);
  else if (s < 3600) snprintf(b, n, "%um", (unsigned)(s / 60));
  else snprintf(b, n, "%uh", (unsigned)(s / 3600));
}
void snrStr(int8_t snr_q, char* b, size_t n) {
  int q = snr_q;
  const char* sign = (q < 0) ? "-" : "";
  int a = q < 0 ? -q : q;
  snprintf(b, n, "%s%d.%d", sign, a / 4, (a % 4) * 25 / 10);
}
void distStr(int32_t m, char* b, size_t n) {
  if (m < 0) snprintf(b, n, "n/a");
  else if (m < 1000) snprintf(b, n, "%dm", (int)m);
  else snprintf(b, n, "%d.%dkm", (int)(m / 1000), (int)((m % 1000) / 100));
}
int entryH(DisplayDriver& d) { return big(d) ? 22 : 12; }
}  // namespace

int LastHeardScreen::render(DisplayDriver& d) {
  return _detail ? renderDetail(d) : renderList(d);
}

int LastHeardScreen::renderList(DisplayDriver& d) {
  int W = d.width(), hH = headerH(d), eh = entryH(d);
  drawHeaderBar(d, "Heard", true);
  col(d, C_HEADER);
  d.fillRect(0, hH, W, d.height() - hH);

  if (_count == 0) {
    col(d, C_LABEL);
    d.drawTextCentered(W / 2, d.height() / 2, "Nothing heard yet");
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

  char snr[12], age[12], meta[40];
  for (int row = 0; row < vis; row++) {
    int pos = _scroll_top + row;
    if (pos >= _count) break;
    const HeardStation& st = _list[pos];
    int y = hH + row * eh;
    bool selected = (pos == _sel);

    col(d, selected ? C_CARDSEL : C_CARD);
    d.fillRect(0, y, W, eh - 1);
    col(d, C_DIV);
    d.fillRect(0, y + eh - 1, W, 1);
    if (selected) { col(d, C_ACCENT); d.fillRect(0, y, 3, eh - 1); }

    int ty = y + (eh - 8) / 2;
    snrStr(st.snr_q, snr, sizeof(snr));
    ageStr(_now, st.recv_ts, age, sizeof(age));
    snprintf(meta, sizeof(meta), "h%u %sdB %ddBm %s", st.hops, snr, st.rssi, age);

    int metaW = d.getTextWidth(meta);
    col(d, C_VALUE);
    d.drawTextEllipsized(5, ty, right - 5 - metaW - 6, st.name);
    col(d, C_LABEL);
    d.drawTextRightAlign(right - 2, ty, meta);
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

int LastHeardScreen::renderDetail(DisplayDriver& d) {
  int W = d.width(), hH = headerH(d);
  drawHeaderBar(d, "Station", true);
  col(d, C_HEADER);
  d.fillRect(0, hH, W, d.height() - hH);
  const HeardStation& st = _list[_sel];

  char snr[12], age[12], val[24];
  snrStr(st.snr_q, snr, sizeof(snr));
  ageStr(_now, st.recv_ts, age, sizeof(age));

  int lh = big(d) ? 19 : 11;
  int y = hH + 1;
  auto row = [&](const char* label, const char* value) {
    col(d, C_CARD);
    d.fillRect(0, y, W, lh - 1);
    col(d, C_LABEL);
    d.setCursor(6, y + (lh - 8) / 2);
    d.print(label);
    col(d, C_VALUE);
    d.drawTextRightAlign(W - 6, y + (lh - 8) / 2, value);
    col(d, C_DIV);
    d.fillRect(0, y + lh - 1, W, 1);
    y += lh;
  };

  row("Name", st.name);
  snprintf(val, sizeof(val), "%u", st.hops);   row("Hops", val);
  row("Age", age);
  snprintf(val, sizeof(val), "%s dB", snr);    row("SNR", val);
  snprintf(val, sizeof(val), "%d dBm", st.rssi); row("RSSI", val);
  distStr(st.dist_m, val, sizeof(val));        row("Distance", val);
  return 1000;
}

bool LastHeardScreen::handleInput(char c) {
  if (_detail) {
    if (c == KEY_CANCEL || c == KEY_LEFT || c == KEY_ENTER || c == KEY_SELECT) _detail = false;
    return true;
  }
  if (c == KEY_UP || c == KEY_PREV) { if (_sel > 0) _sel--; return true; }
  if (c == KEY_DOWN || c == KEY_NEXT) { if (_sel < _count - 1) _sel++; return true; }
  if (c == KEY_ENTER || c == KEY_SELECT) { if (_count > 0) _detail = true; return true; }
  if (c == KEY_CANCEL || c == KEY_LEFT) { _task->gotoHomeScreen(); return true; }
  return false;
}

bool LastHeardScreen::handleTouch(int x, int y, TouchEvent ev) {
  DisplayDriver* d = _task->getDisplay();
  if (!d) return false;
  int hH = headerH(*d), eh = entryH(*d);

  if (_detail) {
    if (ev == TouchEvent::release) _detail = false;
    return true;
  }
  if (ev == TouchEvent::press) { _press_y = y; _last_y = y; _moved = false; _pressing = true; return true; }
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
    if (pos >= 0 && pos < _count) { _sel = pos; _detail = true; }
    return true;
  }
  return false;
}
