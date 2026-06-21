#pragma once

#include <helpers/ui/UIScreen.h>
#include <stdint.h>

class UITask;

// A repeater/room entry shown in the list (saved or heard-candidate).
struct NodeEntry {
  uint8_t pubkey[6];
  char    name[20];
  uint8_t type;   // ADV_TYPE_REPEATER / ADV_TYPE_ROOM
  bool    fav;
};

// Repeater management home: Saved | Scan tabs. Saved = repeaters/rooms in
// contacts (tap to manage); Scan = recently-heard nodes not yet saved (tap to
// add). Lists are populated by the task.
class RepeatersScreen : public UIScreen {
  UITask* _task;
  static const int MAXE = 16;
  NodeEntry _saved[MAXE]; int _nsaved;
  NodeEntry _scan[MAXE];  int _nscan;
  int _tab;            // 0 = saved, 1 = scan
  int _sel[2], _top[2];
  int _last_y; bool _moved, _pressing;

  NodeEntry* curList(int& n);

public:
  RepeatersScreen(UITask* t)
      : _task(t), _nsaved(0), _nscan(0), _tab(0), _last_y(0), _moved(false), _pressing(false) {
    _sel[0] = _sel[1] = 0; _top[0] = _top[1] = 0;
  }

  void clear() { _nsaved = 0; _nscan = 0; }
  void addSaved(const uint8_t* pk, const char* name, uint8_t type, bool fav);
  void addScan(const uint8_t* pk, const char* name, uint8_t type);
  void begin() { if (_tab != 0 && _tab != 1) _tab = 0; }
  void setTab(int t) { _tab = (t == 1) ? 1 : 0; }

  int render(DisplayDriver& display) override;
  bool handleInput(char c) override;
  bool handleTouch(int x, int y, TouchEvent ev) override;

private:
  void activate(int pos);
};
