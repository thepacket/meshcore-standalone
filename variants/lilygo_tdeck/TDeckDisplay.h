#pragma once

// LovyanGFX display + GT911 capacitive touch for the LilyGo T-Deck (Plus).
// Wraps MeshCore's portable LGFXDisplay so the standalone touch UI works through
// the normal DisplayDriver/UIScreen abstraction.
//
// NOTE: pin/rotation/touch-address values below match the common T-Deck wiring,
// but some are board-revision dependent -- confirm on hardware (see plan risks):
//   - touch I2C address (0x5D vs 0x14)
//   - display rotation / touch offset_rotation (orientation)
//   - SPI host shared with the LoRa radio

#include <helpers/ui/LGFXDisplay.h>

#define LGFX_USE_V1
#include <LovyanGFX.hpp>

class LGFX_TDeck : public lgfx::LGFX_Device {
  lgfx::Panel_ST7789 _panel;
  lgfx::Bus_SPI      _bus;
  lgfx::Light_PWM    _light;
  lgfx::Touch_GT911  _touch;

public:
  LGFX_TDeck() {
    {  // SPI bus (shared with the LoRa radio; LovyanGFX manages the TFT CS)
      auto cfg = _bus.config();
      cfg.spi_host   = SPI2_HOST;
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
    {  // GT911 capacitive touch on the shared I2C bus (SDA 18 / SCL 8)
      auto cfg = _touch.config();
      cfg.x_min = 0;   cfg.x_max = 319;
      cfg.y_min = 0;   cfg.y_max = 239;
      cfg.pin_int = -1;
      cfg.pin_rst = -1;
      cfg.bus_shared = true;
      cfg.offset_rotation = 0;
      cfg.i2c_port = 0;        // same port radio_init() uses for Wire(18,8)
      cfg.i2c_addr = 0x5D;     // GT911 (alternative 0x14)
      cfg.pin_sda = 18;
      cfg.pin_scl = 8;
      cfg.freq    = 400000;
      _touch.config(cfg);
      _panel.setTouch(&_touch);
    }
    setPanel(&_panel);
  }
};

class TDeckDisplay : public LGFXDisplay {
  LGFX_TDeck _disp;

public:
  TDeckDisplay() : LGFXDisplay(320, 240, _disp) {}
};
