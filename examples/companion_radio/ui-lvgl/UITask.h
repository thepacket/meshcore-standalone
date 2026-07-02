#pragma once

// LVGL UITask for the LilyGo T-Deck companion radio (the Android-style Material
// UI). Implements AbstractUITask so MyMesh can drive it exactly like the bitmap
// ui-new/UITask; selected at build time via -D TDECK_LVGL_UI (see platformio.ini).
//
// Phase 1: brings LVGL up on the panel (flush + touch + tick + screen manager)
// and renders the ui-lvgl screens. Live data binding (the AbstractUITask event
// hooks) is wired incrementally in later phases.
#include "../AbstractUITask.h"

// records a received packet into the packet-monitor ring buffer (UITask.cpp)
void ui_log_packet(float snr, float rssi, const uint8_t* raw, int len);
// records a text message (incoming or outgoing) into the chat store (UITask.cpp).
// peer6 = 6-byte sender pubkey prefix for DMs (NULL for channel messages).
// path_len is the encoded route length of an inbound message (0xFF = direct/unknown).
void ui_store_message(bool is_channel, int channel_idx, const uint8_t* peer6,
                      const char* who, const char* text, bool outgoing, uint8_t path_len = 0xFF);
// records a trace-route result for the on-device trace screen (UITask.cpp)
void ui_store_trace(uint32_t tag, const uint8_t* path_hashes, const uint8_t* path_snrs,
                    uint8_t path_len, uint8_t path_sz, int8_t final_snr_q);
// repeater-admin async results (UITask.cpp)
void ui_rep_on_login(const uint8_t* pk6, bool ok, uint8_t perms);
void ui_rep_on_status(const uint8_t* pk6, const uint8_t* data, uint8_t len);
void ui_rep_on_cmdreply(const uint8_t* pk6, const char* text);
// telemetry response for the peer card (UITask.cpp)
void ui_peer_on_telemetry(const uint8_t* pk6, const uint8_t* data, uint8_t len);
// delivery ack for one of our outbound DMs (UITask.cpp)
void ui_msg_confirmed(uint32_t ack);

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
  void onMsgSendConfirmed(uint32_t ack, uint32_t trip_millis) override { ui_msg_confirmed(ack); }
  void notify(UIEventType t = UIEventType::none) override;   // speaker chirps (UITask.cpp)
  void onRawRx(float snr, float rssi, const uint8_t* raw, int len) override {
    _last_rssi = (int)rssi;
    ui_log_packet(snr, rssi, raw, len);
  }
  void onTextMessage(bool is_channel, int channel_idx, const uint8_t* dm_prefix6,
                     const char* dm_name, const char* text, uint32_t timestamp,
                     uint8_t path_len, int8_t snr_q) override {
    ui_store_message(is_channel, channel_idx, dm_prefix6, dm_name, text, false, path_len);
    // Chirp from here, not from MyMesh's notify(): that one is suppressed while
    // "a companion app is connected", and the USB serial interface always
    // reports connected -- on a standalone device the chirp must always fire.
    notify(is_channel ? UIEventType::channelMessage : UIEventType::contactMessage);
  }
  void onTraceResult(uint32_t tag, const uint8_t* path_hashes, const uint8_t* path_snrs,
                     uint8_t path_len, uint8_t path_sz, int8_t final_snr_q) override {
    ui_store_trace(tag, path_hashes, path_snrs, path_len, path_sz, final_snr_q);
  }
  void onLoginResult(const uint8_t* pk6, bool ok, uint8_t perms) override { ui_rep_on_login(pk6, ok, perms); }
  void onStatusResponse(const uint8_t* pk6, const uint8_t* data, uint8_t len) override { ui_rep_on_status(pk6, data, len); }
  void onTelemetryResponse(const uint8_t* pk6, const uint8_t* data, uint8_t len) override { ui_peer_on_telemetry(pk6, data, len); }
  void onCommandReply(const uint8_t* pk6, const char* text) override { ui_rep_on_cmdreply(pk6, text); }
};
