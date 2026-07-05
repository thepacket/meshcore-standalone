#pragma once

// C-callable view-model bridge between the (C) LVGL screens and the live data.
// The firmware implements these in ui-lvgl/UITask.cpp (pulling from MyMesh /
// rtc_clock / radio_driver), so the screens stay data-source-agnostic.
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
int         lvd_unread_count(void);     // total unread (Chat badge)

// ---- heard stations (Heard screen) -----------------------------------------
typedef struct {
  char name[24];
  char meta[44];   // pre-formatted "SNR x dB   RSSI y"
  char route[32];  // "<type> - direct" / "<type> - 3 hops"
  char age[12];    // "12s ago"
  char dist[16];   // "4.2 km NE" or ""
  int  bars;       // 0..4 (signal-dot colour)
  int  saved;      // 1 if this node is a saved contact
  int  type;       // ADV_TYPE_*: 1=chat 2=repeater 3=room 4=sensor (0=unknown)
} lvd_heard_t;
// Heard-list sort order: 0 = most recent (default), 1 = strongest signal
void lvd_heard_set_sort(int mode);
int  lvd_heard_sort(void);

int  lvd_heard_count(void);
bool lvd_heard_get(int i, lvd_heard_t* out);

// ---- contacts (Contacts screen) --------------------------------------------
typedef struct {
  char name[32];
  char subtitle[40];   // pre-formatted "Chat / direct", "Repeater / 2 hops", ...
  int  type;           // ADV_TYPE_*: 1=chat 2=repeater 3=room 4=sensor (0=other)
  int  fav;            // 1 if flagged as a favourite
} lvd_contact_t;

int  lvd_contact_count(void);
bool lvd_contact_get(int i, lvd_contact_t* out);
void        lvd_contact_set_filter(const char* s);  // name substring filter ("" = all)
const char* lvd_contact_filter(void);
void        lvd_contact_set_fav_only(int on);       // show only favourites when set
int         lvd_contact_fav_only(void);
void        lvd_contact_set_type(int t);            // node-type filter: 0 all, 1 chats, 2 repeaters, 3 rooms
int         lvd_contact_type(void);
int         lvd_contact_total(void);                // total contacts (ignores the filter)
// match a name against space-separated OR tokens ("sky hull" => sky OR hull)
bool        lvd_name_match(const char* hay, const char* needle);

// ---- settings / device config (Settings screens) ---------------------------
// Bound fields are keyed by (group title, field label). lvd_cfg_get fills the
// live value (val for VAL/INFO, *sel for ENUM/BOOL) and returns true; an unbound
// field returns false and keeps its prototype default. lvd_cfg_set applies an
// edit (validate + apply-live + persist); lvd_cfg_action fires an action field.
bool lvd_cfg_get(const char* group, const char* label, char* val, int val_len, int* sel);
void lvd_cfg_set(const char* group, const char* label, const char* val, int sel);
void lvd_cfg_action(const char* group, const char* label);

// on-screen (touch) keyboard preference; text screens skip it when disabled
bool lvd_osk_enabled(void);
void lvd_osk_set(bool on);

// ---- microSD file browser (read + write) -----------------------------------
typedef struct { char name[64]; int is_dir; unsigned long size; } lvd_sd_t;
int  lvd_sd_available(void);                            // 1 if a card is mounted
int  lvd_sd_list(const char* path, lvd_sd_t* out, int max);  // dir entries; -1 if no card
void lvd_sd_usage(char* out, int len);                 // "used / total" summary
int  lvd_sd_remove(const char* path);                  // delete a file/empty dir; 1 on success
int  lvd_sd_format(void);                              // reformat the card as FAT32 (destructive!); 1 on success

// ---- regions: named {radio preset + contacts + channels} snapshots on SD ----
// A region is a self-contained network profile stored under /meshcore/regions/
// <name>/ (config.txt + appdata.txt). Selecting one saves the active region,
// clears the live contacts/channels, and restores the target -- all live, no
// reboot. The active region is remembered in NVS. Needs a mounted SD card.
int  lvd_region_count(void);                           // saved regions; -1 if no card
int  lvd_region_get(int i, char* name, int nlen, int* active);  // i-th name; *active=1 if current
int  lvd_region_active(char* name, int nlen);          // active region name; 1 if any, 0 if none
int  lvd_region_create(const char* name);              // snapshot current -> new region + make active
                                                       // 1 ok, 0 no card/write fail, -1 exists, -2 bad name
int  lvd_region_select(int i);                         // switch to region i (saves current first); 1 ok
int  lvd_region_delete(int i);                         // delete region i; 1 ok, -1 active (refused), 0 fail

