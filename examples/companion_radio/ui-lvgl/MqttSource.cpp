// Observe-only MQTT subscriber for the meshcore.ca live-packet feed. See
// MqttSource.h for the design rationale.
//
// Transport: the ESP-IDF esp-mqtt client, which speaks MQTT 3.1.1 over
// WebSocket+TLS when handed a wss:// URI (the meshcore.ca brokers only accept
// 443). Server certs are verified against the Arduino x509 bundle. The client
// runs on its own FreeRTOS task and auto-reconnects; decoded packets are pushed
// onto a queue and injected into the packet ring from the UI task in
// mqtt_src_drain(), so ui_log_packet_net() is only ever called single-threaded.

#include "MqttSource.h"

#include <Arduino.h>
#include <string.h>
#include <ArduinoJson.h>
#include <mqtt_client.h>
#include <esp_heap_caps.h>
#include <freertos/FreeRTOS.h>
#include <freertos/queue.h>

// Defined in UITask.cpp: merge a network-sourced packet into the monitor ring
// and the mesh Heard list (adverts only, never relayed onto the radio).
extern void ui_inject_net_packet(float snr, float rssi, const uint8_t* raw, int len, const char* region);

static const char* TAG = "MqttSource";

// Trust anchor for the meshcore.ca brokers: they serve Let's Encrypt certs
// (leaf -> "E8" ECDSA intermediate -> ISRG Root X1). We verify against this
// embedded root directly rather than the Arduino CA bundle, which isn't linked
// into this firmware (arduino_esp_crt_bundle_attach() fails at runtime). ISRG
// Root X1 is valid until 2035-06-04.
static const char ISRG_ROOT_X1[] =
"-----BEGIN CERTIFICATE-----\n"
"MIIFazCCA1OgAwIBAgIRAIIQz7DSQONZRGPgu2OCiwAwDQYJKoZIhvcNAQELBQAw\n"
"TzELMAkGA1UEBhMCVVMxKTAnBgNVBAoTIEludGVybmV0IFNlY3VyaXR5IFJlc2Vh\n"
"cmNoIEdyb3VwMRUwEwYDVQQDEwxJU1JHIFJvb3QgWDEwHhcNMTUwNjA0MTEwNDM4\n"
"WhcNMzUwNjA0MTEwNDM4WjBPMQswCQYDVQQGEwJVUzEpMCcGA1UEChMgSW50ZXJu\n"
"ZXQgU2VjdXJpdHkgUmVzZWFyY2ggR3JvdXAxFTATBgNVBAMTDElTUkcgUm9vdCBY\n"
"MTCCAiIwDQYJKoZIhvcNAQEBBQADggIPADCCAgoCggIBAK3oJHP0FDfzm54rVygc\n"
"h77ct984kIxuPOZXoHj3dcKi/vVqbvYATyjb3miGbESTtrFj/RQSa78f0uoxmyF+\n"
"0TM8ukj13Xnfs7j/EvEhmkvBioZxaUpmZmyPfjxwv60pIgbz5MDmgK7iS4+3mX6U\n"
"A5/TR5d8mUgjU+g4rk8Kb4Mu0UlXjIB0ttov0DiNewNwIRt18jA8+o+u3dpjq+sW\n"
"T8KOEUt+zwvo/7V3LvSye0rgTBIlDHCNAymg4VMk7BPZ7hm/ELNKjD+Jo2FR3qyH\n"
"B5T0Y3HsLuJvW5iB4YlcNHlsdu87kGJ55tukmi8mxdAQ4Q7e2RCOFvu396j3x+UC\n"
"B5iPNgiV5+I3lg02dZ77DnKxHZu8A/lJBdiB3QW0KtZB6awBdpUKD9jf1b0SHzUv\n"
"KBds0pjBqAlkd25HN7rOrFleaJ1/ctaJxQZBKT5ZPt0m9STJEadao0xAH0ahmbWn\n"
"OlFuhjuefXKnEgV4We0+UXgVCwOPjdAvBbI+e0ocS3MFEvzG6uBQE3xDk3SzynTn\n"
"jh8BCNAw1FtxNrQHusEwMFxIt4I7mKZ9YIqioymCzLq9gwQbooMDQaHWBfEbwrbw\n"
"qHyGO0aoSCqI3Haadr8faqU9GY/rOPNk3sgrDQoo//fb4hVC1CLQJ13hef4Y53CI\n"
"rU7m2Ys6xt0nUW7/vGT1M0NPAgMBAAGjQjBAMA4GA1UdDwEB/wQEAwIBBjAPBgNV\n"
"HRMBAf8EBTADAQH/MB0GA1UdDgQWBBR5tFnme7bl5AFzgAiIyBpY9umbbjANBgkq\n"
"hkiG9w0BAQsFAAOCAgEAVR9YqbyyqFDQDLHYGmkgJykIrGF1XIpu+ILlaS/V9lZL\n"
"ubhzEFnTIZd+50xx+7LSYK05qAvqFyFWhfFQDlnrzuBZ6brJFe+GnY+EgPbk6ZGQ\n"
"3BebYhtF8GaV0nxvwuo77x/Py9auJ/GpsMiu/X1+mvoiBOv/2X/qkSsisRcOj/KK\n"
"NFtY2PwByVS5uCbMiogziUwthDyC3+6WVwW6LLv3xLfHTjuCvjHIInNzktHCgKQ5\n"
"ORAzI4JMPJ+GslWYHb4phowim57iaztXOoJwTdwJx4nLCgdNbOhdjsnvzqvHu7Ur\n"
"TkXWStAmzOVyyghqpZXjFaH3pO3JLF+l+/+sKAIuvtd7u+Nxe5AW0wdeRlN8NwdC\n"
"jNPElpzVmbUq4JUagEiuTDkHzsxHpFKVK7q4+63SM1N95R1NbdWhscdCb+ZAJzVc\n"
"oyi3B43njTOQ5yOf+1CceWxG1bQVs5ZufpsMljq4Ui0/1lvh+wjChP4kqKOJ2qxq\n"
"4RgqsahDYVvTH9w7jXbyLeiNdd8XM2w9U/t7y0Ff/9yi0GE44Za4rF2LN9d11TPA\n"
"mRGunUHBcnWEvgJBQl9nJEiU0Zsnvgc/ubhPgXRR4Xq37Z0j4r7g1SgEEzwxA57d\n"
"emyPxgcYxn/eR44/KJ4EBs+lVDR3veyJm+kXQ99b21/+jh5Xos1AnX5iItreGCc=\n"
"-----END CERTIFICATE-----\n";

