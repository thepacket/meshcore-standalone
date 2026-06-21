#include "NoiseScopeScreen.h"
#include "UITask.h"
#include "UIStyle.h"
#include <stdio.h>

using namespace uistyle;

namespace {
const int DB_TOP = -40;    // top of the plot (dBm)
const int DB_BOT = -120;   // bottom of the plot (dBm)
int dbToY(int dbm, int y0, int plotH) {
  if (dbm > DB_TOP) dbm = DB_TOP;
  if (dbm < DB_BOT) dbm = DB_BOT;
  // DB_TOP -> y0 (top), DB_BOT -> y0+plotH (bottom)
  int frac = (DB_TOP - dbm) * plotH / (DB_TOP - DB_BOT);
  return y0 + frac;
}
}  // namespace

int NoiseScopeScreen::render(DisplayDriver& d) {
  int W = d.width(), H = d.height(), hH = headerH(d);
  drawHeaderBar(d, "Noise", true);

  // clear plot background
  col(d, C_HEADER);
  d.fillRect(0, hH, W, H - hH);

  int labelH = big(d) ? 12 : 9;
  int y0 = hH + labelH;
  int plotH = H - y0 - 2;
  if (plotH < 4) plotH = 4;

  // reference gridlines at -60/-80/-100 dBm
  d.setTextSize(1);
  for (int g = -60; g >= -100; g -= 20) {
    int gy = dbToY(g, y0, plotH);
    col(d, C_DIV);
    for (int x = 0; x < W; x += 4) d.fillRect(x, gy, 2, 1);  // dashed
    char lbl[8];
    snprintf(lbl, sizeof(lbl), "%d", g);
    col(d, C_MUTED);
    d.drawTextRightAlign(W - 2, gy - 4, lbl);
  }

  if (_count == 0) {
    col(d, C_LABEL);
    d.drawTextCentered(W / 2, y0 + plotH / 2 - 4, "Sampling...");
    return 500;
  }

  // plot the most recent samples, newest at the right edge
  int shown = _count < W ? _count : W;
  int cur = _buf[_head], mn = 127, mx = -127;
  int baseY = y0 + plotH;
  int prevX = -1, prevY = 0;
  for (int i = 0; i < shown; i++) {
    int idx = (_head - (shown - 1 - i) + CAP) % CAP;  // oldest..newest left..right
    int v = _buf[idx];
    if (v < mn) mn = v;
    if (v > mx) mx = v;
    int x = W - shown + i;
    int yv = dbToY(v, y0, plotH);
    // filled bar from baseline up to the sample
    col(d, C_CARDSEL);
    d.fillRect(x, yv, 1, baseY - yv);
    // bright top pixel / connecting line
    col(d, C_ACCENT);
    if (prevX >= 0) {
      int a = prevY, b = yv;
      if (a > b) { int t = a; a = b; b = t; }
      d.fillRect(x, a, 1, (b - a) + 1);
    } else {
      d.fillRect(x, yv, 1, 1);
    }
    prevX = x; prevY = yv;
  }

  // current / min / max readout
  char info[40];
  snprintf(info, sizeof(info), "now %ddBm  lo %d  hi %d", cur, mn, mx);
  col(d, C_VALUE);
  d.drawTextLeftAlign(4, hH + (labelH - 8) / 2, info);
  return 500;  // ~2 Hz refresh
}

bool NoiseScopeScreen::handleInput(char c) {
  if (c == KEY_CANCEL || c == KEY_LEFT) { _task->gotoHomeScreen(); return true; }
  return false;
}

bool NoiseScopeScreen::handleTouch(int x, int y, TouchEvent ev) {
  DisplayDriver* d = _task->getDisplay();
  if (!d) return false;
  if (ev == TouchEvent::release && y < headerH(*d)) { _task->gotoHomeScreen(); return true; }
  return true;
}
