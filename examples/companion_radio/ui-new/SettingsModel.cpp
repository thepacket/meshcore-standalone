#include "SettingsModel.h"
#include <math.h>
#include <stdlib.h>
#include "../MyMesh.h"
#include "UITask.h"

// Thin forwarders: each reads/writes via the shared MyMesh config API (the same
// ops the frame protocol uses), so the on-device UI and the phone app stay in lockstep.

static inline NodePrefs* P() { return the_mesh.getNodePrefs(); }

// ---------------- Public info ----------------
static const char* get_node_name() { return P()->node_name; }
static bool set_node_name(const char* s) { the_mesh.setAdvertName(s); return true; }

static const EnumOpt OPT_LOC[] = { {"Hidden", ADVERT_LOC_NONE}, {"Shared", ADVERT_LOC_SHARE} };
static int32_t get_loc_policy() { return P()->advert_loc_policy; }
static bool set_loc_policy(int32_t v) { the_mesh.setAdvertLocPolicy((uint8_t)v); return true; }

static void act_advert(UITask* t) {
  bool ok = the_mesh.advert();
  if (t) t->showAlert(ok ? "Advert sent" : "Advert failed", 1200);
}
static void act_advert_flood(UITask* t) {
  bool ok = the_mesh.advertFlood();
  if (t) t->showAlert(ok ? "Flood advert sent" : "Advert failed", 1200);
}

static const Setting GRP_PUBLIC[] = {
  SET_STRING("Node name", get_node_name, set_node_name),
  SET_ENUM  ("Location", get_loc_policy, set_loc_policy, OPT_LOC, 2),
  SET_ACTION("Send advert", act_advert),
  SET_ACTION("Send advert (flood)", act_advert_flood),
};

// ---------------- Radio presets ----------------
// Region frequency plans. Add a row + a matching OPT_PRESET entry to extend.
const RadioPreset RADIO_PRESETS[] = {
  {"USA/Canada", 910.525f, 62.5f, 7, 5},
  {"EU/UK",      869.618f, 62.5f, 8, 8},
};
const uint8_t RADIO_PRESETS_COUNT = sizeof(RADIO_PRESETS) / sizeof(RADIO_PRESETS[0]);

// ENUM options: "Custom" (-1) plus one per preset (value = index). Keep in sync.
static const EnumOpt OPT_PRESET[] = {
  {"Custom", -1}, {"USA/Canada", 0}, {"EU/UK", 1},
};
static int32_t get_preset() {
  NodePrefs* p = P();
  for (uint8_t i = 0; i < RADIO_PRESETS_COUNT; i++) {
    const RadioPreset& r = RADIO_PRESETS[i];
    if (fabsf(p->freq - r.freq) < 0.01f && fabsf(p->bw - r.bw) < 0.01f && p->sf == r.sf && p->cr == r.cr)
      return i;
  }
  return -1;  // no match -> Custom
}
static bool set_preset(int32_t idx) {
  if (idx < 0 || idx >= RADIO_PRESETS_COUNT) return true;  // "Custom" -> no-op
  const RadioPreset& r = RADIO_PRESETS[idx];
  return the_mesh.setRadioParams((uint32_t)lroundf(r.freq * 1000.0f), (uint32_t)lroundf(r.bw * 1000.0f),
                                 r.sf, r.cr, P()->client_repeat);
}