// ---- offline map (raw RGB565 tiles on SD at /maps/{z}/{x}/{y}.bin) ----------
#define LVD_MAP_TILE_PX 256   // tile edge, pixels (standard slippy tiles)
// our current position (GPS or the manual Settings lat/lon); 1 if known (non-zero)
int  lvd_map_here(double* lat, double* lon);
// "Show on map": set a one-shot centre target, then nav to "map" (consumed on open)
void lvd_map_focus(double lat, double lon);
int  lvd_map_take_focus(double* lat, double* lon);   // 1 + clears if a focus is pending
// map markers = saved contacts/repeaters that carry a GPS fix
typedef struct { double lat, lon; char name[32]; int type; } lvd_marker_t;
int  lvd_map_marker_count(void);
bool lvd_map_marker_get(int i, lvd_marker_t* out);
// read one raw RGB565 tile into buf (needs TILE_PX*TILE_PX*2 bytes); 1 ok, 0 missing/no card
int  lvd_map_tile(int z, int x, int y, unsigned char* buf, int maxbytes);
// lowest/highest zoom level present on the card (scans /maps); both -1 if none
void lvd_map_zoom_range(int* zmin, int* zmax);
// on-device tile fetch over Wi-Fi (cache-miss -> HTTPS GET -> RGB565 -> SD cache)
int      lvd_map_online(void);                  // 1 if Wi-Fi is connected (fetch possible)
int      lvd_map_fetch(int z, int x, int y);    // queue a missing tile for background fetch
unsigned lvd_map_fetch_gen(void);               // bumps when a fetched tile lands -> redraw
const char* lvd_map_url(void);                  // active tile URL template ({z}/{x}/{y})
void        lvd_map_set_url(const char* u);     // set a custom URL (selects the Custom provider)
// tile provider selector (each caches to its own dir; Custom uses the URL above)
int         lvd_map_provider(void);             // active provider index
int         lvd_map_provider_count(void);
const char* lvd_map_provider_name(int i);
void        lvd_map_set_provider(int i);

// ---- Wi-Fi (internet access, station mode) ----------------------------------
// Runtime STA connection for internet-backed features (NTP clock sync first).
// Credentials persist in NVS; independent of the companion transport (USB/BLE).
int         lvd_wifi_enabled(void);              // user toggle (persisted)
void        lvd_wifi_set_enabled(int on);        // starts / tears down the connection
const char* lvd_wifi_ssid(void);                 // "" if unset
void        lvd_wifi_set_ssid(const char* ssid);
const char* lvd_wifi_pass(void);
void        lvd_wifi_set_pass(const char* pass);
int         lvd_wifi_state(void);                // 0 off, 1 connecting, 2 connected
void        lvd_wifi_status(char* out, int len); // "192.168.1.7  -52 dBm" / "connecting..." / "off"
// network scan (async): start returns 1 if a scan is now running
int  lvd_wifi_scan_start(void);
int  lvd_wifi_scan_state(void);                  // 0 idle, 1 scanning, 2 done
int  lvd_wifi_scan_count(void);                  // results (strongest first), state 2 only
bool lvd_wifi_scan_get(int i, char* ssid, int ssid_len, int* rssi, int* secure);
// NTP clock sync (sets the mesh RTC while connected)
int  lvd_ntp_enabled(void);
void lvd_ntp_set(int on);
// ping test (async ICMP, 4 pings to 8.8.8.8): verifies real internet reachability
int  lvd_wifi_ping_start(void);                  // 0 if not connected
void lvd_wifi_ping_status(char* out, int len);   // "--" / "pinging..." / "4/4 replies  avg 23 ms"

// ---- statistics (Stats screen) ---------------------------------------------
typedef struct {
  int      noise_floor;            // current noise floor, dBm (0 = unknown)
  int      noise_min, noise_max;   // over the recent on-screen window
  int      last_rssi;              // dBm
  int      last_snr_q;             // SNR * 10 (avoids float formatting on the C side)
  unsigned pkt_recv, pkt_sent, pkt_recv_err;
  unsigned batt_mv;
  unsigned uptime_secs;
  unsigned free_ram_kb;            // free heap (KB)
  unsigned flash_used_kb, flash_total_kb;   // filesystem usage
  unsigned err_flags;              // ERR_EVENT_* bitmask (radio dispatcher)
  int      num_contacts, max_contacts;      // contact-slot usage
  int      num_channels;           // configured group channels
} lvd_stats_t;
void lvd_stats_get(lvd_stats_t* out);              // sample now (advances noise + battery history)
int  lvd_stats_noise_history(int* out, int max);   // oldest..newest; returns count
int  lvd_stats_batt_history(int* out, int max);    // battery mV, oldest..newest; returns count
const char* lvd_stats_err_str(void);               // radio error flags, human-readable ("None" if clear)
void lvd_stats_reset(void);                        // zero the packet counters + error flags

