#include <Arduino.h>
#include "target.h"

TDeckBoard board;

#if defined(P_LORA_SCLK)
  static SPIClass spi;
  RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY, spi);
#else
  RADIO_CLASS radio = new Module(P_LORA_NSS, P_LORA_DIO_1, P_LORA_RESET, P_LORA_BUSY);
#endif

WRAPPER_CLASS radio_driver(radio, board);

ESP32RTCClock fallback_clock;
AutoDiscoverRTCClock rtc_clock(fallback_clock);
MicroNMEALocationProvider gps(Serial1, &rtc_clock);
EnvironmentSensorManager sensors(gps);

#ifdef DISPLAY_CLASS
  DISPLAY_IMPL display;
  MomentaryButton user_btn(PIN_USER_BTN, 1000, true);
  #if defined(UI_HAS_TRACKBALL)
    // trackball directions: active-low momentary pulses with internal pull-up.
    // GPIO numbers vary by board revision -- confirm/swap on hardware.
    // multiclick=false: the trackball emits fast pulses as it rolls; multi-click
    // coalescing (the default) merges them into double/triple-clicks the UI drops,
    // so each pulse must fire an immediate single CLICK instead.
    MomentaryButton trackball_up(PIN_TRACKBALL_UP, 1000, true, true, false);
    MomentaryButton trackball_down(PIN_TRACKBALL_DOWN, 1000, true, true, false);
    MomentaryButton trackball_left(PIN_TRACKBALL_LEFT, 1000, true, true, false);
    MomentaryButton trackball_right(PIN_TRACKBALL_RIGHT, 1000, true, true, false);
  #endif
  #if defined(UI_HAS_KEYBOARD)
    TDeckKeyboard tdeck_keyboard;
  #endif
#endif

bool radio_init() {
  Wire.begin(18, 8);          // shared I2C (keyboard 0x55, GT911 touch 0x5D); must
  fallback_clock.begin();     // be begun before the RTC auto-discover probes it
  rtc_clock.begin(Wire);

#if defined(P_LORA_SCLK)
  return radio.std_init(&spi);
#else
  return radio.std_init();
#endif
}

mesh::LocalIdentity radio_new_identity() {
  RadioNoiseListener rng(radio);
  return mesh::LocalIdentity(&rng); // create new random identity
}