// Max raw bytes carried per queued packet -- matches the monitor ring's PKT_RAW.
#define MQ_RAW_MAX   192
#define MQ_QUEUE_LEN 8      // decoded packets buffered between drains (internal RAM)

struct MqPkt {
  float   snr;
  float   rssi;
  uint8_t len;
  uint8_t raw[MQ_RAW_MAX];
  char    region[16];   // region segment of the topic (meshcore/<region>/.../packets)
};

static esp_mqtt_client_handle_t s_client = nullptr;
static QueueHandle_t            s_queue  = nullptr;
// The decoded-packet queue storage lives in PSRAM (see mqtt_src_start): its
// ~1.6 KB would otherwise sit in the internal DRAM that Wi-Fi/TLS need. Only the
// small StaticQueue_t control block stays in internal .bss.
static StaticQueue_t            s_queue_ctrl;
static uint8_t*                 s_queue_store = nullptr;   // PSRAM
static volatile int             s_state  = MQTT_SRC_OFF;
static volatile uint32_t        s_recv   = 0;
static char                     s_last_err[48] = "";   // last transport/refuse error, for the status row

// Remember what we last connected with, so a repeated start() with identical
// settings is a no-op instead of a needless reconnect.
static char s_url[128] = "", s_topic[64] = "", s_user[64] = "", s_pass[256] = "";

