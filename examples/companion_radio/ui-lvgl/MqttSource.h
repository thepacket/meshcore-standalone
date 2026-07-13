#pragma once

// Observe-only MQTT subscriber for the meshcore.ca live-packet feed.
//
// Mirrors the Android app's MqttPacketSource: connects to a broker over
// MQTT-3.1.1-on-WebSocket+TLS (the meshcore.ca brokers only accept 443), and
// for every JSON message hex-decodes the `raw` packet and hands it to the LVGL
// packet monitor via ui_log_packet_net(). It never publishes and never routes
// packets onto the LoRa mesh -- it is a passive display feed only.
//
// The ESP-IDF esp-mqtt client runs the socket on its own task and auto-
// reconnects; these entry points just start/stop/interrogate it. UITask owns
// the persisted settings (broker/region/credentials) and gates start/stop on
// Wi-Fi being connected (see ui_mqtt_poll()).

#include <stdint.h>

// Connection state, shared with the settings UI status row.
enum {
  MQTT_SRC_OFF = 0,      // stopped / not started
  MQTT_SRC_CONNECTING,   // socket up, awaiting CONNACK + SUBACK
  MQTT_SRC_CONNECTED,    // subscribed and receiving
  MQTT_SRC_AUTH_ERR,     // broker rejected credentials -- stopped, won't retry
};

// Start (or restart) the subscriber. `url` is a wss:// broker URI, `topic` the
// subscription filter (e.g. "meshcore/YOW/+/packets"); user/pass are required
// by the JWT-gated brokers. Safe to call repeatedly; a no-op if the target is
// unchanged and already running.
void mqtt_src_start(const char* url, const char* topic, const char* user, const char* pass);

// Tear the connection down (config changed, disabled, or Wi-Fi dropped).
void mqtt_src_stop();

// Current connection state (one of the MQTT_SRC_* values above).
int  mqtt_src_state();

// Count of packets injected since start (for the status row).
uint32_t mqtt_src_received();

// Human-readable status line for the settings screen, e.g.
// "connected  1234 pkts" / "connecting..." / "not authorized" / "off".
void mqtt_src_status(char* out, int len);

// Inject packets received since the last call into the LVGL packet monitor.
// MUST be called from the UI/LVGL task: esp-mqtt delivers on its own task, so
// decoding is queued there and the actual ui_log_packet_net() injection is
// deferred to here, keeping the packet ring single-threaded (like the RF path).
void mqtt_src_drain(void);
