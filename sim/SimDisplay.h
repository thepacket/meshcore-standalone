#pragma once

// Desktop DisplayDriver backed by SDL2 + SDL2_ttf. Lets the real on-device
// UIScreen code render into a window so the UI can be exercised without hardware.
#include <helpers/ui/DisplayDriver.h>
#include <SDL.h>
#include <SDL_ttf.h>

class SimDisplay : public DisplayDriver {
  SDL_Renderer* _ren;
  TTF_Font* _font1;
  TTF_Font* _font2;
  TTF_Font* _cur;
  SDL_Color _col;
  int _cx, _cy;
  bool _mouse_down;
  int _mx, _my;

public:
  SimDisplay(SDL_Renderer* ren, TTF_Font* f1, TTF_Font* f2)
      : DisplayDriver(320, 240), _ren(ren), _font1(f1), _font2(f2), _cur(f1),
        _cx(0), _cy(0), _mouse_down(false), _mx(0), _my(0) {
    _col = {255, 255, 255, 255};
  }

  void setMouse(bool down, int x, int y) { _mouse_down = down; _mx = x; _my = y; }

  bool isOn() override { return true; }
  void turnOn() override {}
  void turnOff() override {}
  void clear() override {}
  void startFrame(Color bkg = DARK) override;
  void setTextSize(int sz) override { _cur = (sz >= 2) ? _font2 : _font1; }
  void setColor(Color c) override;
  void setCursor(int x, int y) override { _cx = x; _cy = y; }
  void print(const char* str) override;
  void fillRect(int x, int y, int w, int h) override;
  void drawRect(int x, int y, int w, int h) override;
  void drawXbm(int, int, const uint8_t*, int, int) override {}
  uint16_t getTextWidth(const char* str) override;
  void endFrame() override;
  bool getTouch(int* x, int* y) override {
    if (_mouse_down) { *x = _mx; *y = _my; return true; }
    return false;
  }
  bool hasTouch() override { return true; }
};
