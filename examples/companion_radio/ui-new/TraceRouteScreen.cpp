#include "TraceRouteScreen.h"
#include "UITask.h"
#include "UIStyle.h"
#include <stdio.h>
#include <string.h>

using namespace uistyle;

namespace {
const uint32_t TIMEOUT_S = 12;
void snrStr(int8_t snr_q, char* b, size_t n) {
  int q = snr_q;
  const char* sign = (q < 0) ? "-" : "+";
  int a = q < 0 ? -q : q;
  snprintf(b, n, "%s%d.%d", sign, a / 4, (a % 4) * 25 / 10);
}
int rowH_(DisplayDriver& d) { return big(d) ? 20 : 12; }
}  // namespace

void TraceRouteScreen::addTarget(const char* name, int idx) {
  if (_ntargets >= MAX_TARGETS) return;
  Target& t = _targets[_ntargets++];
  strncpy(t.name, name, sizeof(t.name) - 1);
  t.name[sizeof(t.name) - 1] = 0;
  t.idx = idx;
}

void TraceRouteScreen::onTraceStarted(uint32_t tag, const char* target_name) {
  _tag = tag;
  _start_ts = _now;
  strncpy(_target_name, target_name, sizeof(_target_name) - 1);
  _target_name[sizeof(_target_name) - 1] = 0;
  _state = TRACING;
}

void TraceRouteScreen::onTraceFailed(const char* reason) {
  strncpy(_fail_msg, reason, sizeof(_fail_msg) - 1);
  _fail_msg[sizeof(_fail_msg) - 1] = 0;
  _state = FAILED;
}

void TraceRouteScreen::onResult(uint32_t tag, const uint8_t* hashes, const uint8_t* snrs,
                                uint8_t path_len, uint8_t path_sz, int8_t final_snr_q) {
  if (_state != TRACING || tag != _tag) return;  // not ours
  int nsnr = path_len >> path_sz;
  int n = path_len < MAX_HOPS ? path_len : MAX_HOPS;
  _nhops = n;
  for (int i = 0; i < n; i++) {
    _hop_hash[i] = hashes ? hashes[i] : 0;
    _hop_snr[i] = (i < nsnr && snrs) ? (int8_t)snrs[i] : 0;
  }
  _final_snr = final_snr_q;
  _state = RESULT;
}

int TraceRouteScreen::render(DisplayDriver& d) {
  if (_state == TRACING && _now > 0 && _start_ts > 0 && _now - _start_ts > TIMEOUT_S)
    onTraceFailed("No response (timed out)");
  switch (_state) {
    case PICK:   return renderPick(d);
    case RESULT: return renderResult(d);
    default:     return renderStatus(d);
  }
}

int TraceRouteScreen::renderPick(DisplayDriver& d) {
  int W = d.width(), hH = headerH(d), eh = rowH_(d);
  drawHeaderBar(d, "Trace", true);
  col(d, C_HEADER);
  d.fillRect(0, hH, W, d.height() - hH);

  if (_ntargets == 0) {
    col(d, C_LABEL);
    d.drawTextCentered(W / 2, d.height() / 2 - 8, "No reachable nodes");
    col(d, C_MUTED);
    d.drawTextCentered(W / 2, d.height() / 2 + 6, "(need a known path)");
    return 1000;
  }

  int vis = (d.height() - hH) / eh;
  if (vis < 1) vis = 1;
  if (_sel < 0) _sel = 0;
  if (_sel > _ntargets - 1) _sel = _ntargets - 1;
  if (_sel < _scroll_top) _scroll_top = _sel;
  if (_sel >= _scroll_top + vis) _scroll_top = _sel - vis + 1;

  for (int row = 0; row < vis; row++) {
    int pos = _scroll_top + row;
    if (pos >= _ntargets) break;
    int y = hH + row * eh;
    bool selected = (pos == _sel);
    col(d, selected ? C_CARDSEL : C_CARD);
    d.fillRect(0, y, W, eh - 1);
    col(d, C_DIV);
    d.fillRect(0, y + eh - 1, W, 1);
    if (selected) { col(d, C_ACCENT); d.fillRect(0, y, 3, eh - 1); }
    col(d, C_VALUE);
    d.drawTextEllipsized(6, y + (eh - 8) / 2, W - 10, _targets[pos].name);
  }
  return 1000;
}

