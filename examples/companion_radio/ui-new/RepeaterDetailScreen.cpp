#include "RepeaterDetailScreen.h"
#include "UITask.h"
#include "UIStyle.h"
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

using namespace uistyle;

namespace {
const int N_ACTIONS = 5;
const char* ACTIONS[N_ACTIONS] = {"Get status", "Login (admin)", "Send advert", "Sync clock", "Favourite"};
int actionH(DisplayDriver& d) { return big(d) ? 20 : 13; }
int lineH(DisplayDriver& d) { return big(d) ? 14 : 11; }
void uptimeStr(uint32_t s, char* b, size_t n) {
  if (s >= 86400) snprintf(b, n, "%ud %uh", (unsigned)(s / 86400), (unsigned)((s % 86400) / 3600));
  else if (s >= 3600) snprintf(b, n, "%uh %um", (unsigned)(s / 3600), (unsigned)((s % 3600) / 60));
  else snprintf(b, n, "%um", (unsigned)(s / 60));
}
}  // namespace

void RepeaterDetailScreen::begin(const uint8_t* pubkey6, const char* name, uint8_t type, bool fav, bool use_osk) {
  memcpy(_pubkey, pubkey6, 6);
  strncpy(_name, name, sizeof(_name) - 1); _name[sizeof(_name) - 1] = 0;
  _type = type; _fav = fav; _use_osk = use_osk;
  _logged_in = false; _perms = 0; _has_status = false;
  _login_mode = false; _osk_open = false; _pwlen = 0; _pw[0] = 0;
  _scroll = 0; _sel = 0;
  snprintf(_info, sizeof(_info), "Not logged in");
  _kb.reset();
}

int RepeaterDetailScreen::oskHeight(DisplayDriver& d) { return _osk_open ? (d.height() * 48 / 100) : 0; }

void RepeaterDetailScreen::onLogin(bool success, uint8_t perms) {
  _logged_in = success; _perms = perms;
  snprintf(_info, sizeof(_info), success ? (perms ? "Login OK (admin)" : "Login OK") : "Login failed");
  _login_mode = false; _osk_open = false;
}

void RepeaterDetailScreen::onReply(const char* text) {
  strncpy(_info, text, sizeof(_info) - 1); _info[sizeof(_info) - 1] = 0;
}

void RepeaterDetailScreen::onStatus(const uint8_t* d, uint8_t len) {
  if (len < 56) { snprintf(_info, sizeof(_info), "Bad status (%u b)", len); return; }
  auto u16 = [&](int o) { return (uint16_t)(d[o] | (d[o + 1] << 8)); };
  auto i16 = [&](int o) { return (int16_t)u16(o); };
  auto u32 = [&](int o) { return (uint32_t)(d[o] | (d[o + 1] << 8) | (d[o + 2] << 16) | ((uint32_t)d[o + 3] << 24)); };
  _st.batt_mv = u16(0); _st.queue = u16(2); _st.noise = i16(4); _st.rssi = i16(6);
  _st.n_recv = u32(8); _st.n_sent = u32(12); _st.air_tx = u32(16); _st.uptime = u32(20);
  _st.sent_flood = u32(24); _st.sent_direct = u32(28); _st.recv_flood = u32(32); _st.recv_direct = u32(36);
  _st.err_events = u16(40); _st.snr_q = i16(42);
  _st.air_rx = (len >= 52) ? u32(48) : 0;
  _st.recv_errors = (len >= 56) ? u32(52) : 0;
  _has_status = true;
  snprintf(_info, sizeof(_info), "Status updated");
}

