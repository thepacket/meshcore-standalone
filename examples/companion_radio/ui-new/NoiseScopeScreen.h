#pragma once

#include <helpers/ui/UIScreen.h>
#include <stdint.h>

class UITask;

// Live scrolling graph of the RF noise floor (dBm). Samples are pushed
// continuously from UITask::loop() so history exists when the screen is opened.
class NoiseScopeScreen : public UIScreen {
  UITask* _task;
  static const int CAP = 240;   // one sample per pixel column at most
  int8_t _buf[CAP];
  int _count, _head;            // _head = index of newest sample

public:
  NoiseScopeScreen(UITask* task) : _task(task), _count(0), _head(0) {}

  void reset() { _count = 0; _head = 0; }
  void addSample(int dbm) {
    if (dbm > 0) dbm = 0;
    if (dbm < -127) dbm = -127;
    _head = (_count == 0) ? 0 : (_head + 1) % CAP;
    _buf[_head] = (int8_t)dbm;
    if (_count < CAP) _count++;
  }

  int render(DisplayDriver& display) override;
  bool handleInput(char c) override;
  bool handleTouch(int x, int y, TouchEvent ev) override;
};