// ---- home hero widgets -----------------------------------------------------
int      lvd_rf_rssi(void);       // instantaneous RF level on the listening freq, dBm
unsigned lvd_pkt_recv(void);      // total packets received (drives the activity rate)
unsigned lvd_pkt_sent(void);      // total packets transmitted
unsigned lvd_pkt_recv_err(void);  // total receive errors
int      lvd_last_rssi(void);     // dBm of the last received packet
int      lvd_last_snr_q(void);    // SNR*4 of the last received packet
unsigned lvd_free_ram_kb(void);   // free heap (KB)
unsigned lvd_free_flash_kb(void); // free filesystem storage (KB)

// ---- packet monitor (Terminal > Packets) -----------------------------------
typedef struct {
  char     type[6];   // payload type: "TXT","ADV","ACK","GRP","TRC",...
  char     meta[44];  // pre-formatted "<route>  len<n>  <rssi>dBm  <snr>  <age>"
  uint32_t color;     // pill colour for the type tag
} lvd_packet_t;

int      lvd_packet_count(void);
unsigned lvd_packet_total(void);                 // monotonic count, for refresh detection
bool     lvd_packet_get(int i, lvd_packet_t* out);   // i = 0 is the newest

// packet detail (tap a row): snapshot row i, then read its decoded breakdown +
// raw hex. The breakdown is a list of label/value pairs (like the Android dialog).
typedef struct { char label[16]; char value[88]; } lvd_kv_t;
void        lvd_packet_select(int i);                  // snapshot packet i (0 = newest)
int         lvd_packet_detail(lvd_kv_t* out, int max); // fill rows, returns count
const char* lvd_packet_hex(void);                      // raw hex of the selected packet
void        lvd_packet_set_path_filter(const char* s); // show only packets whose path matches
const char* lvd_packet_path_filter(void);

// floods view: the ring grouped by payhash (flood rebroadcasts). Shows mesh
// redundancy -- how many times / how many hops a flooded packet arrived.
typedef struct {
  char     type[6];    // payload type tag
  uint32_t color;
  int      copies;     // # times this flood was heard in the ring
  char     meta[64];   // "x4  2-3 hops  best -78 dBm  9.0"
  char     path[96];   // strongest copy's path ("A > B > 7F", or "direct")
} lvd_flood_t;
int  lvd_flood_count(void);
bool lvd_flood_get(int i, lvd_flood_t* out);      // sorted most-rebroadcast first
int  lvd_flood_hophist(int* bins, int maxbins);   // bins[h] = # packets with h hops; returns bins used

// ---- discover (direct neighbours that answered our discovery request) ------
typedef struct {
  char name[32];
  char subtitle[40];   // "<type>  -  SNR x dB  -  RSSI y"
  char age[12];        // "8s ago"
  char dist[16];       // "4.2 km NE" or ""
  int  type;           // ADV_TYPE_*
  int  bars;           // 0..4 signal-dot bucket (from SNR)
  int  fresh;          // 1 if it answered the current scan
} lvd_disc_t;

int  lvd_disc_count(void);      // responders to the last discovery request (SNR-sorted)
bool lvd_disc_get(int i, lvd_disc_t* out);  // responders are auto-added as contacts
int  lvd_disc_request(void);    // send a zero-hop discovery request (paced; 0=sent, else secs left)
int  lvd_disc_next_secs(void);  // seconds until the next scan is allowed (0 = ready now)
const char* lvd_disc_summary(void);  // "5 direct neighbours - 2 repeaters, 3 companions"
void lvd_disc_clear(void);      // empty the discovered-nodes list
void lvd_disc_announce(void);       // zero-hop self-advert to prompt neighbours to respond
void lvd_disc_announce_flood(void); // flood-routed self-advert (reaches beyond direct range)

// ---- chat: Public channel conversation (v1) --------------------------------
typedef struct {
  char sender[24];   // "" for channel msgs (sender is embedded in text)
  char text[124];
  int  outgoing;     // 1 = sent by us
  char time[8];      // "HH:MM" (or "" if clock unset)
  int  status;       // outgoing only: 0 none, 1 sent, 2 pending, 3 delivered, 4 failed
  char route[16];    // incoming only: "direct" / "N hops" ("" for outgoing)
  int  can_resend;   // 1 if this is a failed outbound DM (offer Resend)
} lvd_msg_t;