int RepeaterDetailScreen::render(DisplayDriver& dd) {
  int W = dd.width(), H = dd.height(), hH = headerH(dd), ah = actionH(dd), lh = lineH(dd);
  drawHeaderBar(dd, _name, true);
  int oskH = oskHeight(dd);
  int bottom = H - oskH;
  col(dd, C_HEADER); dd.fillRect(0, hH, W, bottom - hH);

  if (_login_mode) {
    // password entry
    col(dd, C_LABEL); dd.drawTextLeftAlign(6, hH + 6, "Password:");
    int by = hH + 20;
    col(dd, C_CARD); dd.fillRect(6, by, W - 12, 16);
    col(dd, C_DIV); dd.drawRect(6, by, W - 12, 16);
    char mask[24]; int m = _pwlen < 23 ? _pwlen : 23;
    for (int i = 0; i < m; i++) mask[i] = '*'; mask[m] = 0;
    col(dd, C_VALUE); dd.drawTextLeftAlign(10, by + 4, mask);
    col(dd, C_MUTED); dd.drawTextCentered(W / 2, by + 24, _use_osk ? "type + OK to log in" : "type, Enter to log in");
    if (_osk_open) _kb.render(dd, 0, H - oskH, W, oskH);
    return 1000;
  }

  // action buttons
  int y = hH;
  for (int i = 0; i < N_ACTIONS; i++) {
    bool seld = (i == _sel);
    col(dd, seld ? C_CARDSEL : C_CARD);
    dd.fillRect(0, y, W, ah - 1);
    col(dd, C_DIV); dd.fillRect(0, y + ah - 1, W, 1);
    if (seld) { col(dd, C_ACCENT); dd.fillRect(0, y, 3, ah - 1); }
    col(dd, C_VALUE);
    const char* label = ACTIONS[i];
    char fav[16];
    if (i == 4) { snprintf(fav, sizeof(fav), _fav ? "Unfavourite" : "Favourite"); label = fav; }
    dd.drawTextLeftAlign(8, y + (ah - 8) / 2, label);
    if (i == 4 && _fav) { col(dd, C_BADGE); dd.drawTextRightAlign(W - 8, y + (ah - 8) / 2, "*"); }
    y += ah;
  }

  // info line
  col(dd, C_DIV); dd.fillRect(0, y, W, 1); y += 2;
  col(dd, _logged_in ? C_ACCENT : C_LABEL);
  dd.drawTextEllipsized(6, y, W - 10, _info); y += lh;

  // stats
  if (_has_status) {
    char k[24], v[24];
    auto row = [&](const char* key, const char* val) {
      if (y + lh > bottom) return;
      col(dd, C_LABEL); dd.drawTextLeftAlign(6, y, key);
      col(dd, C_VALUE); dd.drawTextRightAlign(W - 6, y, val);
      y += lh;
    };
    snprintf(v, sizeof(v), "%u.%02u V", _st.batt_mv / 1000, (_st.batt_mv % 1000) / 10); row("Battery", v);
    uptimeStr(_st.uptime, v, sizeof(v)); row("Uptime", v);
    snprintf(v, sizeof(v), "tx %us  rx %us", (unsigned)_st.air_tx, (unsigned)_st.air_rx); row("Airtime", v);
    snprintf(v, sizeof(v), "%u rx / %u tx", (unsigned)_st.n_recv, (unsigned)_st.n_sent); row("Packets", v);
    snprintf(v, sizeof(v), "%ddBm  q%u", _st.noise, _st.queue); row("Noise", v);
    snprintf(v, sizeof(v), "%ddBm  %d.%ddB", _st.rssi, _st.snr_q / 4, (abs(_st.snr_q) % 4) * 25 / 10); row("Last RSSI/SNR", v);
    snprintf(v, sizeof(v), "%u", _st.err_events); row("Errors", v);
    (void)k;
  }
  return 1000;
}

void RepeaterDetailScreen::activate(int a) {
  uint32_t to;
  switch (a) {
    case 0: _task->requestStatus(_pubkey); snprintf(_info, sizeof(_info), "Requesting status..."); break;
    case 1: _login_mode = true; _pwlen = 0; _pw[0] = 0; if (_use_osk) _osk_open = true; break;
    case 2: _task->sendTrigger(_pubkey, "advert"); snprintf(_info, sizeof(_info), "Advert sent..."); break;
    case 3: _task->sendTrigger(_pubkey, "clock sync"); snprintf(_info, sizeof(_info), "Clock sync sent..."); break;
    case 4: _fav = !_fav; _task->toggleFavourite(_pubkey, _fav); break;
  }
  (void)to;
}

bool RepeaterDetailScreen::handleInput(char c) {
  if (_login_mode) {
    if (c == KEY_ENTER || c == KEY_SELECT) { _task->startLogin(_pubkey, _pw); _login_mode = false; _osk_open = false; return true; }
    if (c == KEY_BACKSPACE) { if (_pwlen > 0) _pw[--_pwlen] = 0; return true; }
    if (c == KEY_CANCEL || c == KEY_LEFT) { _login_mode = false; _osk_open = false; return true; }
    if (c >= 32 && c < 127) { if (_pwlen < (int)sizeof(_pw) - 1) { _pw[_pwlen++] = c; _pw[_pwlen] = 0; } return true; }
    return true;
  }
  if (c == KEY_UP || c == KEY_PREV) { if (_sel > 0) _sel--; return true; }
  if (c == KEY_DOWN || c == KEY_NEXT) { if (_sel < N_ACTIONS - 1) _sel++; return true; }
  if (c == KEY_ENTER || c == KEY_SELECT) { activate(_sel); return true; }
  if (c == KEY_CANCEL || c == KEY_LEFT) { _task->gotoRepeaters(); return true; }
  return false;
}

bool RepeaterDetailScreen::handleTouch(int x, int y, TouchEvent ev) {
  DisplayDriver* d = _task->getDisplay();
  if (!d) return false;
  int W = d->width(), H = d->height(), hH = headerH(*d), ah = actionH(*d);

  if (_login_mode) {
    if (ev != TouchEvent::release) return true;
    int oskH = oskHeight(*d);
    if (_osk_open && y >= H - oskH) {
      char k = _kb.handleTap(x, y, 0, H - oskH, W, oskH);
      if (k == OnScreenKeyboard::KEY_OK) { _task->startLogin(_pubkey, _pw); _login_mode = false; _osk_open = false; }
      else if (k == OnScreenKeyboard::KEY_BKSP) { if (_pwlen > 0) _pw[--_pwlen] = 0; }
      else if (k >= 32 && k < 127) { if (_pwlen < (int)sizeof(_pw) - 1) { _pw[_pwlen++] = k; _pw[_pwlen] = 0; } }
      return true;
    }
    if (y < hH) { _login_mode = false; _osk_open = false; }
    return true;
  }

  if (ev != TouchEvent::release) return true;
  if (y < hH) { _task->gotoRepeaters(); return true; }
  int idx = (y - hH) / ah;
  if (idx >= 0 && idx < N_ACTIONS) { _sel = idx; activate(idx); }
  return true;
}
