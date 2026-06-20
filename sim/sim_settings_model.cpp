// Host mock of the settings model: same SETTINGS_ROOT structure as the firmware,
// but forwarders read/write an in-memory state instead of MyMesh/NodePrefs. This
// lets the real screens be exercised on the desktop without the mesh stack.
#include "SettingsModel.h"
#include "UITask.h"
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// ---- fake device state (mirrors the NodePrefs fields the UI touches) ----
struct SimState {
  char  node_name[32] = "MyNode";
  int   advert_loc = 0;          // 0 hidden / 1 shared
  float freq = 915.0f;           // MHz
  float bw = 250.0f;             // kHz
  int   sf = 10, cr = 5;
  int   tx = 20;
  int   client_repeat = 0;
  int   manual_add = 0;
  int   autoadd = 0x1E;          // chat|repeater|room|sensor
  int   max_hops = 3;
  int   multi_acks = 1;
  int   path_hash = 0;
  float airtime = 1.0f, rx_delay = 0.5f;
  float lat = 0.0f, lon = 0.0f;
  int   gps_enabled = 0, gps_interval = 0;
  int   telem_base = 0, telem_loc = 0, telem_env = 0;
  unsigned long ble_pin = 123456;
  char  scope_name[31] = "";
};
static SimState S;

// ---- Public info ----
static const char* get_node_name() { return S.node_name; }
static bool set_node_name(const char* s) { strncpy(S.node_name, s, sizeof(S.node_name) - 1); S.node_name[sizeof(S.node_name) - 1] = 0; return true; }
static const EnumOpt OPT_LOC[] = {{"Hidden", 0}, {"Shared", 1}};
static int32_t get_loc() { return S.advert_loc; }
static bool set_loc(int32_t v) { S.advert_loc = v; return true; }
static void act_advert(UITask* t) { if (t) t->showAlert("Advert sent", 1200); }
static void act_advert_flood(UITask* t) { if (t) t->showAlert("Flood advert sent", 1200); }
static const Setting GRP_PUBLIC[] = {
  SET_STRING("Node name", get_node_name, set_node_name),
  SET_ENUM("Location", get_loc, set_loc, OPT_LOC, 2),
  SET_ACTION("Send advert", act_advert),
  SET_ACTION("Send advert (flood)", act_advert_flood),
};

// ---- Radio ----
static float get_freq() { return S.freq; }
static bool set_freq(float v) { if (v < 150 || v > 960) return false; S.freq = v; return true; }
static const EnumOpt OPT_BW[] = {
  {"7.8", 7800}, {"10.4", 10400}, {"15.6", 15600}, {"20.8", 20800}, {"31.25", 31250},
  {"41.7", 41700}, {"62.5", 62500}, {"125", 125000}, {"250", 250000}, {"500", 500000},
};
static int32_t get_bw() { return (int32_t)lroundf(S.bw * 1000.0f); }
static bool set_bw(int32_t hz) { S.bw = hz / 1000.0f; return true; }
static int32_t get_sf() { return S.sf; }
static bool set_sf(int32_t v) { S.sf = v; return true; }
static int32_t get_cr() { return S.cr; }
static bool set_cr(int32_t v) { S.cr = v; return true; }
static int32_t get_tx() { return S.tx; }
static bool set_tx(int32_t v) { S.tx = v; return true; }
static int32_t get_rep() { return S.client_repeat; }
static bool set_rep(int32_t v) { S.client_repeat = v ? 1 : 0; return true; }
static const Setting GRP_RADIO[] = {
  SET_FLOAT("Frequency", get_freq, set_freq, 150.0f, 960.0f, 0.125f, "MHz"),
  SET_ENUM("Bandwidth", get_bw, set_bw, OPT_BW, 10),
  SET_INT("Spread factor", get_sf, set_sf, 5, 12, 1, ""),
  SET_INT("Coding rate", get_cr, set_cr, 5, 8, 1, ""),
  SET_INT("TX power", get_tx, set_tx, -9, 22, 1, "dBm"),
  SET_BOOL("Client repeat", get_rep, set_rep),
};

// ---- Contacts ----
static int32_t aa(int bit) { return (S.autoadd & bit) ? 1 : 0; }
static void aset(int bit, int32_t v) { if (v) S.autoadd |= bit; else S.autoadd &= ~bit; }
static int32_t g_chat() { return aa(0x02); }      static bool s_chat(int32_t v) { aset(0x02, v); return true; }
static int32_t g_rep() { return aa(0x04); }       static bool s_rep(int32_t v) { aset(0x04, v); return true; }
static int32_t g_room() { return aa(0x08); }      static bool s_room(int32_t v) { aset(0x08, v); return true; }
static int32_t g_sens() { return aa(0x10); }      static bool s_sens(int32_t v) { aset(0x10, v); return true; }
static int32_t g_over() { return aa(0x01); }      static bool s_over(int32_t v) { aset(0x01, v); return true; }
static int32_t get_manual() { return S.manual_add; } static bool set_manual(int32_t v) { S.manual_add = v ? 1 : 0; return true; }
static int32_t get_hops() { return S.max_hops; }  static bool set_hops(int32_t v) { S.max_hops = v; return true; }
static const Setting GRP_CONTACTS[] = {
  SET_BOOL("Manual add", get_manual, set_manual),
  SET_BOOL("Auto-add users", g_chat, s_chat),
  SET_BOOL("Auto-add repeaters", g_rep, s_rep),
  SET_BOOL("Auto-add rooms", g_room, s_room),
  SET_BOOL("Auto-add sensors", g_sens, s_sens),
  SET_BOOL("Overwrite oldest", g_over, s_over),
  SET_INT("Max hops", get_hops, set_hops, 0, 64, 1, ""),
};

