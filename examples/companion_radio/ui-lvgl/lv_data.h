#pragma once

// C-callable view-model bridge between the (C) LVGL screens and the live data.
// The firmware implements these in ui-lvgl/UITask.cpp (pulling from MyMesh /
// rtc_clock / radio_driver); the desktop sim implements them in sim-lvgl/main.c
// with mock data, so the screens stay shared and data-source-agnostic.
#include <stdint.h>
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// ---- home status bar -------------------------------------------------------
const char* lvd_node_name(void);       // this node's name
const char* lvd_device_label(void);    // e.g. "TDeck+"
void        lvd_clock_hhmm(char* out, int len);   // "HH:MM" (or "--:--")
int         lvd_batt_pct(void);        // 0..100, -1 = unknown
int         lvd_signal_bars(void);     // 0..4
int         lvd_unread_count(void);     // total unread (Chat badge)

// ---- heard stations (Heard screen) -----------------------------------------
typedef struct {
  char name[24];
  char meta[44];   // pre-formatted "SNR x dB   RSSI y"
  char age[12];    // "12s ago"
  char dist[12];   // "4.2 km" or ""
  int  bars;       // 0..4 (signal-dot colour)
} lvd_heard_t;

int  lvd_heard_count(void);
bool lvd_heard_get(int i, lvd_heard_t* out);

// ---- contacts (Contacts screen) --------------------------------------------
typedef struct {
  char name[32];
  char subtitle[40];   // pre-formatted "Chat / direct", "Repeater / 2 hops", ...
  int  type;           // ADV_TYPE_*: 1=chat 2=repeater 3=room 4=sensor (0=other)
} lvd_contact_t;

int  lvd_contact_count(void);
bool lvd_contact_get(int i, lvd_contact_t* out);

// ---- settings / device config (Settings screens) ---------------------------
// Bound fields are keyed by (group title, field label). lvd_cfg_get fills the
// live value (val for VAL/INFO, *sel for ENUM/BOOL) and returns true; an unbound
// field returns false and keeps its prototype default. lvd_cfg_set applies an
// edit (validate + apply-live + persist); lvd_cfg_action fires an action field.
bool lvd_cfg_get(const char* group, const char* label, char* val, int val_len, int* sel);
void lvd_cfg_set(const char* group, const char* label, const char* val, int sel);
void lvd_cfg_action(const char* group, const char* label);

// ---- statistics (Stats screen) ---------------------------------------------
typedef struct {
  int      noise_floor;            // current noise floor, dBm (0 = unknown)
  int      noise_min, noise_max;   // over the recent on-screen window
  int      last_rssi;              // dBm
  int      last_snr_q;             // SNR * 10 (avoids float formatting on the C side)
  unsigned pkt_recv, pkt_sent, pkt_recv_err;
  unsigned batt_mv;
  unsigned uptime_secs;
} lvd_stats_t;
void lvd_stats_get(lvd_stats_t* out);              // sample now (advances noise history)
int  lvd_stats_noise_history(int* out, int max);   // oldest..newest; returns count

// ---- home hero widgets -----------------------------------------------------
int      lvd_noise_floor(void);   // current noise floor, dBm (0 = unknown)
unsigned lvd_pkt_recv(void);      // total packets received (drives the activity rate)

// ---- packet monitor (Terminal > Packets) -----------------------------------
typedef struct {
  char     type[6];   // payload type: "TXT","ADV","ACK","GRP","TRC",...
  char     meta[44];  // pre-formatted "<route>  len<n>  <rssi>dBm  <snr>  <age>"
  uint32_t color;     // pill colour for the type tag
} lvd_packet_t;

int      lvd_packet_count(void);
unsigned lvd_packet_total(void);                 // monotonic count, for refresh detection
bool     lvd_packet_get(int i, lvd_packet_t* out);   // i = 0 is the newest

// ---- discover (recently-heard nodes not yet saved as contacts) -------------
typedef struct {
  char name[32];
  char subtitle[40];   // "<type>  -  tap to add"
  int  type;           // ADV_TYPE_*
} lvd_disc_t;

int  lvd_disc_count(void);
bool lvd_disc_get(int i, lvd_disc_t* out);
void lvd_disc_add(int i);       // promote candidate i to a saved contact
void lvd_disc_announce(void);   // zero-hop self-advert to prompt neighbours to respond

// ---- chat: Public channel conversation (v1) --------------------------------
typedef struct {
  char sender[24];   // "" for channel msgs (sender is embedded in text)
  char text[124];
  int  outgoing;     // 1 = sent by us
} lvd_msg_t;

// The conversation screen shows one "active" conversation: the Public channel or
// a direct message with a contact. Open one before showing the conversation.
void        lvd_chat_open_public(void);
void        lvd_chat_open_dm(const char* contact_name);
const char* lvd_chat_title(void);              // "Public" or the DM peer name

int      lvd_chat_count(void);                 // messages in the active conversation
bool     lvd_chat_get(int i, lvd_msg_t* out);  // oldest..newest
unsigned lvd_chat_total(void);                 // monotonic, for refresh detection
const char* lvd_chat_last_preview(void);       // last Public msg text (for the list row)
void     lvd_chat_send(const char* text);      // send to the active conversation

#ifdef __cplusplus
}
#endif
