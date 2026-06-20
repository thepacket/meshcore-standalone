#pragma once

#include "DisplayDriver.h"

#define KEY_LEFT           0xB4
#define KEY_UP             0xB5
#define KEY_DOWN           0xB6
#define KEY_RIGHT          0xB7
#define KEY_SELECT           10
#define KEY_ENTER            13
#define KEY_CANCEL           27   // Esc
#define KEY_BACKSPACE         8   // delete previous char (text entry)
#define KEY_HOME           0xF0
#define KEY_NEXT           0xF1
#define KEY_PREV           0xF2
#define KEY_CONTEXT_MENU   0xF3

// Coordinate-based input events (e.g. capacitive touch). Coords are in the
// display's logical coordinate space (the same space render() draws in).
enum class TouchEvent : uint8_t {
  press,    // finger down
  release,  // finger lifted (a tap completes on release)
  move,     // finger moved while down (use for drag/scroll)
};

class UIScreen {
protected:
  UIScreen() { }
public:
  virtual int render(DisplayDriver& display) =0;   // return value is number of millis until next render
  virtual bool handleInput(char c) { return false; }
  // touch dispatch; default no-op so existing keycode-only screens/boards are unaffected.
  virtual bool handleTouch(int x, int y, TouchEvent ev) { return false; }
  virtual void poll() { }
};

