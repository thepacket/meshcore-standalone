#pragma once

#define RADIOLIB_STATIC_ONLY 1
#include <RadioLib.h>
#include <helpers/radiolib/RadioLibWrappers.h>
#include <helpers/radiolib/CustomSX1262Wrapper.h>
#include <TDeckBoard.h>
#include <helpers/AutoDiscoverRTCClock.h>
#include <helpers/SensorManager.h>
#ifdef DISPLAY_CLASS
  // DISPLAY_CLASS is the "display present" sentinel (ST7789LCDDisplay by default).
  // The companion standalone UI selects a touch-capable display via TDECK_TOUCH_UI;
  // DISPLAY_IMPL is the concrete type actually instantiated.
  #if defined(TDECK_TOUCH_UI)
    #include "TDeckDisplay.h"          // LovyanGFX + GT911 touch (companion standalone UI)
    #define DISPLAY_IMPL TDeckDisplay
  #else
    #include <helpers/ui/ST7789LCDDisplay.h>
    #define DISPLAY_IMPL DISPLAY_CLASS
  #endif
  #include <helpers/ui/MomentaryButton.h>
  #if defined(UI_HAS_KEYBOARD)
    #include "TDeckKeyboard.h"
  #endif
#endif
#include "helpers/sensors/EnvironmentSensorManager.h"
#include "helpers/sensors/MicroNMEALocationProvider.h"

extern TDeckBoard board;
extern WRAPPER_CLASS radio_driver;
extern AutoDiscoverRTCClock rtc_clock;
extern EnvironmentSensorManager sensors;

#ifdef DISPLAY_CLASS
  extern DISPLAY_IMPL display;
  extern MomentaryButton user_btn;
  #if defined(UI_HAS_TRACKBALL)
    extern MomentaryButton trackball_up, trackball_down, trackball_left, trackball_right;
  #endif
  #if defined(UI_HAS_KEYBOARD)
    extern TDeckKeyboard tdeck_keyboard;
  #endif
#endif

bool radio_init();
mesh::LocalIdentity radio_new_identity();

// Hand the shared SPI output pins back to the LoRa radio (call after each display
// flush). See target.cpp -- the radio (SPI2) and LovyanGFX display (SPI3) share
// the physical SCLK/MOSI pins on the T-Deck.
void radio_spi_claim();

// ---- microSD (read + write) ------------------------------------------------
// The card sits on the same SPI2 bus as the radio (CS=39). Each helper mounts on
// demand (read/write), does its work, then re-claims the bus for the radio.
// Returns false / -1 when no card is present. Used by the Files browser and by
// features that persist data to the card (message history, exports, map tiles).
#ifndef P_SDCARD_CS
  #define P_SDCARD_CS 39
#endif
// ---- I2S speaker (notification tones) ---------------------------------------
// The T-Deck's MAX98357 amp sits on I2S (BCK 7, WS 5, DOUT 6). tdeck_tones_start
// synthesizes a short melody of (freq_hz, dur_ms) pairs into the DMA buffer and
// returns immediately (DMA plays it out); the driver is installed per chirp and
// released by tdeck_tones_poll() (call it from the UI loop) so no RAM is held
// between notifications. Melodies longer than ~250ms will block in i2s_write.
// amplitude = peak sample value (0..32767); ~1800 soft, ~5000 medium, ~12000 loud.
bool tdeck_tones_start(const uint16_t* pairs, int n_notes, int amplitude);
void tdeck_tones_poll();

struct SdEntry { char name[64]; bool is_dir; uint32_t size; };
bool     tdeck_sd_ok();                                        // card mounted & usable
uint64_t tdeck_sd_bytes(uint64_t* used_out);                   // total bytes (fills used); 0 if none
int      tdeck_sd_list(const char* path, SdEntry* out, int max);  // dir entries; -1 if no card
// write helpers (RW): return false on failure / no card
bool     tdeck_sd_write(const char* path, const uint8_t* data, size_t len, bool append);
bool     tdeck_sd_mkdir(const char* path);
bool     tdeck_sd_remove(const char* path);                    // file or empty dir
int      tdeck_sd_read(const char* path, uint8_t* buf, int max);  // bytes read, -1 on error
bool     tdeck_sd_format();                                    // reformat as FAT32 (destructive!)
