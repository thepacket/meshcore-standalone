#pragma once
// SPDX-License-Identifier: GPL-3.0-or-later
// Ported and adapted from wadamesh (Kaj Schittecat and contributors),
// GPL-3.0-or-later. meshcore-standalone is distributed under GPL-3.0-or-later;
// see LICENSE and NOTICE at the repo root.
// ============================================================================
// Web UI mirror — lock-free bridge between the LVGL UI thread and the WebScreen
// network task. The device streams its live framebuffer (every LVGL flush) to a
// browser <canvas> over a lightweight WebSocket server, and the browser sends
// taps + keys back. The browser renders the device's OWN UI verbatim, so any
// firmware UI change shows up remotely with zero web-side code.
//
// Two independent single-producer / single-consumer channels, so no locks:
//   * display : LVGL flush thread PRODUCES rects  -> net task CONSUMES + sends
//   * input   : net task PRODUCES pointers/keys    -> LVGL indev CONSUMES
//
// Header is lvgl-free (stdint only) so WebScreen can include it without LVGL.
// Ported from wadamesh WebMirror, trimmed to the framebuffer + pointer + key
// channels (the T-Deck has a fixed 320x240 landscape panel and a real keyboard,
// so the remote-orientation / exit / soft-keyboard / terminal extras are gone).
// ============================================================================
#include <stdint.h>
#include <stddef.h>

class WebMirror {
public:
  void begin(size_t ring_bytes = 0);   // lazily allocate the PSRAM display ring (0 = default size)
  void setEnabled(bool en);            // user opt-in (Settings > Remote screen toggle)
  bool enabled() const { return _enabled; }

  void setScreenSize(int w, int h) { _scr_w = (uint16_t)w; _scr_h = (uint16_t)h; }
  uint16_t screenW() const { return _scr_w; }
  uint16_t screenH() const { return _scr_h; }
  size_t   ringBytes() const { return _cap; }   // 0 = ring alloc failed (feature can't stream)

  // True only when we should capture/stream: opted in, ring ready, >=1 client.
  // The LVGL flush hook checks this first, so the feature costs one bool load
  // when off.
  bool active() const { return _enabled && _ring && _clients > 0; }

  // ---- display: PRODUCER (LVGL flush thread) ----
  // Push one framebuffer rect as header+body. Returns false if dropped (ring
  // backed up); a drop arms a full-repaint so the mirror self-heals.
  bool pushFrame(const uint8_t* hdr, size_t hdr_len, const uint8_t* body, size_t body_len);

  // ---- display: CONSUMER (network task) ----
  size_t popFrame(uint8_t* dst, size_t max_len);   // 0 if the ring is empty
  bool empty() const { return _head == _tail; }    // ring fully drained -> producer backpressure gate

  // Full-screen repaint handshake (new client / after a drop). The UI thread
  // consumes takeFullRepaint() and invalidates the screen.
  void requestFullRepaint() { _need_full = true; }
  bool takeFullRepaint() { if (!_need_full) return false; _need_full = false; return true; }

  // Mirror client count, refreshed by the WS server each loop.
  void noteClients(int n) { _clients = n; }
  int  clients() const { return _clients; }

  // ---- pointer: PRODUCER (network task) / CONSUMER (LVGL indev) ----
  void pushPointer(int16_t x, int16_t y, uint8_t pressed);
  bool readPointer(int16_t* x, int16_t* y, bool* pressed) const;  // latest known state

  // ---- keyboard: PRODUCER (network task) / CONSUMER (LVGL/UI thread) ----
  void pushKey(uint16_t cp);       // codepoint, or a control code (0x08 backspace, 0x0D enter)
  bool popKey(uint16_t* cp);       // false if the queue is empty

private:
  uint8_t* _ring = nullptr;
  size_t   _cap  = 0;
  volatile size_t _head = 0;       // producer writes, consumer reads
  volatile size_t _tail = 0;       // consumer writes, producer reads
  volatile bool   _enabled   = false;
  volatile bool   _need_full = false;
  volatile int    _clients   = 0;
  uint16_t _scr_w = 0, _scr_h = 0;
  volatile int16_t _px = 0, _py = 0;
  volatile uint8_t _ppressed = 0;
  volatile uint8_t _pknown   = 0;

  static const int KEY_RING = 32;   // typed keys awaiting the UI thread (SPSC)
  uint16_t _keys[KEY_RING];
  volatile uint8_t _khead = 0, _ktail = 0;
};

extern WebMirror g_web_mirror;