// ---------------- Radio ----------------
// Frequency is typed on the keyboard (it needs decimals), not stepped with +/-.
// Allowed frequency bands (MHz). Extend as more regions are supported.
static bool freqAllowed(float mhz) {
  return (mhz >= 902.0f && mhz <= 928.0f) ||      // 915 MHz ISM band
         (mhz >= 869.40f && mhz <= 869.65f);      // EU 869 MHz band
}
static const char* get_freq() {
  static char b[12];
  snprintf(b, sizeof(b), "%g", P()->freq);
  return b;
}
static bool set_freq(const char* s) {
  float mhz = atof(s);
  if (!freqAllowed(mhz)) return false;
  NodePrefs* p = P();
  return the_mesh.setRadioParams((uint32_t)lroundf(mhz * 1000.0f), (uint32_t)lroundf(p->bw * 1000.0f),
                                 p->sf, p->cr, p->client_repeat);
}
// bandwidth stored in kHz; the radio op's bw arg is in Hz (kHz*1000)
static const EnumOpt OPT_BW[] = {
  {"7.8", 7800}, {"10.4", 10400}, {"15.6", 15600}, {"20.8", 20800}, {"31.25", 31250},
  {"41.7", 41700}, {"62.5", 62500}, {"125", 125000}, {"250", 250000}, {"500", 500000},
};
static int32_t get_bw() { return (int32_t)lroundf(P()->bw * 1000.0f); }
static bool set_bw(int32_t hz) {
  NodePrefs* p = P();
  return the_mesh.setRadioParams((uint32_t)lroundf(p->freq * 1000.0f), (uint32_t)hz, p->sf, p->cr,
                                 p->client_repeat);
}
static int32_t get_sf() { return P()->sf; }
static bool set_sf(int32_t v) {
  NodePrefs* p = P();
  return the_mesh.setRadioParams((uint32_t)lroundf(p->freq * 1000.0f), (uint32_t)lroundf(p->bw * 1000.0f),
                                 (uint8_t)v, p->cr, p->client_repeat);
}
static int32_t get_cr() { return P()->cr; }
static bool set_cr(int32_t v) {
  NodePrefs* p = P();
  return the_mesh.setRadioParams((uint32_t)lroundf(p->freq * 1000.0f), (uint32_t)lroundf(p->bw * 1000.0f),
                                 p->sf, (uint8_t)v, p->client_repeat);
}
static int32_t get_txp() { return P()->tx_power_dbm; }
static bool set_txp(int32_t v) { return the_mesh.setTxPower((int8_t)v); }

static int32_t get_repeat() { return P()->client_repeat; }
static bool set_repeat(int32_t v) {
  NodePrefs* p = P();
  return the_mesh.setRadioParams((uint32_t)lroundf(p->freq * 1000.0f), (uint32_t)lroundf(p->bw * 1000.0f),
                                 p->sf, p->cr, (uint8_t)(v ? 1 : 0));
}

static const Setting GRP_RADIO[] = {
  SET_ENUM("Preset", get_preset, set_preset, OPT_PRESET, 3),
  SET_STRING("Frequency (MHz)", get_freq, set_freq),
  SET_ENUM ("Bandwidth", get_bw, set_bw, OPT_BW, 10),
  SET_INT  ("Spread factor", get_sf, set_sf, 5, 12, 1, ""),
  SET_INT  ("Coding rate", get_cr, set_cr, 5, 8, 1, ""),
  SET_INT  ("TX power", get_txp, set_txp, -9, MAX_LORA_TX_POWER, 1, "dBm"),
  SET_BOOL ("Client repeat", get_repeat, set_repeat),
};

// ---------------- Contacts ----------------
// auto-add bitmask bits (mirror MyMesh.cpp)
#define AA_OVERWRITE  (1 << 0)
#define AA_CHAT       (1 << 1)
#define AA_REPEATER   (1 << 2)
#define AA_ROOM       (1 << 3)
#define AA_SENSOR     (1 << 4)
static int32_t aa_get(uint8_t bit) { return (P()->autoadd_config & bit) ? 1 : 0; }
static void aa_set(uint8_t bit, int32_t v) {
  uint8_t m = P()->autoadd_config;
  if (v) m |= bit; else m &= ~bit;
  the_mesh.setAutoAddConfig(m);
}
static int32_t get_aa_chat()     { return aa_get(AA_CHAT); }
static bool    set_aa_chat(int32_t v)     { aa_set(AA_CHAT, v); return true; }
static int32_t get_aa_repeater() { return aa_get(AA_REPEATER); }
static bool    set_aa_repeater(int32_t v) { aa_set(AA_REPEATER, v); return true; }
static int32_t get_aa_room()     { return aa_get(AA_ROOM); }
static bool    set_aa_room(int32_t v)     { aa_set(AA_ROOM, v); return true; }
static int32_t get_aa_sensor()   { return aa_get(AA_SENSOR); }
static bool    set_aa_sensor(int32_t v)   { aa_set(AA_SENSOR, v); return true; }
static int32_t get_aa_overwrite() { return aa_get(AA_OVERWRITE); }
static bool    set_aa_overwrite(int32_t v) { aa_set(AA_OVERWRITE, v); return true; }

