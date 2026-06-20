#pragma once

#include <stdint.h>

// Data-driven settings model for the standalone on-device UI.
//
// Each Setting is a descriptor pointing at thin getter/setter "forwarder" free
// functions (see SettingsModel.cpp) that call the shared MyMesh config ops. The
// generic settings screens render and edit any descriptor, so adding a setting
// is just one table row + a small forwarder -- no new screen code.
//
// All tables are `static const` (live in flash); no dynamic allocation.

class UITask;  // forward decl (ACTION callbacks receive the owning UITask)

enum SettingType : uint8_t {
  ST_BOOL,     // on/off toggle              (getInt/setInt: 0 or 1)
  ST_ENUM,     // pick one of opts[]         (getInt/setInt: opt value)
  ST_INT,      // integer w/ min/max/step    (getInt/setInt)
  ST_FLOAT,    // float w/ fmin/fmax/fstep   (getFloat/setFloat)
  ST_STRING,   // free text                  (getStr/setStr)
  ST_ACTION,   // run an action              (action)
  ST_INFO,     // read-only display          (getStr)
};

struct EnumOpt {
  const char* label;
  int32_t     value;
};

struct Setting {
  const char* label;
  SettingType type;
  int32_t     (*getInt)();
  bool        (*setInt)(int32_t);     // returns false on invalid value
  float       (*getFloat)();
  bool        (*setFloat)(float);
  const char* (*getStr)();
  bool        (*setStr)(const char*);
  void        (*action)(UITask*);
  const EnumOpt* opts;
  uint8_t     num_opts;
  int32_t     imin, imax, istep;
  float       fmin, fmax, fstep;
  const char* units;                  // shown after the value, e.g. "dBm" (may be NULL)
};

struct SettingsGroup {
  const char*    title;
  const Setting* items;
  uint8_t        count;
};

// field order matches the Setting struct exactly; unused fields are zero/NULL
#define SET_BOOL(label, get, set) \
  { label, ST_BOOL, get, set, nullptr,nullptr, nullptr,nullptr, nullptr, nullptr,0, 0,0,0, 0,0,0, nullptr }
#define SET_ENUM(label, get, set, opts, n) \
  { label, ST_ENUM, get, set, nullptr,nullptr, nullptr,nullptr, nullptr, opts,n, 0,0,0, 0,0,0, nullptr }
#define SET_INT(label, get, set, lo, hi, step, units) \
  { label, ST_INT, get, set, nullptr,nullptr, nullptr,nullptr, nullptr, nullptr,0, lo,hi,step, 0,0,0, units }
#define SET_FLOAT(label, get, set, lo, hi, step, units) \
  { label, ST_FLOAT, nullptr,nullptr, get,set, nullptr,nullptr, nullptr, nullptr,0, 0,0,0, lo,hi,step, units }
#define SET_STRING(label, get, set) \
  { label, ST_STRING, nullptr,nullptr, nullptr,nullptr, get,set, nullptr, nullptr,0, 0,0,0, 0,0,0, nullptr }
#define SET_ACTION(label, act) \
  { label, ST_ACTION, nullptr,nullptr, nullptr,nullptr, nullptr,nullptr, act, nullptr,0, 0,0,0, 0,0,0, nullptr }
#define SET_INFO(label, get) \
  { label, ST_INFO, nullptr,nullptr, nullptr,nullptr, get,nullptr, nullptr, nullptr,0, 0,0,0, 0,0,0, nullptr }

// Radio frequency-plan presets (region settings). Selecting one applies
// freq/bw/sf/cr together. Values are regulatory — extend with care.
struct RadioPreset {
  const char* name;
  float   freq;  // MHz
  float   bw;    // kHz
  uint8_t sf;
  uint8_t cr;
};
extern const RadioPreset RADIO_PRESETS[];
extern const uint8_t RADIO_PRESETS_COUNT;

// The root list of categories. Defined in SettingsModel.cpp.
extern const SettingsGroup SETTINGS_ROOT[];
extern const uint8_t SETTINGS_ROOT_COUNT;
