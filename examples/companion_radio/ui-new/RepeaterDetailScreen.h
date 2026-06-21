#pragma once

#include <helpers/ui/UIScreen.h>
#include <stdint.h>
#include "OnScreenKeyboard.h"

class UITask;

// Manage one repeater/room: get status (parsed RepeaterStats), admin login
// (password), remote triggers (advert / clock sync), favourite toggle.
class RepeaterDetailScreen : public UIScreen {
  UITask* _task;
  uint8_t _pubkey[6];
  char _name[24];
  uint8_t _type;
  bool _fav;
  bool _logged_in;
  uint8_t _perms;
  char _info[80];          // status line / last reply

  bool _has_status;
  struct Stats {           // parsed RepeaterStats (56-byte LE blob)
    uint16_t batt_mv, queue;
    int16_t  noise, rssi;
    uint32_t n_recv, n_sent, air_tx, uptime;
    uint32_t sent_flood, sent_direct, recv_flood, recv_direct;
    uint16_t err_events; int16_t snr_q;
    uint32_t air_rx, recv_errors;
  } _st;

  // password login sub-mode
  bool _login_mode, _osk_open, _use_osk;
  char _pw[24]; int _pwlen;
  OnScreenKeyboard _kb;

  // advert one-hop/flood choice overlay
  bool _advert_menu; int _advert_sel;
  void drawAdvertMenu(DisplayDriver& d);

  int _scroll;
  int _sel;                // selected action
  int _last_y; bool _moved, _pressing;

  void activate(int a);
  int oskHeight(DisplayDriver& d);

public:
  RepeaterDetailScreen(UITask* t)
      : _task(t), _type(0), _fav(false), _logged_in(false), _perms(0),
        _has_status(false), _login_mode(false), _osk_open(false), _use_osk(false),
        _pwlen(0), _advert_menu(false), _advert_sel(0),
        _scroll(0), _sel(0), _last_y(0), _moved(false), _pressing(false) {
    _name[0] = 0; _info[0] = 0; _pw[0] = 0;
  }

  void begin(const uint8_t* pubkey6, const char* name, uint8_t type, bool fav, bool use_osk);
  const uint8_t* pubkey() const { return _pubkey; }

  // results routed from the task
  void onLogin(bool success, uint8_t perms);
  void onStatus(const uint8_t* data, uint8_t len);
  void onReply(const char* text);

  int render(DisplayDriver& display) override;
  bool handleInput(char c) override;
  bool handleTouch(int x, int y, TouchEvent ev) override;
};
