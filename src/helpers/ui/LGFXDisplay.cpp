#include "LGFXDisplay.h"

#ifndef LGFX_COLOR_DEPTH
  #define LGFX_COLOR_DEPTH 8
#endif
// Use the EXPLICIT LovyanGFX colour-depth enums (not the int overload) so the
// sprite is guaranteed a direct/non-indexed colour format -- the int
// setColorDepth(8) can yield a palette (indexed) sprite, which scrambles colours.
#if LGFX_COLOR_DEPTH == 16
  #define LGFX_CD lgfx::v1::color_depth_t::rgb565_2Byte
#else
  #define LGFX_CD lgfx::v1::color_depth_t::rgb332_1Byte
#endif

bool LGFXDisplay::begin() {
  turnOn();
  display->init();
  display->setRotation(1);
  display->setBrightness(64);
  display->setColorDepth(LGFX_CD);
  display->setTextColor(TFT_WHITE);

  buffer.setColorDepth(LGFX_CD);
  buffer.setPsram(true);
  buffer.createSprite(width(), height());

  return true;
}

void LGFXDisplay::turnOn() {
//  display->wakeup();
  if (!_isOn) {
    display->wakeup();
  }
  _isOn = true;
}

void LGFXDisplay::turnOff() {
  if (_isOn) {
    display->sleep();
  }
  _isOn = false;
}

void LGFXDisplay::clear() {
//  display->clearDisplay();
  buffer.clearDisplay();
}

void LGFXDisplay::startFrame(Color bkg) {
//  display->startWrite();
//  display->getScanLine();
  buffer.clearDisplay();
  buffer.setTextColor(TFT_WHITE);
}

void LGFXDisplay::setTextSize(int sz) {
  buffer.setTextSize(sz);
}

void LGFXDisplay::setColor(Color c) {
  // The draw calls interpret _color as RGB565, so build 565 values here. (Using
  // color888() stored a 24-bit value whose low 16 bits were read as 565, dropping
  // the red byte -- red->black, green->yellow, near-white->pink, etc.)
  switch (c) {
    case DARK:   _color = buffer.color565(0, 0, 0);       break;
    case LIGHT:  _color = buffer.color565(255, 255, 255); break;
    case RED:    _color = buffer.color565(255, 0, 0);     break;
    case GREEN:  _color = buffer.color565(0, 255, 0);     break;
    case BLUE:   _color = buffer.color565(0, 0, 255);     break;
    case YELLOW: _color = buffer.color565(255, 255, 0);   break;
    case ORANGE: _color = buffer.color565(255, 165, 0);   break;
    default:     _color = buffer.color565(255, 255, 255);
  }
  buffer.setTextColor(_color);
}

void LGFXDisplay::setColorRGB(uint8_t r, uint8_t g, uint8_t b) {
  _color = buffer.color565(r, g, b);   // draw calls read _color as RGB565
  buffer.setTextColor(_color);
}

void LGFXDisplay::setCursor(int x, int y) {
  buffer.setCursor(x, y);
}

void LGFXDisplay::print(const char* str) {
  buffer.println(str);
//  Serial.println(str);
}

void LGFXDisplay::fillRect(int x, int y, int w, int h) {
  buffer.fillRect(x, y, w, h, _color);
}

void LGFXDisplay::drawRect(int x, int y, int w, int h) {
  buffer.drawRect(x, y, w, h, _color);
}

void LGFXDisplay::drawXbm(int x, int y, const uint8_t* bits, int w, int h) {
  buffer.drawBitmap(x, y, bits, w, h, _color);
}

uint16_t LGFXDisplay::getTextWidth(const char* str) {
  return buffer.textWidth(str);
}

void LGFXDisplay::endFrame() {
  display->startWrite();
  if (UI_ZOOM != 1) {
    buffer.pushRotateZoom(display, display->width()/2, display->height()/2 , 0, UI_ZOOM, UI_ZOOM);
  } else {
    buffer.pushSprite(display, 0, 0);
  }
  display->endWrite();
}

bool LGFXDisplay::getTouch(int *x, int *y) {
  lgfx::v1::touch_point_t point;
  display->getTouch(&point);
  if (UI_ZOOM != 1) {
    *x = point.x / UI_ZOOM;
    *y = point.y / UI_ZOOM;
  } else {
    *x = point.x;
    *y = point.y;
  }
  return (*x >= 0) && (*y >= 0);
}