#pragma once

#include <Wire.h>

// LilyGo T-Deck QWERTY keyboard: a separate ESP32-C3 module on the shared I2C
// bus that returns the ASCII of the last pressed key (0 if none) on a 1-byte read.
#ifndef TDECK_KB_ADDR
  #define TDECK_KB_ADDR 0x55
#endif

class TDeckKeyboard {
public:
  // The shared Wire bus is already begun in radio_init() (Wire.begin(18, 8)),
  // so nothing to init here.
  void begin() {}

  // returns the pressed key (ASCII), or 0 if no key is available
  char read() {
    if (Wire.requestFrom((uint8_t)TDECK_KB_ADDR, (uint8_t)1) == 0) return 0;
    if (!Wire.available()) return 0;
    int v = Wire.read();
    return (v > 0) ? (char)v : 0;
  }
};