// The conversation screen shows one "active" conversation: a channel (Public or a
// named group channel) or a direct message. Open one before showing the conversation.
void        lvd_chat_open_public(void);
void        lvd_chat_open_channel(int i);      // open channel by chat-list display index
void        lvd_chat_open_dm(const char* contact_name);
const char* lvd_chat_title(void);              // channel name or the DM peer name

int      lvd_chat_count(void);                 // messages in the active conversation
bool     lvd_chat_get(int i, lvd_msg_t* out);  // oldest..newest
unsigned lvd_chat_total(void);                 // monotonic, for refresh detection
int      lvd_chat_has_pending(void);           // 1 while an active-conv DM awaits its ack
void     lvd_chat_send(const char* text);      // send to the active conversation
void     lvd_chat_resend(int i);               // re-send message i's text (failed outbound DM)
void     lvd_chat_delete(int i);               // remove message i from this conversation
bool     lvd_chat_send_location(void);         // emergency position share (text); false if no position

// ---- trace route -----------------------------------------------------------
typedef struct {
  char left[36];     // "1.  GW-Hertford" (friendly name) or "to you"
  char snr[16];      // "+8.0 dB"
  int  quality;      // 0 = weak, 1 = ok, 2 = good (row colour)
  int  weakest;      // 1 if this is the weakest link on the path (bottleneck)
  int  snr_pct;      // 0..100, SNR mapped to a bar length (-20..+10 dB)
} lvd_hop_t;

// build a path (chain of repeaters) then trace through it
void        lvd_trace_path_clear(void);
void        lvd_trace_path_add(int i);          // add saved repeater/room i (from lvd_rep_get(0,i))
int         lvd_trace_path_len(void);
const char* lvd_trace_path_str(void);           // "A > B" chain being built
void        lvd_trace_go(void);                 // send the trace along the built path (also re-runs = Repeat)

int         lvd_trace_state(void);    // 0 idle, 1 tracing, 2 done, 3 failed, 4 timed out
const char* lvd_trace_target(void);
void        lvd_trace_poll(void);     // call ~1/s while tracing; flips to timed out
int         lvd_trace_count(void);    // hop rows (intermediate hops + final "to you")
bool        lvd_trace_get(int i, lvd_hop_t* out);
unsigned    lvd_trace_seq(void);      // monotonic, for refresh detection
const char* lvd_trace_summary(void);  // result: "3 hops - RTT 1.4 s - weakest -3.0 dB"
unsigned    lvd_trace_elapsed_ms(void); // ms since the in-flight trace was sent (live timer)
// traceable contacts (those we hold a routed multi-hop path to)
int         lvd_trace_contact_count(void);
bool        lvd_trace_contact_get(int i, char* name, int name_len, int* hops);
int         lvd_trace_contact_go(int i);   // trace contact i over its learned path; 0 sent

// ---- repeater / room admin -------------------------------------------------
typedef struct { char name[32]; char type[6]; int fav; } lvd_replist_t;
int  lvd_rep_count(int scan);                       // scan: 0=saved repeaters/rooms, 1=heard
bool lvd_rep_get(int scan, int i, lvd_replist_t* out);
void lvd_rep_open(int scan, int i);                 // set the active repeater from the list
int  lvd_rep_open_contact(const char* name);        // set active repeater from a contact; 0 ok, 1 not found, 2 not a repeater

const char* lvd_rep_name(void);
int  lvd_rep_login_state(void);                     // 0 none, 1 pending, 2 in, 3 failed
void lvd_rep_login(const char* password);
int  lvd_rep_login_remembered(void);                // auto-login with a saved password; 1 started, 0 none
void lvd_rep_request_status(void);

typedef struct {
  int  have;
  char batt[16], uptime[16], recv[14], sent[14], airtime[14], snr[12];
} lvd_repstat_t;
void lvd_rep_status_get(lvd_repstat_t* out);

void        lvd_rep_send_cmd(const char* cmd);      // remote CLI command
int         lvd_rep_cli_count(void);
const char* lvd_rep_cli_line(int i);                // oldest..newest

unsigned    lvd_rep_seq(void);                      // refresh detection

