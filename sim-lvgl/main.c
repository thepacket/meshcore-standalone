// LVGL prototype harness.
//   ./lvglsim                      -> live interactive window (mouse = touch)
//   ./lvglsim <screen> <out.ppm>   -> headless: render one screen to a PPM
#include "lvgl.h"
#include "lv_ui.h"
#include "lv_data.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

// ---- mock implementations of the live-data bridge (firmware uses MyMesh) ----
const char* lvd_node_name(void) { return "Andy"; }
const char* lvd_device_label(void) { return "TDeck+"; }
void lvd_clock_hhmm(char* out, int len) { snprintf(out, len, "15:34"); }
int lvd_batt_pct(void) { return 78; }
int lvd_signal_bars(void) { return 4; }
int lvd_unread_count(void) { return 2; }

static const lvd_heard_t MOCK_HEARD[] = {
  {"Repeater-7",  "SNR 9.0 dB   RSSI -78",  "12s ago", "4.2 km", 4},
  {"Andy-Mobile", "SNR 6.0 dB   RSSI -92",  "45s ago", "0.8 km", 2},
  {"GW-Hertford", "SNR 2.0 dB   RSSI -108", "10m ago", "",       1},
};
int lvd_heard_count(void) { return (int)(sizeof(MOCK_HEARD)/sizeof(MOCK_HEARD[0])); }
bool lvd_heard_get(int i, lvd_heard_t* out) {
  if (i < 0 || i >= lvd_heard_count()) return false;
  *out = MOCK_HEARD[i];
  return true;
}

static const lvd_contact_t MOCK_CONTACTS[] = {
  {"Andy-Mobile",   "Chat / direct",   1},
  {"Field-Team",    "Room / 2 hops",   3},
  {"GW-Hertford",   "Repeater / flood",2},
  {"Hilltop-Relay", "Repeater / 2 hops",2},
  {"Sensor-Barn",   "Sensor / direct", 4},
};
int lvd_contact_count(void) { return (int)(sizeof(MOCK_CONTACTS)/sizeof(MOCK_CONTACTS[0])); }
bool lvd_contact_get(int i, lvd_contact_t* out) {
  if (i < 0 || i >= lvd_contact_count()) return false;
  *out = MOCK_CONTACTS[i];
  return true;
}
static char g_cfilter[24] = "";
void lvd_contact_set_filter(const char* s) { g_cfilter[0] = 0; if (s) { int i = 0; for (; s[i] && i < 23; i++) g_cfilter[i] = s[i]; g_cfilter[i] = 0; } }
const char* lvd_contact_filter(void) { return g_cfilter; }
int lvd_contact_total(void) { return lvd_contact_count(); }
bool lvd_name_match(const char* hay, const char* needle) {
  if (!needle || !needle[0]) return true;
  const char* p = needle; bool saw = false;
  while (*p) {
    while (*p == ' ') p++;
    if (!*p) break;
    const char* s = p; while (*p && *p != ' ') p++;
    int len = (int)(p - s); if (len > 31) len = 31;
    char tok[32]; for (int i = 0; i < len; i++) { char a = s[i]; tok[i] = (a >= 'A' && a <= 'Z') ? a + 32 : a; } tok[len] = 0;
    saw = true;
    for (const char* h = hay; *h; h++) {
      const char* hh = h; const char* nn = tok;
      while (*hh && *nn) { char a = *hh, b = *nn; if (a >= 'A' && a <= 'Z') a += 32; if (a != b) break; hh++; nn++; }
      if (!*nn) return true;
    }
  }
  return !saw;
}

// settings bridge: sim keeps prototype defaults (get returns unbound)
bool lvd_cfg_get(const char* group, const char* label, char* val, int len, int* sel) {
  (void)group; (void)label; (void)val; (void)len; (void)sel; return false;
}
void lvd_cfg_set(const char* group, const char* label, const char* val, int sel) {
  (void)group; (void)label; (void)val; (void)sel;
}
void lvd_cfg_action(const char* group, const char* label) { (void)group; (void)label; }
static bool s_osk = true;
bool lvd_osk_enabled(void) { return s_osk; }
void lvd_osk_set(bool on) { s_osk = on; }