int TraceRouteScreen::renderStatus(DisplayDriver& d) {
  int W = d.width(), hH = headerH(d);
  drawHeaderBar(d, "Trace", true);
  col(d, C_HEADER);
  d.fillRect(0, hH, W, d.height() - hH);
  int cy = d.height() / 2;

  if (_state == TRACING) {
    char line[40];
    int dots = (int)(_now % 4);
    char d3[4] = {0};
    for (int i = 0; i < dots; i++) d3[i] = '.';
    snprintf(line, sizeof(line), "Tracing %s%s", _target_name, d3);
    col(d, C_VALUE);
    d.drawTextCentered(W / 2, cy - 4, line);
  } else {  // FAILED
    col(d, C_WARN);
    d.drawTextCentered(W / 2, cy - 10, _fail_msg);
    col(d, C_MUTED);
    d.drawTextCentered(W / 2, cy + 6, "tap to retry");
  }
  return 500;
}

int TraceRouteScreen::renderResult(DisplayDriver& d) {
  int W = d.width(), hH = headerH(d), lh = rowH_(d);
  drawHeaderBar(d, "Trace", true);
  col(d, C_HEADER);
  d.fillRect(0, hH, W, d.height() - hH);

  int y = hH;
  // target header row
  col(d, C_CARDSEL);
  d.fillRect(0, y, W, lh - 1);
  col(d, C_ACCENT);
  d.drawTextEllipsized(6, y + (lh - 8) / 2, W - 10, _target_name);
  col(d, C_DIV); d.fillRect(0, y + lh - 1, W, 1);
  y += lh;

  char snr[12], line[48];
  for (int i = 0; i < _nhops; i++) {
    if (y + lh > d.height()) break;
    snrStr(_hop_snr[i], snr, sizeof(snr));
    snprintf(line, sizeof(line), "%d. id %02X", i + 1, _hop_hash[i]);
    col(d, C_CARD); d.fillRect(0, y, W, lh - 1);
    col(d, C_VALUE); d.drawTextLeftAlign(6, y + (lh - 8) / 2, line);
    char sv[16]; snprintf(sv, sizeof(sv), "%s dB", snr);
    col(d, C_LABEL); d.drawTextRightAlign(W - 6, y + (lh - 8) / 2, sv);
    col(d, C_DIV); d.fillRect(0, y + lh - 1, W, 1);
    y += lh;
  }
  // final hop to us
  if (y + lh <= d.height()) {
    snrStr(_final_snr, snr, sizeof(snr));
    col(d, C_CARD); d.fillRect(0, y, W, lh - 1);
    col(d, C_VALUE); d.drawTextLeftAlign(6, y + (lh - 8) / 2, "-> you");
    char sv[16]; snprintf(sv, sizeof(sv), "%s dB", snr);
    col(d, C_ACCENT); d.drawTextRightAlign(W - 6, y + (lh - 8) / 2, sv);
  }
  return 1000;
}

bool TraceRouteScreen::handleInput(char c) {
  if (_state == PICK) {
    if (c == KEY_UP || c == KEY_PREV) { if (_sel > 0) _sel--; return true; }
    if (c == KEY_DOWN || c == KEY_NEXT) { if (_sel < _ntargets - 1) _sel++; return true; }
    if ((c == KEY_ENTER || c == KEY_SELECT) && _ntargets > 0) {
      uint32_t tag = _task->startTrace(_targets[_sel].idx);
      if (tag) onTraceStarted(tag, _targets[_sel].name);
      else onTraceFailed("No route to node");
      return true;
    }
    if (c == KEY_CANCEL || c == KEY_LEFT) { _task->gotoHomeScreen(); return true; }
    return false;
  }
  // TRACING / RESULT / FAILED -> back to picker
  if (c == KEY_CANCEL || c == KEY_LEFT || c == KEY_ENTER || c == KEY_SELECT) {
    beginPick();
    return true;
  }
  return false;
}

bool TraceRouteScreen::handleTouch(int x, int y, TouchEvent ev) {
  DisplayDriver* d = _task->getDisplay();
  if (!d) return false;
  int hH = headerH(*d), eh = rowH_(*d);

  if (_state == PICK) {
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
      if (pos >= 0 && pos < _ntargets) {
        _sel = pos;
        uint32_t tag = _task->startTrace(_targets[_sel].idx);
        if (tag) onTraceStarted(tag, _targets[_sel].name);
        else onTraceFailed("No route to node");
      }
      return true;
    }
    return false;
  }
  // any tap returns to the picker
  if (ev == TouchEvent::release) {
    if (y < hH) _task->gotoHomeScreen();
    else beginPick();
    return true;
  }
  return true;
}