// remote neighbour list (REQ_TYPE_GET_NEIGHBOURS on the active repeater; the
// strongest ~11 direct neighbours with heard-age and SNR; requires login)
typedef struct { char name[28]; char age[12]; char snr[12]; int known; } lvd_neigh_t;
int  lvd_rep_neigh_request(void);   // ask the active repeater; 0 = sent
int  lvd_rep_neigh_state(void);     // 0 idle, 1 waiting, 2 have, 3 timed out
int  lvd_rep_neigh_total(void);     // repeater-side neighbour count (-1 unknown)
int  lvd_rep_neigh_count(void);     // entries received
bool lvd_rep_neigh_get(int i, lvd_neigh_t* out);

// ---- peer / contact details ------------------------------------------------
typedef struct {
  char type[16];        // "Chat contact", "Repeater", ...
  char rssi[12], snr[12], dist[12], hops[12], lastheard[14], path[12];
  char lat[16], lon[16], pubkey[72];
  int  fav;             // 1 if flagged as a favourite
} lvd_peer_t;
bool lvd_peer_get(const char* name, lvd_peer_t* out);   // false if not a saved contact
// contact ops (by name)
bool        lvd_peer_share(const char* name);       // re-advertise the contact (zero-hop)
bool        lvd_peer_reset_path(const char* name);  // forget the learned return path
bool        lvd_peer_set_fav(const char* name, int on);  // toggle favourite (persists)
bool        lvd_peer_remove(const char* name);      // delete the contact
const char* lvd_peer_export_hex(const char* name);  // advert card as hex (for QR + display)
bool        lvd_peer_add(const char* name);         // save a heard-only node as a contact
// remote telemetry (battery etc.) from this contact; reply is async
bool        lvd_peer_telem_request(const char* name);
int         lvd_peer_telem_state(const char* name); // 0 idle, 1 waiting, 2 have, 3 timed out
int         lvd_peer_telem_get(lvd_kv_t* out, int max);   // decoded rows (state 2 only)
// trace this contact's learned path (Trace screen shows the result)
int         lvd_peer_trace(const char* name);       // 0 sent, 1 unknown contact, 2 no routed path

// ---- channels (channel management) -----------------------------------------
typedef struct {
  char name[32];
  char info[28];     // "Public - 128-bit key" / "256-bit key"
  int  is_public;    // 1 = the Public channel (cannot be removed)
} lvd_chan_t;
int         lvd_chan_count(void);
bool        lvd_chan_get(int i, lvd_chan_t* out);
bool        lvd_chan_add(const char* name, const char* psk_b64);   // join/create; false on bad PSK
bool        lvd_chan_remove(int i);                                // delete (not the Public channel)
const char* lvd_chan_psk(int i);                                   // channel PSK as base64 (share/QR)
const char* lvd_chan_new_psk(void);                                // fresh random 128-bit key (base64)
const char* lvd_chan_hashtag_psk(const char* name);                // key derived from a '#name' (sha256, base64)
void        lvd_factory_reset(void);                               // formats FS + reboots (confirm first!)
// chat list: the channels to show as conversations (Public + named group channels)
int         lvd_chat_chan_count(void);
bool        lvd_chat_chan_get(int i, lvd_chan_t* out);             // name + is_public
const char* lvd_chat_chan_preview(int i);                          // last message in channel i
int         lvd_chat_chan_unread(int i);                           // unread count for channel row i
const char* lvd_chat_chan_time(int i);                             // "HH:MM" of last message (or "")

// chat list: DM threads (distinct peers we have message history with).
// is_room marks room-server threads (posts attributed to their original
// authors; opening the thread logs in + syncs unseen posts).
typedef struct { char name[32]; char preview[100]; char time[8]; int unread; int is_room; } lvd_dm_t;
int         lvd_dm_count(void);
bool        lvd_dm_get(int i, lvd_dm_t* out);                      // name + last-message preview + time + unread
void        lvd_dm_open(int i);                                    // make DM i the active conversation

// ---- signal coverage (per repeater/room) -----------------------------------
typedef struct {
  char name[32];
  int  rssi;        // dBm (drives the bar)
  char info[48];    // "-78 dBm   SNR 9.0 dB   +24 dB margin"
  char sub[40];     // "direct - 4.2 km NE" (route + distance/bearing)
  char age[12];     // "12s" / "2m"
  int  heard;       // 1 if recently heard
  int  trend;       // RSSI trend since last refresh: -1 down, 0 flat, 1 up
} lvd_sig_t;
int  lvd_signal_count(void);
bool lvd_signal_get(int i, lvd_sig_t* out);
// sort order: 0 = signal (default), 1 = distance, 2 = name
void lvd_signal_set_sort(int mode);
int  lvd_signal_sort(void);

#ifdef __cplusplus
}
#endif