void lvd_stats_get(lvd_stats_t* out) {
  out->noise_floor = -104; out->noise_min = -118; out->noise_max = -92;
  out->last_rssi = -78; out->last_snr_q = 90;
  out->pkt_recv = 1942; out->pkt_sent = 238; out->pkt_recv_err = 7;
  out->batt_mv = 4050; out->uptime_secs = 3 * 3600 + 12 * 60;
}
int lvd_stats_noise_history(int* out, int max) {
  int n = max < 40 ? max : 40;
  for (int i = 0; i < n; i++) out[i] = -104 + (i * 11 % 6) - 3;
  return n;
}
int lvd_noise_floor(void) { return -104; }
unsigned lvd_pkt_recv(void) { static unsigned r = 1900; return r += 2; }
unsigned lvd_pkt_sent(void) { static unsigned s = 230; return s += 1; }
unsigned lvd_pkt_recv_err(void) { return 7; }
int lvd_last_rssi(void) { return -78; }
int lvd_last_snr_q(void) { return 36; }   /* 9.0 dB */

static const lvd_packet_t MOCK_PKTS[] = {
  {"TXT", "FLD  len41  -78dBm  9.0  3s",   UI_BLUE},
  {"ADV", "FLD  len27  -99dBm  4.7  17s",  UI_PURPLE},
  {"GRP", "TFL  len33  -78dBm  9.2  20s",  UI_CYAN},
  {"ACK", "DIR  len8   -105dBm  2.5  1m",  UI_GREEN},
  {"TRC", "FLD  len42  -70dBm  11.0  2m",  UI_AMBER},
};
int lvd_packet_count(void) { return (int)(sizeof(MOCK_PKTS)/sizeof(MOCK_PKTS[0])); }
unsigned lvd_packet_total(void) { return (unsigned)lvd_packet_count(); }
bool lvd_packet_get(int i, lvd_packet_t* out) {
  if (i < 0 || i >= lvd_packet_count()) return false;
  *out = MOCK_PKTS[i];
  return true;
}
static int g_psel = 0;
void lvd_packet_select(int i) { g_psel = i; }
int lvd_packet_detail(lvd_kv_t* out, int max) {
  static const lvd_kv_t D[] = {
    {"Type", "Advert"}, {"Route", "Flood (v1)"}, {"Path", "direct (0 hops)"},
    {"Advertiser", "GW-Hertford (a37f12..)"}, {"SNR / RSSI", "9.0 dB / -78 dBm"},
    {"Length", "41 B (payload 38 B)"}, {"Header", "0x11"}, {"Received", "3s ago"},
  };
  int n = (int)(sizeof(D)/sizeof(D[0]));
  if (n > max) n = max;
  for (int i = 0; i < n; i++) out[i] = D[i];
  return n;
}
const char* lvd_packet_hex(void) {
  return "11 00 04 A3 7F 12 C4 9B 0E 5D 61 2F 8A 44 D3 B7 E1 90 CA";
}
static char g_pktf[24] = "";
void lvd_packet_set_path_filter(const char* s) { g_pktf[0] = 0; if (s) { int i = 0; for (; s[i] && i < 23; i++) g_pktf[i] = s[i]; g_pktf[i] = 0; } }
const char* lvd_packet_path_filter(void) { return g_pktf; }

static const lvd_disc_t MOCK_DISC[] = {
  {"New-Repeater", "Repeater  -  tap to add", 2},
  {"Field-Sensor", "Sensor  -  tap to add",   4},
};
int lvd_disc_count(void) { return (int)(sizeof(MOCK_DISC)/sizeof(MOCK_DISC[0])); }
bool lvd_disc_get(int i, lvd_disc_t* out) {
  if (i < 0 || i >= lvd_disc_count()) return false;
  *out = MOCK_DISC[i];
  return true;
}
int lvd_disc_request(void) { return 0; }
void lvd_disc_clear(void) {}
void lvd_disc_announce(void) {}
void lvd_disc_announce_flood(void) {}

