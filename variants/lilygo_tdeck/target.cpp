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

// Hand the shared SPI output pins back to the LoRa radio. On the T-Deck the radio
// (Arduino SPI HAL on SPI2) and the LovyanGFX TFT (ESP-IDF spi_master on SPI3)
// run on separate hosts but share the physical SCLK/MOSI pins; each display flush
// leaves those pins routed to the display, so this re-asserts the radio's SPI2
// bus afterwards. Only touches SPI2 -- harmless to the SPI3 display bus. The
// SX1262 receives autonomously between commands, so it only needs the pins at the
// moments it issues/reads SPI.
void radio_spi_claim() {
#if defined(P_LORA_SCLK)
  spi.end();
  spi.begin(P_LORA_SCLK, P_LORA_MISO, P_LORA_MOSI);
#endif
}

// ---- microSD (read + write) ------------------------------------------------
// The card shares the radio's SPI2 bus. We keep it mounted once (CS-selected per
// transaction), and re-claim the bus for the radio after every access so RX/TX
// is not disturbed (same discipline as the display flush).
#if defined(P_LORA_SCLK) && !defined(TDECK_NO_SD)
#include <SD.h>
extern "C" {
  #include "ff.h"     // FatFs f_mkfs for on-device FAT32 format
}

static bool s_sd_mounted = false;
static bool sd_mount() {
  if (s_sd_mounted) return true;
  // SD on the shared SPI2 bus; modest 8 MHz clock to stay reliable alongside the radio
  s_sd_mounted = SD.begin(P_SDCARD_CS, spi, 8000000U);
  radio_spi_claim();                       // hand the bus back to the radio
  return s_sd_mounted;
}
bool tdeck_sd_ok() { return sd_mount(); }

uint64_t tdeck_sd_bytes(uint64_t* used_out) {
  if (!sd_mount()) { if (used_out) *used_out = 0; return 0; }
  uint64_t total = SD.totalBytes(), used = SD.usedBytes();
  radio_spi_claim();
  if (used_out) *used_out = used;
  return total;
}
int tdeck_sd_list(const char* path, SdEntry* out, int max) {
  if (!sd_mount()) return -1;
  int n = 0;
  File dir = SD.open(path && path[0] ? path : "/");
  if (dir && dir.isDirectory()) {
    for (File f = dir.openNextFile(); f && n < max; f = dir.openNextFile()) {
      const char* nm = f.name();
      const char* base = strrchr(nm, '/'); if (base) nm = base + 1;   // leaf name only
      strncpy(out[n].name, nm, sizeof(out[n].name) - 1); out[n].name[sizeof(out[n].name) - 1] = 0;
      out[n].is_dir = f.isDirectory();
      out[n].size = f.isDirectory() ? 0 : (uint32_t)f.size();
      n++;
      f.close();
    }
  }
  if (dir) dir.close();
  radio_spi_claim();
  return n;
}
bool tdeck_sd_write(const char* path, const uint8_t* data, size_t len, bool append) {
  if (!sd_mount()) return false;
  File f = SD.open(path, append ? FILE_APPEND : FILE_WRITE);
  bool ok = false;
  if (f) { ok = (f.write(data, len) == len); f.close(); }
  radio_spi_claim();
  return ok;
}
bool tdeck_sd_mkdir(const char* path) {
  if (!sd_mount()) return false;
  bool ok = SD.mkdir(path);
  radio_spi_claim();
  return ok;
}
bool tdeck_sd_remove(const char* path) {
  if (!sd_mount()) return false;
  bool ok = SD.remove(path) || SD.rmdir(path);
  radio_spi_claim();
  return ok;
}
int tdeck_sd_read(const char* path, uint8_t* buf, int max) {
  if (!sd_mount()) return -1;
  int n = -1;
  File f = SD.open(path, FILE_READ);
  if (f) { n = f.read(buf, max); f.close(); }
  radio_spi_claim();
  return n;
}
// Reformat the card as FAT32 (destructive). The SD is the only FatFs volume
// (SPIFFS is the internal FS), so it is drive "0:". Holds the shared bus for the
// duration, then re-claims it for the radio.
bool tdeck_sd_format() {
  if (!sd_mount()) return false;                 // registers the FatFs drive
  // work buffer from PSRAM (internal DRAM is scarce); the SD SPI path copies
  // bytes through the driver, so a PSRAM buffer is fine here
  uint8_t* work = (uint8_t*)ps_malloc(4096);
  if (!work) work = (uint8_t*)malloc(4096);
  if (!work) return false;
  FRESULT res = f_mkfs("0:", FM_FAT32, 0, work, 4096);
  free(work);
  SD.end(); s_sd_mounted = false;                // drop the stale mount
  bool ok = (res == FR_OK);
  if (ok) { s_sd_mounted = SD.begin(P_SDCARD_CS, spi, 8000000U); ok = s_sd_mounted; }
  radio_spi_claim();
  return ok;
}
#else
bool     tdeck_sd_ok() { return false; }
uint64_t tdeck_sd_bytes(uint64_t* u) { if (u) *u = 0; return 0; }
int      tdeck_sd_list(const char*, SdEntry*, int) { return -1; }
bool     tdeck_sd_write(const char*, const uint8_t*, size_t, bool) { return false; }
bool     tdeck_sd_mkdir(const char*) { return false; }
bool     tdeck_sd_remove(const char*) { return false; }
int      tdeck_sd_read(const char*, uint8_t*, int) { return -1; }
bool     tdeck_sd_format() { return false; }
#endif
