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

#ifdef __cplusplus
}
#endif