static int32_t get_manual() { return P()->manual_add_contacts; }
static bool set_manual(int32_t v) { the_mesh.setManualAdd(v != 0); return true; }
static int32_t get_maxhops() { return P()->autoadd_max_hops; }
static bool set_maxhops(int32_t v) { the_mesh.setMaxHops((uint8_t)v); return true; }

static const Setting GRP_CONTACTS[] = {
  SET_BOOL("Manual add", get_manual, set_manual),
  SET_BOOL("Auto-add users", get_aa_chat, set_aa_chat),
  SET_BOOL("Auto-add repeaters", get_aa_repeater, set_aa_repeater),
  SET_BOOL("Auto-add rooms", get_aa_room, set_aa_room),
  SET_BOOL("Auto-add sensors", get_aa_sensor, set_aa_sensor),
  SET_BOOL("Overwrite oldest", get_aa_overwrite, set_aa_overwrite),
  SET_INT ("Max hops", get_maxhops, set_maxhops, 0, 64, 1, ""),
};

// ---------------- Message ----------------
static int32_t get_multiacks() { return P()->multi_acks; }
static bool set_multiacks(int32_t v) { the_mesh.setMultiAcks((uint8_t)v); return true; }
static const EnumOpt OPT_PHM[] = { {"Off", 0}, {"Mode 1", 1}, {"Mode 2", 2} };
static int32_t get_phm() { return P()->path_hash_mode; }
static bool set_phm(int32_t v) { return the_mesh.setPathHashMode((uint8_t)v); }

static const Setting GRP_MESSAGE[] = {
  SET_INT ("Multi-acks", get_multiacks, set_multiacks, 0, 4, 1, ""),
  SET_ENUM("Path hash mode", get_phm, set_phm, OPT_PHM, 3),
};

// ---------------- Position ----------------
static float get_lat() { return (float)sensors.node_lat; }
static bool set_lat(float v) {
  return the_mesh.setAdvertLatLon((int32_t)lroundf(v * 1000000.0f),
                                  (int32_t)lroundf((float)sensors.node_lon * 1000000.0f));
}
static float get_lon() { return (float)sensors.node_lon; }
static bool set_lon(float v) {
  return the_mesh.setAdvertLatLon((int32_t)lroundf((float)sensors.node_lat * 1000000.0f),
                                  (int32_t)lroundf(v * 1000000.0f));
}
#if ENV_INCLUDE_GPS == 1
static int32_t get_gps() { return P()->gps_enabled; }
static bool set_gps(int32_t v) { the_mesh.setGpsEnabled(v != 0); return true; }
static int32_t get_gpsint() { return (int32_t)P()->gps_interval; }
static bool set_gpsint(int32_t v) { the_mesh.setGpsInterval((uint32_t)v); return true; }
#endif

static const Setting GRP_POSITION[] = {
  SET_FLOAT("Latitude", get_lat, set_lat, -90.0f, 90.0f, 0.0001f, "deg"),
  SET_FLOAT("Longitude", get_lon, set_lon, -180.0f, 180.0f, 0.0001f, "deg"),
#if ENV_INCLUDE_GPS == 1
  SET_BOOL ("GPS enabled", get_gps, set_gps),
  SET_INT  ("GPS interval", get_gpsint, set_gpsint, 0, 86400, 30, "s"),
#endif
};

