#pragma once

#include <helpers/ui/UIScreen.h>
#include <stdint.h>
#include "ChatStore.h"
#include "OnScreenKeyboard.h"

class UITask;

// A single conversation: scrollable speech-bubble thread (colour-coded sender
// names, delivery status on sent messages) + a compose bar driven by either a
// physical keyboard or the on-screen keyboard.
class ConversationScreen : public UIScreen {
  UITask* _task;
  chat::Conv* _conv;
  bool _use_osk, _osk_open, _emoji_open;
  bool _qr_open;
  char _qr_url[128];
  char _title[24];
  char _input[100]; int _ilen;
  char _reply[80];      // quoted text being replied to ("" = none)
  int _scroll;          // pixels scrolled up from the bottom (0 = newest visible)
  uint32_t _now;
  OnScreenKeyboard _kb;
  int _last_y; bool _moved, _pressing;

  void send();
  void tapMessage(DisplayDriver& d, int y);  // reply or open QR for a tapped bubble
  int composeBarH(DisplayDriver& d);
  int replyStripH(DisplayDriver& d);
  int oskHeight(DisplayDriver& d);
  int threadBottom(DisplayDriver& d);
  void drawQR(DisplayDriver& d);

public:
  ConversationScreen(UITask* t)
      : _task(t), _conv(nullptr), _use_osk(false), _osk_open(false), _emoji_open(false),
        _qr_open(false), _ilen(0), _scroll(0), _now(0), _last_y(0), _moved(false), _pressing(false) {
    _title[0] = 0; _input[0] = 0; _reply[0] = 0; _qr_url[0] = 0;
  }

  void begin(chat::Conv* c, const char* title, bool use_osk) {
    _conv = c; _use_osk = use_osk; _osk_open = false; _emoji_open = false; _qr_open = false;
    _ilen = 0; _input[0] = 0; _reply[0] = 0; _scroll = 0;
    strncpy(_title, title, sizeof(_title) - 1); _title[sizeof(_title) - 1] = 0;
    _kb.reset();
  }
  void setNow(uint32_t n) { _now = n; }
  chat::Conv* conv() { return _conv; }

  int render(DisplayDriver& display) override;
  bool handleInput(char c) override;
  bool handleTouch(int x, int y, TouchEvent ev) override;
};
