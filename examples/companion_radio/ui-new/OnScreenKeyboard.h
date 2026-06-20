#pragma once

#include <helpers/ui/DisplayDriver.h>

// A minimal tappable on-screen keyboard for text entry on touch-only boards
// (boards with a physical keyboard feed chars directly and don't need this).
// Stateless apart from a shift flag; the caller owns the text buffer and just
// feeds taps in -> gets back a character (or a special action code).
class OnScreenKeyboard {
  bool _shift = false;

public:
  // special return codes from handleTap() (outside printable ASCII range)
  static const char KEY_NONE = 0;      // tap hit nothing actionable
  static const char KEY_BKSP = '\b';   // 8
  static const char KEY_OK    = '\r';   // 13

  void reset() { _shift = false; }
  bool isShift() const { return _shift; }

  // draw the keyboard filling rect (rx,ry,rw,rh)
  void render(DisplayDriver& d, int rx, int ry, int rw, int rh);

  // map a tap at (tx,ty) within rect (rx,ry,rw,rh) to a character.
  // returns a printable ASCII char, KEY_BKSP, KEY_OK, or KEY_NONE.
  // toggling shift is handled internally and returns KEY_NONE.
  char handleTap(int tx, int ty, int rx, int ry, int rw, int rh);
};