// ---------------- Telemetry ----------------
static const EnumOpt OPT_TELEM[] = {
  {"Deny", TELEM_MODE_DENY}, {"Flags", TELEM_MODE_ALLOW_FLAGS}, {"All", TELEM_MODE_ALLOW_ALL},
};
static int32_t get_tb() { return P()->telemetry_mode_base; }
static bool set_tb(int32_t v) { the_mesh.setTelemetryModes((uint8_t)v, P()->telemetry_mode_loc, P()->telemetry_mode_env); return true; }
static int32_t get_tl() { return P()->telemetry_mode_loc; }
static bool set_tl(int32_t v) { the_mesh.setTelemetryModes(P()->telemetry_mode_base, (uint8_t)v, P()->telemetry_mode_env); return true; }
static int32_t get_te() { return P()->telemetry_mode_env; }
static bool set_te(int32_t v) { the_mesh.setTelemetryModes(P()->telemetry_mode_base, P()->telemetry_mode_loc, (uint8_t)v); return true; }

static const Setting GRP_TELEMETRY[] = {
  SET_ENUM("Base", get_tb, set_tb, OPT_TELEM, 3),
  SET_ENUM("Location", get_tl, set_tl, OPT_TELEM, 3),
  SET_ENUM("Environment", get_te, set_te, OPT_TELEM, 3),
};

// ---------------- Experimental ----------------
static float get_airtime() { return P()->airtime_factor; }
static bool set_airtime(float v) { the_mesh.setTuningParams(P()->rx_delay_base, v); return true; }
static float get_rxdelay() { return P()->rx_delay_base; }
static bool set_rxdelay(float v) { the_mesh.setTuningParams(v, P()->airtime_factor); return true; }
static char scope_buf[40];
static const char* info_scope() {
  const char* n = P()->default_scope_name;
  snprintf(scope_buf, sizeof(scope_buf), "%s", (n && n[0]) ? n : "(none)");
  return scope_buf;
}
static void act_clear_scope(UITask* t) {
  the_mesh.setDefaultFloodScope(NULL, NULL);
  if (t) t->showAlert("Scope cleared", 1000);
}

static const Setting GRP_EXPERIMENTAL[] = {
  SET_FLOAT ("Airtime factor", get_airtime, set_airtime, 0.0f, 10.0f, 0.1f, ""),
  SET_FLOAT ("RX delay base", get_rxdelay, set_rxdelay, 0.0f, 10.0f, 0.1f, "s"),
  SET_INFO  ("Default scope", info_scope),
  SET_ACTION("Clear default scope", act_clear_scope),
};

// ---------------- Device / Admin ----------------
static char batt_buf[48];
static const char* info_batt() {
  uint16_t mv; uint32_t used, total;
  the_mesh.getBattAndStorage(mv, used, total);
  snprintf(batt_buf, sizeof(batt_buf), "%umV %lu/%luKB", mv, (unsigned long)used, (unsigned long)total);
  return batt_buf;
}
static const char* info_fw() { return FIRMWARE_VERSION; }
static const char* info_dev() { return board.getManufacturerName(); }
static char pin_buf[8];
static const char* get_pin() { snprintf(pin_buf, sizeof(pin_buf), "%lu", (unsigned long)P()->ble_pin); return pin_buf; }
static bool set_pin(const char* s) { return the_mesh.setBlePin((uint32_t)atol(s)); }
static void act_reboot(UITask* t) { (void)t; the_mesh.rebootDevice(); }
static void act_factory(UITask* t) { (void)t; the_mesh.factoryReset(); }

static const Setting GRP_DEVICE[] = {
  SET_INFO  ("Battery/storage", info_batt),
  SET_INFO  ("Firmware", info_fw),
  SET_INFO  ("Device", info_dev),
  SET_STRING("BLE pin", get_pin, set_pin),
  SET_ACTION_CONFIRM("Reboot", act_reboot),
  SET_ACTION_CONFIRM("Factory reset", act_factory),
};

// ---------------- Root ----------------
#define GRP(title, arr) { title, arr, (uint8_t)(sizeof(arr) / sizeof(arr[0])) }
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