// ---- Message ----
static int32_t get_ma() { return S.multi_acks; } static bool set_ma(int32_t v) { S.multi_acks = v; return true; }
static const EnumOpt OPT_PHM[] = {{"Off", 0}, {"Mode 1", 1}, {"Mode 2", 2}};
static int32_t get_phm() { return S.path_hash; } static bool set_phm(int32_t v) { if (v >= 3) return false; S.path_hash = v; return true; }
static const Setting GRP_MESSAGE[] = {
  SET_INT("Multi-acks", get_ma, set_ma, 0, 4, 1, ""),
  SET_ENUM("Path hash mode", get_phm, set_phm, OPT_PHM, 3),
};

// ---- Position ----
static float get_lat() { return S.lat; }  static bool set_lat(float v) { if (v < -90 || v > 90) return false; S.lat = v; return true; }
static float get_lon() { return S.lon; }  static bool set_lon(float v) { if (v < -180 || v > 180) return false; S.lon = v; return true; }
static int32_t get_gps() { return S.gps_enabled; } static bool set_gps(int32_t v) { S.gps_enabled = v ? 1 : 0; return true; }
static int32_t get_gint() { return S.gps_interval; } static bool set_gint(int32_t v) { S.gps_interval = v; return true; }
static const Setting GRP_POSITION[] = {
  SET_FLOAT("Latitude", get_lat, set_lat, -90.0f, 90.0f, 0.0001f, "deg"),
  SET_FLOAT("Longitude", get_lon, set_lon, -180.0f, 180.0f, 0.0001f, "deg"),
  SET_BOOL("GPS enabled", get_gps, set_gps),
  SET_INT("GPS interval", get_gint, set_gint, 0, 86400, 30, "s"),
};

// ---- Telemetry ----
static const EnumOpt OPT_TELEM[] = {{"Deny", 0}, {"Flags", 1}, {"All", 2}};
static int32_t get_tb() { return S.telem_base; } static bool set_tb(int32_t v) { S.telem_base = v; return true; }
static int32_t get_tl() { return S.telem_loc; }  static bool set_tl(int32_t v) { S.telem_loc = v; return true; }
static int32_t get_te() { return S.telem_env; }  static bool set_te(int32_t v) { S.telem_env = v; return true; }
static const Setting GRP_TELEMETRY[] = {
  SET_ENUM("Base", get_tb, set_tb, OPT_TELEM, 3),
  SET_ENUM("Location", get_tl, set_tl, OPT_TELEM, 3),
  SET_ENUM("Environment", get_te, set_te, OPT_TELEM, 3),
};

// ---- Experimental ----
static float get_air() { return S.airtime; } static bool set_air(float v) { S.airtime = v; return true; }
static float get_rx() { return S.rx_delay; } static bool set_rx(float v) { S.rx_delay = v; return true; }
static char scope_buf[40];
static const char* info_scope() { snprintf(scope_buf, sizeof(scope_buf), "%s", S.scope_name[0] ? S.scope_name : "(none)"); return scope_buf; }
static void act_clear_scope(UITask* t) { S.scope_name[0] = 0; if (t) t->showAlert("Scope cleared", 1000); }
static const Setting GRP_EXPERIMENTAL[] = {
  SET_FLOAT("Airtime factor", get_air, set_air, 0.0f, 10.0f, 0.1f, ""),
  SET_FLOAT("RX delay base", get_rx, set_rx, 0.0f, 10.0f, 0.1f, "s"),
  SET_INFO("Default scope", info_scope),
  SET_ACTION("Clear default scope", act_clear_scope),
};

// ---- Device ----
static const char* info_batt() { return "4050mV 120/1536KB"; }
static const char* info_fw() { return "v1.16.0 (sim)"; }
static const char* info_dev() { return "LilyGo T-Deck (sim)"; }
static char pin_buf[8];
static const char* get_pin() { snprintf(pin_buf, sizeof(pin_buf), "%lu", S.ble_pin); return pin_buf; }
static bool set_pin(const char* s) { unsigned long v = (unsigned long)atol(s); if (v != 0 && (v < 100000 || v > 999999)) return false; S.ble_pin = v; return true; }
static void act_reboot(UITask* t) { if (t) t->showAlert("Reboot (sim)", 1200); }
static void act_factory(UITask* t) { if (t) t->showAlert("Factory reset (sim)", 1200); }
static const Setting GRP_DEVICE[] = {
  SET_INFO("Battery/storage", info_batt),
  SET_INFO("Firmware", info_fw),
  SET_INFO("Device", info_dev),
  SET_STRING("BLE pin", get_pin, set_pin),
  SET_ACTION("Reboot", act_reboot),
  SET_ACTION("Factory reset", act_factory),
};

// ---- Root ----
#define GRP(title, arr) {title, arr, (uint8_t)(sizeof(arr) / sizeof(arr[0]))}
const SettingsGroup SETTINGS_ROOT[] = {
  GRP("Public info", GRP_PUBLIC),
  GRP("Radio", GRP_RADIO),
  GRP("Contacts", GRP_CONTACTS),
  GRP("Message", GRP_MESSAGE),
  GRP("Position", GRP_POSITION),
  GRP("Telemetry", GRP_TELEMETRY),
  GRP("Experimental", GRP_EXPERIMENTAL),
  GRP("Device", GRP_DEVICE),
};
const uint8_t SETTINGS_ROOT_COUNT = sizeof(SETTINGS_ROOT) / sizeof(SETTINGS_ROOT[0]);
