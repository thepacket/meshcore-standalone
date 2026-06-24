#pragma once

// LVGL UITask for the LilyGo T-Deck companion radio (the Android-style Material
// UI). Implements AbstractUITask so MyMesh can drive it exactly like the bitmap
// ui-new/UITask; selected at build time via -D TDECK_LVGL_UI (see platformio.ini).
//
// Phase 1: brings LVGL up on the panel (flush + touch + tick + screen manager)
// and renders the ui-lvgl screens. Live data binding (the AbstractUITask event
// hooks) is wired incrementally in later phases.
#include "../AbstractUITask.h"

class UITask : public AbstractUITask {
  DisplayDriver* _display = nullptr;
  SensorManager* _sensors = nullptr;
  NodePrefs*     _node_prefs = nullptr;
  bool           _lvgl_ready = false;
  int            _last_rssi = 0;   // most recent RX RSSI (for home signal bars)

public:
  NodePrefs* nodePrefs() const { return _node_prefs; }
  int        lastRssi() const { return _last_rssi; }

public:
  UITask(mesh::MainBoard* board, BaseSerialInterface* serial)
    : AbstractUITask(board, serial) {}

  // Called from setup() after the display is up (same signature as ui-new).
  void begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs);

  // --- AbstractUITask ---
  void loop() override;
  void msgRead(int msgcount) override {}
  void newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) override {}
  void notify(UIEventType t = UIEventType::none) override {}
  void onRawRx(float snr, float rssi, const uint8_t* raw, int len) override { _last_rssi = (int)rssi; }
};
