#pragma once
// ============================================================================
// WebScreen — a lightweight, self-contained WebSocket server that mirrors the
// T-Deck's live LVGL screen to a browser <canvas> and injects the browser's
// taps + keystrokes back into the UI (a "VNC over the web" remote control).
//
// It serves one self-contained HTML page at http://<device-ip>:<port>/ and
// upgrades GET /mirror?pin=NNNN to a WebSocket carrying the framebuffer bands
// (device -> browser) and pointer/key events (browser -> device). All screen
// pixels + input flow through g_web_mirror (a lock-free SPSC bridge), so this
// server never touches LVGL directly and runs entirely on its own FreeRTOS
// task, off the UI thread.
//
// Access is PIN-gated: the /mirror upgrade is rejected unless ?pin= matches the
// current PIN (shown on-device in Settings > Remote screen). Plain HTTP, LAN
// only — not exposed beyond the local Wi-Fi.
// ============================================================================
#include <stdint.h>

class WebScreen {
public:
  void begin(uint16_t port = 8080);   // spawn the network task (idle until enabled)
  void setEnabled(bool en);           // gated additionally on Wi-Fi being connected
  bool enabled() const { return _enabled; }
  void setPin(const char* pin);       // 4-6 digit access PIN required in the /mirror URL
  const char* pin() const { return _pin; }
  int  clients() const { return _clients; }   // live mirror viewers
  uint16_t port() const { return _port; }

  void taskLoop();   // internal: the FreeRTOS task entry runs this forever

private:
  volatile bool _enabled = false;
  uint16_t _port = 8080;
  char _pin[8] = "";
  volatile int _clients = 0;
  bool _task_started = false;
};

extern WebScreen g_web_screen;