static const lvd_msg_t MOCK_MSGS[] = {
  {"Alice", "Anyone around the north side?", 0},
  {"Bob",   "Yep, copy you 5 by 9",          0},
  {"",      "On the hill now, strong signal", 1},
};
void lvd_chat_open_public(void) {}
void lvd_chat_open_dm(const char* contact_name) { (void)contact_name; }
const char* lvd_chat_title(void) { return "Public"; }
int lvd_chat_count(void) { return (int)(sizeof(MOCK_MSGS)/sizeof(MOCK_MSGS[0])); }
bool lvd_chat_get(int i, lvd_msg_t* out) {
  if (i < 0 || i >= lvd_chat_count()) return false;
  *out = MOCK_MSGS[i];
  return true;
}
unsigned lvd_chat_total(void) { return (unsigned)lvd_chat_count(); }
const char* lvd_chat_last_preview(void) { return "On the hill now, strong signal"; }
void lvd_chat_send(const char* text) { (void)text; }

static const lvd_hop_t MOCK_HOPS[] = {
  {"1.  id A3", "+8.0 dB", 2}, {"2.  id 7F", "+5.0 dB", 2},
  {"3.  id 12", "+3.0 dB", 1}, {"to you",     "+7.0 dB", 2},
};
void lvd_trace_path_clear(void) {}
void lvd_trace_path_add(int i) { (void)i; }
void lvd_trace_path_add_name(const char* name) { (void)name; }
int lvd_trace_path_len(void) { return 2; }
const char* lvd_trace_path_str(void) { return "GW-Hertford > Hilltop-Relay"; }
void lvd_trace_go(void) {}
void lvd_trace_poll(void) {}
int lvd_trace_state(void) { return 2; }
const char* lvd_trace_target(void) { return "Repeater-7"; }
int lvd_trace_count(void) { return (int)(sizeof(MOCK_HOPS)/sizeof(MOCK_HOPS[0])); }
bool lvd_trace_get(int i, lvd_hop_t* out) {
  if (i < 0 || i >= lvd_trace_count()) return false;
  *out = MOCK_HOPS[i];
  return true;
}
unsigned lvd_trace_seq(void) { return 1; }

static const lvd_replist_t MOCK_REPS[] = {
  {"GW-Hertford", "RPT", 1}, {"Hilltop-Relay", "RPT", 0}, {"Town Square", "ROOM", 0},
};
int lvd_rep_count(int scan) { return scan ? 0 : (int)(sizeof(MOCK_REPS)/sizeof(MOCK_REPS[0])); }
bool lvd_rep_get(int scan, int i, lvd_replist_t* out) {
  if (scan || i < 0 || i >= lvd_rep_count(0)) return false;
  *out = MOCK_REPS[i]; return true;
}
void lvd_rep_open(int scan, int i) { (void)scan; (void)i; }
const char* lvd_rep_name(void) { return "GW-Hertford"; }
int lvd_rep_login_state(void) { return 2; }
void lvd_rep_login(const char* password) { (void)password; }
void lvd_rep_request_status(void) {}
void lvd_rep_status_get(lvd_repstat_t* out) {
  out->have = 1;
  snprintf(out->batt, sizeof(out->batt), "4.05V");
  snprintf(out->uptime, sizeof(out->uptime), "4d 7h");
  snprintf(out->recv, sizeof(out->recv), "150000");
  snprintf(out->sent, sizeof(out->sent), "42000");
  snprintf(out->airtime, sizeof(out->airtime), "70m");
  snprintf(out->snr, sizeof(out->snr), "7.0 dB");
}
void lvd_rep_send_cmd(const char* cmd) { (void)cmd; }
int lvd_rep_cli_count(void) { return 0; }
const char* lvd_rep_cli_line(int i) { (void)i; return ""; }
unsigned lvd_rep_seq(void) { return 1; }