// ---- hex decode -------------------------------------------------------------
static int hex_nibble(char c) {
  if (c >= '0' && c <= '9') return c - '0';
  if (c >= 'a' && c <= 'f') return c - 'a' + 10;
  if (c >= 'A' && c <= 'F') return c - 'A' + 10;
  return -1;
}

// Decode `hex` into `out` (capacity `cap`). Returns the byte count, or -1 if the
// string is empty, odd-length, over capacity, or contains a non-hex character.
static int hex_to_bytes(const char* hex, uint8_t* out, int cap) {
  int n = (int)strlen(hex);
  if (n < 2 || (n & 1)) return -1;
  int bytes = n / 2;
  if (bytes > cap) return -1;
  for (int i = 0; i < bytes; i++) {
    int hi = hex_nibble(hex[i * 2]), lo = hex_nibble(hex[i * 2 + 1]);
    if (hi < 0 || lo < 0) return -1;
    out[i] = (uint8_t)((hi << 4) | lo);
  }
  return bytes;
}

// ---- payload assembly + JSON parse (runs on the esp-mqtt task) --------------
// esp-mqtt can split a large publish across several MQTT_EVENT_DATA callbacks;
// reassemble into this scratch buffer before parsing. Packets are tiny (well
// under the 1 KB RX buffer) so this rarely spans fragments. Buffer lives in
// PSRAM (allocated in mqtt_src_start) to keep it out of internal DRAM.
#define MQ_ASM_MAX 1024
static char*   s_asm = nullptr;      // PSRAM
static int     s_asm_len = 0;
static bool    s_asm_skip = false;   // set when a message overflows the buffer

// The region (city/IATA) segment of the current message's topic:
// meshcore/<region>/<node>/packets. Set on the first data fragment (esp-mqtt
// only carries the topic there) and reused across any continuation fragments.
static char s_msg_region[16] = "";

// Copy the <region> segment of a (non-null-terminated) topic into out.
static void topic_region(const char* topic, int topic_len, char* out, int outn) {
  out[0] = 0;
  if (!topic || topic_len <= 0) return;
  const char* end = topic + topic_len;
  const char* p = topic;
  const char pref[] = "meshcore/";
  const int  pl = (int)sizeof(pref) - 1;
  if (topic_len >= pl && strncmp(topic, pref, pl) == 0) p += pl;   // skip the fixed prefix
  int i = 0;
  while (p < end && *p != '/' && i < outn - 1) out[i++] = *p++;
  out[i] = 0;
}

// Parse one complete JSON message and enqueue the decoded packet. Mirrors the
// Android parse(): the bytes are the hex `raw` field; SNR/RSSI are numeric
// (often string-encoded); observer status events with raw:"" are skipped.
static void parse_and_enqueue(const char* json, int len, const char* region) {
  JsonDocument doc;
  if (deserializeJson(doc, json, len)) return;         // not valid JSON -> ignore
  const char* hex = doc["raw"] | "";
  MqPkt pkt;
  int n = hex_to_bytes(hex, pkt.raw, MQ_RAW_MAX);
  if (n < 0) return;                                   // status msg / no bytes
  pkt.len  = (uint8_t)n;
  pkt.snr  = doc["SNR"].as<float>();                   // coerces numeric strings
  pkt.rssi = doc["RSSI"].as<float>();
  strncpy(pkt.region, region ? region : "", sizeof(pkt.region) - 1);
  pkt.region[sizeof(pkt.region) - 1] = 0;
  if (s_queue) xQueueSend(s_queue, &pkt, 0);           // drop silently if full
}

