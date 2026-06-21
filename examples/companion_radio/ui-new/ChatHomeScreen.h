#pragma once

#include <helpers/ui/UIScreen.h>
#include <stdint.h>
#include "ChatStore.h"

class UITask;

// Chat home: Channels | DMs tabs (channels-first). Each row shows the title, a
// last-message preview, and an unread badge. Lists are populated by the task
// from live mesh enumeration; previews/unread come from the ChatStore.
class ChatHomeScreen : public UIScreen {
public:
  struct Entry {
    bool    is_channel;
    int     channel_idx;
    uint8_t peer[6];
    char    title[20];
  };

private:
  UITask* _task;
  chat::ChatStore* _store;
  static const int MAXE = 24;
  Entry _ch[MAXE]; int _nch;
  Entry _dm[MAXE]; int _ndm;
  int _tab;            // 0 = channels, 1 = DMs
  int _sel[2], _top[2];
  uint32_t _now;
  int _last_y; bool _moved, _pressing;

  Entry* curList(int& n);
  chat::Conv* convFor(const Entry& e);
  void open(const Entry& e);

public:
  ChatHomeScreen(UITask* t, chat::ChatStore* s)
      : _task(t), _store(s), _nch(0), _ndm(0), _tab(0), _now(0),
        _last_y(0), _moved(false), _pressing(false) {
    _sel[0] = _sel[1] = 0; _top[0] = _top[1] = 0;
  }

  void setNow(uint32_t n) { _now = n; }
  void clear() { _nch = 0; _ndm = 0; }
  void addChannel(int idx, const char* title);
  void addDm(const uint8_t* peer6, const char* title);
  void begin() { if (_tab != 0 && _tab != 1) _tab = 0; }

  int render(DisplayDriver& display) override;
  bool handleInput(char c) override;
  bool handleTouch(int x, int y, TouchEvent ev) override;
};
