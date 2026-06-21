#pragma once

#include <helpers/ui/UIScreen.h>
#include <stdint.h>

class UITask;

// A repeater's mesh-coverage entry, prepared by the task.
struct RepeaterSignal {
  char     name[24];
  uint32_t recv_ts;   // when last heard (RTC seconds, 0 = never via advert)
  int8_t   snr_q;     // SNR*4 of last advert (0 = unknown)
  int8_t   rssi;      // dBm of last advert (0 = unknown)
  bool     heard;     // true if we have a recent advert for it
};

// "Cell signal bars, but for mesh coverage" -- one row per known repeater with
// a coverage gauge (from last advert RSSI) and how long ago it was heard.
class SignalScreen : public UIScreen {
  UITask* _task;
  uint32_t _now;
  static const int CAP = 16;
  RepeaterSignal _list[CAP];
  int _count, _scroll_top, _sel;
  int _last_y;
  bool _moved, _pressing;

public:
  SignalScreen(UITask* task)
      : _task(task), _now(0), _count(0), _scroll_top(0), _sel(0),
        _last_y(0), _moved(false), _pressing(false) {}

  void setNow(uint32_t now) { _now = now; }
  void reset() { _scroll_top = 0; _sel = 0; }
  void clear() { _count = 0; }
  void addRepeater(const RepeaterSignal& r) { if (_count < CAP) _list[_count++] = r; }

  int render(DisplayDriver& display) override;
  bool handleInput(char c) override;
  bool handleTouch(int x, int y, TouchEvent ev) override;
};
