#pragma once

#include <helpers/ui/UIScreen.h>
#include <stdint.h>

class UITask;

// One recently-heard station, prepared by the task (distance already computed
// from our GPS vs the advert's position, so the screen stays portable).
struct HeardStation {
  char     name[24];
  uint8_t  hops;       // path length (0 = direct)
  uint32_t recv_ts;    // RTC seconds when last heard
  int8_t   snr_q;      // SNR*4 (0 = unknown)
  int8_t   rssi;       // dBm (0 = unknown)
  int32_t  dist_m;     // metres, -1 = unknown
};

// Recently-heard stations with signal strength, age, and distance. List + tap
// detail, mirroring the packet-monitor pattern.
class LastHeardScreen : public UIScreen {
  UITask* _task;
  uint32_t _now;
  static const int CAP = 16;
  HeardStation _list[CAP];
  int _count, _scroll_top, _sel;
  int _press_y, _last_y;
  bool _moved, _pressing;
  bool _detail;

public:
  LastHeardScreen(UITask* task)
      : _task(task), _now(0), _count(0), _scroll_top(0), _sel(0),
        _press_y(0), _last_y(0), _moved(false), _pressing(false), _detail(false) {}

  void setNow(uint32_t now) { _now = now; }
  void reset() { _scroll_top = 0; _sel = 0; _detail = false; }
  void clear() { _count = 0; }
  void addStation(const HeardStation& s) { if (_count < CAP) _list[_count++] = s; }

  int render(DisplayDriver& display) override;
  bool handleInput(char c) override;
  bool handleTouch(int x, int y, TouchEvent ev) override;

private:
  int renderList(DisplayDriver& d);
  int renderDetail(DisplayDriver& d);
};
