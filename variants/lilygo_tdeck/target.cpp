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
    MomentaryButton trackball_up(PIN_TRACKBALL_UP, 1000, true, true);
    MomentaryButton trackball_down(PIN_TRACKBALL_DOWN, 1000, true, true);
    MomentaryButton trackball_left(PIN_TRACKBALL_LEFT, 1000, true, true);
    MomentaryButton trackball_right(PIN_TRACKBALL_RIGHT, 1000, true, true);
  #endif
  #if defined(UI_HAS_KEYBOARD)
    TDeckKeyboard tdeck_keyboard;
  #endif
#endif

bool radio_init() {
  fallback_clock.begin();
  rtc_clock.begin(Wire);
  Wire.begin(18, 8);

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