bool lvd_peer_get(const char* name, lvd_peer_t* out) {
  (void)name;
  snprintf(out->type, sizeof(out->type), "Chat contact");
  snprintf(out->rssi, sizeof(out->rssi), "-78 dBm");
  snprintf(out->snr, sizeof(out->snr), "9.0 dB");
  snprintf(out->dist, sizeof(out->dist), "--");
  snprintf(out->hops, sizeof(out->hops), "2");
  snprintf(out->lastheard, sizeof(out->lastheard), "12s");
  snprintf(out->path, sizeof(out->path), "direct");
  snprintf(out->lat, sizeof(out->lat), "51.7960");
  snprintf(out->lon, sizeof(out->lon), "-0.0810");
  snprintf(out->pubkey, sizeof(out->pubkey), "a37f12c49b0e5d612f8a44d3b7e190ca");
  return true;
}
bool lvd_peer_share(const char* name)      { (void)name; return true; }
bool lvd_peer_reset_path(const char* name) { (void)name; return true; }
bool lvd_peer_remove(const char* name)     { (void)name; return true; }
const char* lvd_peer_export_hex(const char* name) {
  (void)name;
  return "01a37f12c49b0e5d612f8a44d3b7e190ca0742696c6c79426f74";
}

static const lvd_sig_t MOCK_SIG[] = {
  {"GW-Hertford",  -72, "-72 dBm   SNR 9.0 dB",  "12s", 1},
  {"Hilltop-Relay",-96, "-96 dBm   SNR 2.0 dB",  "2m",  1},
  {"Town Square",  -110,"-110 dBm   SNR -8.0 dB","30m", 1},
};
int lvd_signal_count(void) { return (int)(sizeof(MOCK_SIG)/sizeof(MOCK_SIG[0])); }
bool lvd_signal_get(int i, lvd_sig_t* out) {
  if (i < 0 || i >= lvd_signal_count()) return false;
  *out = MOCK_SIG[i];
  return true;
}

#define W 320
#define H 240

void lv_home_create(lv_obj_t* scr);
void lv_chat_list_create(lv_obj_t* scr);
void lv_chat_conv_create(lv_obj_t* scr);
void lv_settings_create(lv_obj_t* scr);
void lv_settings_group_create(lv_obj_t* scr, int idx);
void lv_settings_edit_create(lv_obj_t* scr);
void lv_noise_create(lv_obj_t* scr);
void lv_signal_create(lv_obj_t* scr);
void lv_heard_create(lv_obj_t* scr);
void lv_repeaters_create(lv_obj_t* scr);
void lv_repeaters_set_tab(int t);
void lv_repeater_detail_create(lv_obj_t* scr);
void lv_peer_create(lv_obj_t* scr);
void lv_trace_create(lv_obj_t* scr);
void lv_terminal_create(lv_obj_t* scr);
void lv_terminal_set_tab(int t);
void lv_stats_create(lv_obj_t* scr);
void lv_discover_create(lv_obj_t* scr);
void lv_contacts_create(lv_obj_t* scr);

