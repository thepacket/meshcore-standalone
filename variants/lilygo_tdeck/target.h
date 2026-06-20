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