static void on_data(esp_mqtt_event_handle_t e) {
  if (e->current_data_offset == 0)                     // topic is only on the first fragment
    topic_region(e->topic, e->topic_len, s_msg_region, sizeof(s_msg_region));
  // Single-fragment fast path: the whole payload arrived in one event.
  if (e->current_data_offset == 0 && e->data_len == e->total_data_len) {
    parse_and_enqueue(e->data, e->data_len, s_msg_region);
    return;
  }
  // Multi-fragment: accumulate, then parse once complete. If the PSRAM buffer
  // wasn't allocated, skip (single-fragment messages still work above).
  if (e->current_data_offset == 0) {                   // first fragment
    s_asm_len = 0;
    s_asm_skip = (!s_asm || e->total_data_len > MQ_ASM_MAX);
  }
  if (!s_asm_skip && e->data_len > 0 &&
      s_asm_len + e->data_len <= MQ_ASM_MAX) {
    memcpy(s_asm + s_asm_len, e->data, e->data_len);
    s_asm_len += e->data_len;
  }
  if (e->current_data_offset + e->data_len >= e->total_data_len) {   // last
    if (!s_asm_skip) parse_and_enqueue(s_asm, s_asm_len, s_msg_region);
    s_asm_len = 0; s_asm_skip = false;
  }
}

// ---- esp-mqtt event handler (esp-mqtt task) ---------------------------------
static void mqtt_event_handler(void* handler_args, esp_event_base_t base,
                               int32_t event_id, void* event_data) {
  esp_mqtt_event_handle_t e = (esp_mqtt_event_handle_t)event_data;
  switch ((esp_mqtt_event_id_t)event_id) {
    case MQTT_EVENT_CONNECTED:
      // Subscribe QoS 0 (AT_MOST_ONCE) -- a live feed, no replay needed.
      esp_mqtt_client_subscribe(s_client, s_topic, 0);
      break;
    case MQTT_EVENT_SUBSCRIBED:
      s_state = MQTT_SRC_CONNECTED;
      s_last_err[0] = 0;
      log_i("subscribed %s", s_topic);
      break;
    case MQTT_EVENT_DATA:
      on_data(e);
      break;
    case MQTT_EVENT_DISCONNECTED:
      // esp-mqtt auto-reconnects; reflect the transient state unless we've
      // latched an auth failure (which stops retrying below).
      if (s_state != MQTT_SRC_AUTH_ERR) s_state = MQTT_SRC_CONNECTING;
      break;
    case MQTT_EVENT_ERROR: {
      esp_mqtt_error_codes_t* ec = e->error_handle;
      if (!ec) break;
      if (ec->error_type == MQTT_ERROR_TYPE_CONNECTION_REFUSED) {
        // The broker answered but rejected the CONNECT. Codes: 4=bad user/pass,
        // 5=not authorized -- credentials won't fix themselves, so stop.
        snprintf(s_last_err, sizeof(s_last_err), "refused (code %d)", (int)ec->connect_return_code);
        if (ec->connect_return_code == MQTT_CONNECTION_REFUSE_NOT_AUTHORIZED ||
            ec->connect_return_code == MQTT_CONNECTION_REFUSE_BAD_USERNAME) {
          // Latch the auth failure; the UI-task poll tears the client down (it is
          // illegal to stop the client from within its own event task).
          s_state = MQTT_SRC_AUTH_ERR;
        }
      } else {
        // TLS/WebSocket/socket failure before CONNACK -- keeps auto-retrying.
        snprintf(s_last_err, sizeof(s_last_err), "transport (errno %d)", ec->esp_transport_sock_errno);
        log_w("transport error errno=%d", ec->esp_transport_sock_errno);
      }
      break;
    }
    default:
      break;
  }
}