static uint32_t millis_cb(void) {
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

// ---- a placeholder for screens not yet ported to LVGL --------------------
static void placeholder(lv_obj_t* scr, const char* name) {
  lv_ui_screen_bg(scr);
  lv_ui_topbar(scr, name, UI_INDIGO, NULL);
  lv_obj_t* c = lv_ui_card(scr, 40, 90, 240, 70);
  lv_obj_t* l = lv_label_create(c);
  lv_label_set_text(l, "Coming soon");
  lv_obj_set_style_text_font(l, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(UI_MUTED), 0);
  lv_obj_center(l);
}

static void build(const char* name) {
  lv_obj_t* s = lv_screen_active();
  lv_obj_clean(s);
  if (!strcmp(name, "home")) lv_home_create(s);
  else if (!strcmp(name, "chat")) lv_chat_list_create(s);
  else if (!strcmp(name, "conv")) lv_chat_conv_create(s);
  else if (!strcmp(name, "settings")) lv_settings_create(s);
  else if (!strcmp(name, "edit")) lv_settings_edit_create(s);
  else if (name[0] == 's' && name[1] == 'g') lv_settings_group_create(s, atoi(name + 2));
  else if (!strcmp(name, "noise")) lv_noise_create(s);
  else if (!strcmp(name, "signal")) lv_signal_create(s);
  else if (!strcmp(name, "heard")) lv_heard_create(s);
  else if (!strcmp(name, "repeaters")) { lv_repeaters_set_tab(0); lv_repeaters_create(s); }
  else if (!strcmp(name, "scan")) { lv_repeaters_set_tab(1); lv_repeaters_create(s); }
  else if (!strcmp(name, "repeater_detail")) lv_repeater_detail_create(s);
  else if (!strcmp(name, "peer")) lv_peer_create(s);
  else if (!strcmp(name, "trace")) lv_trace_create(s);
  else if (!strcmp(name, "terminal")) { lv_terminal_set_tab(0); lv_terminal_create(s); }
  else if (!strcmp(name, "packets")) { lv_terminal_set_tab(1); lv_terminal_create(s); }
  else if (!strcmp(name, "stats")) lv_stats_create(s);
  else if (!strcmp(name, "discover")) lv_discover_create(s);
  else if (!strcmp(name, "contacts")) lv_contacts_create(s);
  else placeholder(s, name);
}

// ---- navigation stack ----------------------------------------------------
static char nav_stack[16][16];
static int nav_sp = 0;

static void nav(const char* dest) {
  if (!strcmp(dest, "back")) {
    if (nav_sp > 1) nav_sp--;
  } else {
    if (nav_sp < 16) { strncpy(nav_stack[nav_sp], dest, 15); nav_stack[nav_sp][15] = 0; nav_sp++; }
  }
  build(nav_stack[nav_sp - 1]);
}

// ===========================================================================
// headless screenshot mode
// ===========================================================================
static uint8_t fb[W * H * 3];
static void flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
  for (int y = area->y1; y <= area->y2; y++)
    for (int x = area->x1; x <= area->x2; x++) {
      const uint8_t* p = px_map + (((y - area->y1) * (area->x2 - area->x1 + 1)) + (x - area->x1)) * 4;
      uint8_t* o = fb + (y * W + x) * 3;
      o[0] = p[2]; o[1] = p[1]; o[2] = p[0];
    }
  lv_display_flush_ready(disp);
}

static int run_shot(const char* screen, const char* out) {
  lv_display_t* disp = lv_display_create(W, H);
  static uint8_t draw_buf[W * H * 4];
  lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, flush_cb);
  build(screen);
  for (int i = 0; i < 3; i++) lv_timer_handler();
  lv_refr_now(disp);
  FILE* f = fopen(out, "wb");
  if (!f) { fprintf(stderr, "cannot open %s\n", out); return 1; }
  fprintf(f, "P6\n%d %d\n255\n", W, H);
  fwrite(fb, 1, sizeof(fb), f);
  fclose(f);
  printf("wrote %s\n", out);
  return 0;
}

// ===========================================================================
// live interactive mode (LVGL SDL backend)
// ===========================================================================
static int run_live(void) {
  lv_sdl_window_create(W, H);
  lv_sdl_mouse_create();
  nav_sp = 0; nav("home");
  printf("live: click tiles to navigate, back chevron to return. Close window to quit.\n");
  while (lv_display_get_default()) {
    uint32_t t = lv_timer_handler();
    lv_delay_ms(t > 5 ? 5 : t);
  }
  return 0;
}

int main(int argc, char** argv) {
  lv_init();
  lv_tick_set_cb(millis_cb);
  lv_nav_cb = nav;
  if (argc >= 3) return run_shot(argv[1], argv[2]);   // headless
  return run_live();                                  // interactive
}
