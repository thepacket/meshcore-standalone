#pragma once

// LovyanGFX display for the LilyGo T-Deck (Plus), wrapping MeshCore's portable
// LGFXDisplay. Touch is NOT handled by LovyanGFX (its GT911 driver installs its
// own ESP-IDF I2C driver which collides with the Arduino Wire bus the keyboard
// and RTC use); instead TDeckDisplay::getTouch() reads the GT911 directly over
// the shared Arduino Wire (single I2C driver -- no collision).

#include <Wire.h>
#include <helpers/ui/LGFXDisplay.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX_TDeck : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel;
  lgfx::Bus_SPI      _bus;
  lgfx::Light_PWM    _light;

public:
  LGFX_TDeck() {
    {  // SPI bus -- shares the physical SCLK/MOSI pins with the LoRa radio but
       // uses a SEPARATE SPI host (SPI3) so the display's ESP-IDF spi_master
       // driver never tears down the radio's Arduino-HAL bus on SPI2. The two
       // only contend for the shared output pins, handed off per flush.
      auto cfg = _bus.config();
      cfg.spi_host   = SPI3_HOST;
      cfg.spi_mode   = 0;
      cfg.freq_write = 40000000;
      cfg.freq_read  = 16000000;
      cfg.spi_3wire  = false;
      cfg.use_lock   = true;
      cfg.dma_channel = SPI_DMA_CH_AUTO;
      cfg.pin_sclk = 40;   // PIN_TFT_SCL
      cfg.pin_mosi = 41;   // PIN_TFT_SDA
      cfg.pin_miso = 38;   // shared SPI MISO
      cfg.pin_dc   = 11;   // PIN_TFT_DC
      _bus.config(cfg);
      _panel.setBus(&_bus);
    }
    {  // ST7789 panel (320x240 landscape via setRotation(1) in LGFXDisplay::begin)
      auto cfg = _panel.config();
      cfg.pin_cs   = 12;   // PIN_TFT_CS
      cfg.pin_rst  = -1;   // PIN_TFT_RST (tied)
      cfg.pin_busy = -1;
      cfg.memory_width  = 240;
      cfg.memory_height = 320;
      cfg.panel_width   = 240;
      cfg.panel_height  = 320;
      cfg.offset_x = 0;
      cfg.offset_y = 0;
      cfg.offset_rotation = 0;
      cfg.readable   = false;
      cfg.invert     = true;
      cfg.rgb_order  = false;
      cfg.dlen_16bit = false;
      cfg.bus_shared = true;
      _panel.config(cfg);
    }
    {  // backlight
      auto cfg = _light.config();
      cfg.pin_bl = 42;     // PIN_TFT_LEDA_CTL
      cfg.freq   = 44100;
      cfg.pwm_channel = 7;
      _light.config(cfg);
      _panel.setLight(&_light);
    }
    setPanel(&_panel);
  }
};

// GT911 capacitive touch read over the shared Arduino Wire bus (addr 0x5D).
class TDeckDisplay : public LGFXDisplay {
  LGFX_TDeck _disp;

  static constexpr uint8_t GT911_ADDR = 0x5D;

  bool gt911_read(uint16_t reg, uint8_t* buf, uint8_t len) {
    Wire.beginTransmission(GT911_ADDR);
    Wire.write((uint8_t)(reg >> 8));
    Wire.write((uint8_t)(reg & 0xFF));
    if (Wire.endTransmission(false) != 0) return false;   // repeated-start
    if (Wire.requestFrom(GT911_ADDR, len) != len) return false;
    for (uint8_t i = 0; i < len; i++) buf[i] = Wire.read();
    return true;
  }
  void gt911_clear_status() {
    Wire.beginTransmission(GT911_ADDR);
    Wire.write(0x81); Wire.write(0x4E); Wire.write((uint8_t)0x00);
    Wire.endTransmission();
  }

public:
  TDeckDisplay() : LGFXDisplay(320, 240, _disp) {}

  bool hasTouch() override { return true; }

  bool getTouch(int* x, int* y) override {
    uint8_t status;
    if (!gt911_read(0x814E, &status, 1)) return false;
    bool ready = status & 0x80;
    uint8_t points = status & 0x0F;
    bool touched = false;
    if (ready && points > 0) {
      uint8_t d[8];
      if (gt911_read(0x8150, d, 8)) {
        // GT911 reports portrait-native coords (240x320), axes swapped vs the
        // landscape screen: native-Y -> screen-X, native-X (inverted) -> screen-Y.
        int sx = d[2] | (d[3] << 8);
        int sy = 240 - (d[0] | (d[1] << 8));
        if (sx < 0) sx = 0; else if (sx > 319) sx = 319;
        if (sy < 0) sy = 0; else if (sy > 239) sy = 239;
        *x = sx; *y = sy;
        touched = true;
      }
    }
    if (ready) gt911_clear_status();
    return touched;
  }
};