// ---- public API -------------------------------------------------------------
void mqtt_src_start(const char* url, const char* topic, const char* user, const char* pass) {
  if (!url || !topic || !url[0] || !topic[0]) { mqtt_src_stop(); return; }

  // No-op if already running against the exact same target.
  if (s_client && s_state != MQTT_SRC_AUTH_ERR &&
      !strcmp(s_url, url) && !strcmp(s_topic, topic) &&
      !strcmp(s_user, user ? user : "") && !strcmp(s_pass, pass ? pass : "")) {
    return;
  }
  mqtt_src_stop();

  strncpy(s_url,   url,          sizeof(s_url) - 1);   s_url[sizeof(s_url) - 1] = 0;
  strncpy(s_topic, topic,        sizeof(s_topic) - 1); s_topic[sizeof(s_topic) - 1] = 0;
  strncpy(s_user,  user ? user : "", sizeof(s_user) - 1); s_user[sizeof(s_user) - 1] = 0;
  strncpy(s_pass,  pass ? pass : "", sizeof(s_pass) - 1); s_pass[sizeof(s_pass) - 1] = 0;

  // One-time PSRAM allocations (kept for the client's lifetime): the reassembly
  // buffer and the decoded-packet queue storage, so neither eats internal DRAM.
  if (!s_asm) s_asm = (char*)ps_malloc(MQ_ASM_MAX);
  if (!s_queue) {
    if (!s_queue_store) s_queue_store = (uint8_t*)ps_malloc(MQ_QUEUE_LEN * sizeof(MqPkt));
    if (s_queue_store)
      s_queue = xQueueCreateStatic(MQ_QUEUE_LEN, sizeof(MqPkt), s_queue_store, &s_queue_ctrl);
    else
      s_queue = xQueueCreate(MQ_QUEUE_LEN, sizeof(MqPkt));   // fallback: internal RAM
  }

  esp_mqtt_client_config_t cfg = {};
  cfg.uri = s_url;                                  // wss:// -> WebSocket+TLS auto-selected
  cfg.cert_pem = ISRG_ROOT_X1;                      // verify the broker against Let's Encrypt's root
  // Halve esp-mqtt's internal RX/TX buffers (default 1 KB each): the feed's
  // messages are small, and larger ones are reassembled in the PSRAM s_asm
  // buffer, so this frees ~1 KB of internal DRAM.
  cfg.buffer_size = 512;
  if (s_user[0]) cfg.username = s_user;
  if (s_pass[0]) cfg.password = s_pass;

  s_client = esp_mqtt_client_init(&cfg);
  if (!s_client) { s_state = MQTT_SRC_OFF; log_w("client init failed"); return; }
  esp_mqtt_client_register_event(s_client, (esp_mqtt_event_id_t)ESP_EVENT_ANY_ID,
                                 mqtt_event_handler, nullptr);
  s_state = MQTT_SRC_CONNECTING;
  s_recv  = 0;
  s_last_err[0] = 0;
  esp_mqtt_client_start(s_client);
  log_i("connecting %s topic=%s auth=%d heap=%u", s_url, s_topic, s_user[0] ? 1 : 0,
        (unsigned)heap_caps_get_free_size(MALLOC_CAP_INTERNAL));
}

void mqtt_src_stop(void) {
  if (s_client) {
    esp_mqtt_client_stop(s_client);
    esp_mqtt_client_destroy(s_client);
    s_client = nullptr;
  }
  s_state = MQTT_SRC_OFF;
  s_url[0] = s_topic[0] = s_user[0] = s_pass[0] = 0;
  s_asm_len = 0; s_asm_skip = false;
  if (s_queue) xQueueReset(s_queue);
}

int mqtt_src_state(void) { return s_state; }

uint32_t mqtt_src_received(void) { return s_recv; }

void mqtt_src_status(char* out, int len) {
  switch (s_state) {
    case MQTT_SRC_CONNECTED:  snprintf(out, len, "connected  %u pkts", (unsigned)s_recv); break;
    case MQTT_SRC_CONNECTING:
      // Show the last handshake error (if any) so a stuck connect is diagnosable
      // on-device; otherwise just the in-progress state.
      if (s_last_err[0]) snprintf(out, len, "connecting - %s", s_last_err);
      else               snprintf(out, len, "connecting...");
      break;
    case MQTT_SRC_AUTH_ERR:   snprintf(out, len, "not authorized - check token"); break;
    default:                  snprintf(out, len, "off"); break;
  }
}

void mqtt_src_drain(void) {
  if (!s_queue) return;
  MqPkt pkt;
  // Bound the work per call so a burst can't stall the UI task.
  for (int i = 0; i < MQ_QUEUE_LEN && xQueueReceive(s_queue, &pkt, 0) == pdTRUE; i++) {
    ui_inject_net_packet(pkt.snr, pkt.rssi, pkt.raw, pkt.len, pkt.region);
    s_recv++;
  }
}
