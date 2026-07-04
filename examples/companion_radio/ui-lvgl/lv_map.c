// LVGL offline map: raw RGB565 slippy tiles from the SD card blitted into a
// full-screen canvas, with touch pan, +/- zoom, a "you are here" marker from
// GPS, and dots for saved contacts that carry a GPS fix. Tiles live at
// /maps/{z}/{x}/{y}.bin (256x256, little-endian RGB565) -- generate them with
// tools/gen_map_tiles.py. Web-mercator (XYZ) projection, matching OSM.
#include "lv_ui.h"
#include "lv_data.h"
#include <math.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>

void lv_chat_set_peer(const char* name);   // defined in lv_chat.c; opens the peer card by name

#define MW   320
#define MH   240
#define TILE LVD_MAP_TILE_PX   // 256
#define TCACHE 8               // resident tiles (PSRAM): >= the ~6 ever visible
#define MAXMARK 64             // contact markers snapshotted on entry

#define RGB565(r, g, b) ((uint16_t)(((r) & 0xF8) << 8 | ((g) & 0xFC) << 3 | (b) >> 3))
static const uint16_t PLACEHOLDER = RGB565(0x14, 0x1b, 0x22);   // "no tile" dark slate

// ---- view state (persists across redraws; re-seeded on screen entry) --------
static double s_cx = 0, s_cy = 0;   // centre lon, lat (deg)
static int    s_z = 0, s_zmin = -1, s_zmax = -1;
static bool   s_seeded = false;

static lv_obj_t* s_canvas = NULL;
static uint16_t* s_cbuf = NULL;     // canvas framebuffer (PSRAM)
static lv_obj_t* s_status = NULL;   // top pill: zoom + fix
static int s_lastx = 0, s_lasty = 0;
static uint32_t s_gen_seen = 0;     // last fetch generation drawn (tiles pop in on change)

// ---- tile cache -------------------------------------------------------------
static struct { int z, x, y; uint16_t* buf; bool valid; } s_tc[TCACHE];
static int s_tc_next = 0;

static uint16_t* tile_get(int z, int x, int y) {
  for (int i = 0; i < TCACHE; i++)
    if (s_tc[i].valid && s_tc[i].z == z && s_tc[i].x == x && s_tc[i].y == y) return s_tc[i].buf;
  int s = s_tc_next; s_tc_next = (s_tc_next + 1) % TCACHE;
  if (!s_tc[s].buf) {
    s_tc[s].buf = (uint16_t*)ps_malloc((size_t)TILE * TILE * 2);
    if (!s_tc[s].buf) s_tc[s].buf = (uint16_t*)malloc((size_t)TILE * TILE * 2);
    if (!s_tc[s].buf) return NULL;
  }
  s_tc[s].valid = false;
  if (!lvd_map_tile(z, x, y, (unsigned char*)s_tc[s].buf, TILE * TILE * 2)) {
    lvd_map_fetch(z, x, y);   // not on SD -> queue a Wi-Fi fetch (no-op if offline/already tried)
    return NULL;
  }
  s_tc[s].z = z; s_tc[s].x = x; s_tc[s].y = y; s_tc[s].valid = true;
  return s_tc[s].buf;
}
static void tile_cache_reset(void) {   // zoom change invalidates the (z-keyed) cache lazily anyway,
  for (int i = 0; i < TCACHE; i++) s_tc[i].valid = false;   // but clear so evicted PSRAM is reused
}

// ---- web-mercator helpers (deg <-> world pixels at the current zoom) --------
static double lon2wx(double lon, double n) { return (lon + 180.0) / 360.0 * n; }
static double lat2wy(double lat, double n) {
  double r = lat * M_PI / 180.0;
  return (1.0 - log(tan(r) + 1.0 / cos(r)) / M_PI) / 2.0 * n;
}
static double wx2lon(double wx, double n) { return wx / n * 360.0 - 180.0; }
static double wy2lat(double wy, double n) { return atan(sinh(M_PI * (1.0 - 2.0 * wy / n))) * 180.0 / M_PI; }

// ---- markers (snapshot on entry) --------------------------------------------
static struct { double lat, lon; uint32_t col; char name[32]; } s_mk[MAXMARK];
static int       s_mk_n = 0;
static lv_obj_t* s_mk_obj[MAXMARK];   // reused dots
static lv_obj_t* s_self = NULL;       // "you are here"

static void marker_click(lv_event_t* e) {   // tap a node dot -> its peer card
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  if (i < 0 || i >= s_mk_n) return;
  lv_chat_set_peer(s_mk[i].name);
  if (lv_nav_cb) lv_nav_cb("peer");
}

static uint32_t marker_colour(int type) {   // rough MeshCore ADV_TYPE colouring
  switch (type) { case 2: return UI_PURPLE;   // repeater
                  case 3: return UI_AMBER;     // room
                  case 4: return UI_TEAL;      // sensor
                  default: return UI_GREEN; }  // chat/user
}
static void markers_snapshot(void) {
  s_mk_n = 0;
  int n = lvd_map_marker_count();
  for (int i = 0; i < n && s_mk_n < MAXMARK; i++) {
    lvd_marker_t m;
    if (!lvd_map_marker_get(i, &m)) continue;
    s_mk[s_mk_n].lat = m.lat; s_mk[s_mk_n].lon = m.lon;
    s_mk[s_mk_n].col = marker_colour(m.type);
    strncpy(s_mk[s_mk_n].name, m.name, sizeof(s_mk[0].name) - 1);
    s_mk[s_mk_n].name[sizeof(s_mk[0].name) - 1] = 0;
    s_mk_n++;
  }
}
static lv_obj_t* make_dot(lv_obj_t* parent, int d, uint32_t col, uint32_t ring) {
  lv_obj_t* o = lv_obj_create(parent);
  lv_obj_remove_flag(o, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(o, d, d);
  lv_obj_set_style_radius(o, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(o, lv_color_hex(col), 0);
  lv_obj_set_style_bg_opa(o, LV_OPA_COVER, 0);
  lv_obj_set_style_border_color(o, lv_color_hex(ring), 0);
  lv_obj_set_style_border_width(o, 2, 0);
  lv_obj_set_style_pad_all(o, 0, 0);
  lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN);
  return o;
}

// ---- rendering --------------------------------------------------------------
static void blit_tile(const uint16_t* t, int dx, int dy) {
  int sx = dx < 0 ? -dx : 0, sy = dy < 0 ? -dy : 0;
  int ex = dx + TILE > MW ? MW - dx : TILE;
  int ey = dy + TILE > MH ? MH - dy : TILE;
  for (int row = sy; row < ey; row++)
    if (ex > sx) memcpy(&s_cbuf[(dy + row) * MW + dx + sx], &t[row * TILE + sx], (size_t)(ex - sx) * 2);
}
static void place_one(lv_obj_t* o, double lat, double lon, double vx, double vy, double n, int d) {
  double wx = lon2wx(lon, n), wy = lat2wy(lat, n);
  int px = (int)lround(wx - vx), py = (int)lround(wy - vy);
  if (px < -d || py < -d || px > MW + d || py > MH + d) { lv_obj_add_flag(o, LV_OBJ_FLAG_HIDDEN); return; }
  lv_obj_set_pos(o, px - d / 2, py - d / 2);
  lv_obj_clear_flag(o, LV_OBJ_FLAG_HIDDEN);
  lv_obj_move_foreground(o);
}

static void map_redraw(void) {
  if (!s_cbuf) return;
  for (int i = 0; i < MW * MH; i++) s_cbuf[i] = PLACEHOLDER;

  double n = 256.0 * (double)(1u << s_z);
  int ntiles = 1 << s_z;
  double vx = lon2wx(s_cx, n) - MW / 2.0;
  double vy = lat2wy(s_cy, n) - MH / 2.0;

  int tx0 = (int)floor(vx / TILE), tx1 = (int)floor((vx + MW - 1) / TILE);
  int ty0 = (int)floor(vy / TILE), ty1 = (int)floor((vy + MH - 1) / TILE);
  for (int ty = ty0; ty <= ty1; ty++) {
    if (ty < 0 || ty >= ntiles) continue;
    for (int tx = tx0; tx <= tx1; tx++) {
      int wx = ((tx % ntiles) + ntiles) % ntiles;   // wrap longitude
      uint16_t* t = tile_get(s_z, wx, ty);
      if (t) blit_tile(t, (int)lround(tx * (double)TILE - vx), (int)lround(ty * (double)TILE - vy));
    }
  }

  for (int i = 0; i < s_mk_n; i++) place_one(s_mk_obj[i], s_mk[i].lat, s_mk[i].lon, vx, vy, n, 14);
  double slat, slon;
  if (s_self && lvd_map_here(&slat, &slon)) place_one(s_self, slat, slon, vx, vy, n, 14);
  else if (s_self) lv_obj_add_flag(s_self, LV_OBJ_FLAG_HIDDEN);

  if (s_status) {
    char b[40];
    if (lvd_map_here(&slat, &slon)) snprintf(b, sizeof(b), "z%d  %.4f, %.4f", s_z, s_cy, s_cx);
    else                           snprintf(b, sizeof(b), "z%d  (no GPS fix)", s_z);
    lv_label_set_text(s_status, b);
  }
  lv_obj_invalidate(s_canvas);
}

// ---- interaction ------------------------------------------------------------
static void canvas_ev(lv_event_t* e) {
  lv_event_code_t c = lv_event_get_code(e);
  lv_indev_t* indev = lv_indev_active();
  if (!indev) return;
  lv_point_t p; lv_indev_get_point(indev, &p);
  if (c == LV_EVENT_PRESSED) { s_lastx = p.x; s_lasty = p.y; return; }
  if (c != LV_EVENT_PRESSING) return;
  int ddx = p.x - s_lastx, ddy = p.y - s_lasty;
  if (!ddx && !ddy) return;
  double n = 256.0 * (double)(1u << s_z);
  double cwx = lon2wx(s_cx, n) - ddx, cwy = lat2wy(s_cy, n) - ddy;
  if (cwy < 0) cwy = 0; else if (cwy > n) cwy = n;   // clamp to the poles
  s_cx = wx2lon(cwx, n); s_cy = wy2lat(cwy, n);
  if (s_cx < -180) s_cx += 360; else if (s_cx > 180) s_cx -= 360;
  s_lastx = p.x; s_lasty = p.y;
  map_redraw();
}
static void zoom_by(int d) {
  int z = s_z + d;
  if (s_zmin >= 0 && z < s_zmin) z = s_zmin;
  if (s_zmax >= 0 && z > s_zmax) z = s_zmax;
  if (z != s_z) { s_z = z; tile_cache_reset(); map_redraw(); }
}
static void zoom_in_ev(lv_event_t* e)  { (void)e; zoom_by(+1); }
static void zoom_out_ev(lv_event_t* e) { (void)e; zoom_by(-1); }
static void locate_ev(lv_event_t* e) {
  (void)e;
  double lat, lon;
  if (lvd_map_here(&lat, &lon)) { s_cx = lon; s_cy = lat; map_redraw(); }
  else lv_ui_toast("No GPS fix yet");
}
static void back_ev(lv_event_t* e) { (void)e; if (lv_nav_cb) lv_nav_cb("back"); }

// ~1 Hz refresh hook: redraw when a Wi-Fi-fetched tile has landed on SD
static void map_refresh(void) {
  uint32_t g = lvd_map_fetch_gen();
  if (g != s_gen_seen) { s_gen_seen = g; map_redraw(); }
}

// small translucent round control button with a symbol label
static lv_obj_t* ctl_btn(lv_obj_t* scr, int x, int y, const char* sym, lv_event_cb_t cb) {
  lv_obj_t* b = lv_obj_create(scr);
  lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(b, 40, 40); lv_obj_set_pos(b, x, y);
  lv_obj_set_style_radius(b, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(0x0d1218), 0); lv_obj_set_style_bg_opa(b, 190, 0);
  lv_obj_set_style_border_color(b, lv_color_hex(MD_PRIMARY), 0); lv_obj_set_style_border_width(b, 1, 0);
  lv_obj_set_style_pad_all(b, 0, 0);
  lv_obj_add_flag(b, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(b, cb, LV_EVENT_CLICKED, NULL);
  lv_ui_press_fx(b);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, sym);
  lv_obj_set_style_text_color(l, lv_color_hex(0xffffff), 0);
  lv_obj_center(l);
  return b;
}

void lv_map_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lvd_map_zoom_range(&s_zmin, &s_zmax);
  // when Wi-Fi is up we can fetch any tile on demand -> allow a broad zoom range
  // regardless of what's cached; offline we're limited to what's on the card.
  if (lvd_map_online()) {
    if (s_zmin < 0 || s_zmin > 3) s_zmin = 3;
    if (s_zmax < 0 || s_zmax < 16) s_zmax = 16;
  }

  // full-screen canvas (PSRAM buffer kept across rebuilds)
  if (!s_cbuf) {
    s_cbuf = (uint16_t*)ps_malloc((size_t)MW * MH * 2);
    if (!s_cbuf) s_cbuf = (uint16_t*)malloc((size_t)MW * MH * 2);
  }
  if (!s_cbuf || s_zmax < 0) {
    lv_ui_md_topbar(scr, "Map");
    lv_obj_t* h = lv_label_create(scr);
    lv_label_set_text(h, !s_cbuf ? "Out of memory for the map."
                                 : "No map tiles yet.\n\nTurn on Wi-Fi (Settings > Wi-Fi) and reopen "
                                   "the map -- tiles download automatically and cache to the SD card.");
    lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP); lv_obj_set_width(h, 300);
    lv_obj_align(h, LV_ALIGN_CENTER, 0, 0);
    lv_obj_set_style_text_color(h, lv_color_hex(MD_MUTED), 0);
    lv_obj_set_style_text_align(h, LV_TEXT_ALIGN_CENTER, 0);
    return;
  }

  // seed the view once: centre on our GPS fix (or 0,0), mid zoom
  if (!s_seeded) {
    double lat, lon;
    if (lvd_map_here(&lat, &lon)) { s_cx = lon; s_cy = lat; }
    s_z = (s_zmin + s_zmax) / 2;
    s_seeded = true;
  }
  if (s_z < s_zmin) s_z = s_zmin;
  if (s_z > s_zmax) s_z = s_zmax;

  s_canvas = lv_canvas_create(scr);
  lv_canvas_set_buffer(s_canvas, s_cbuf, MW, MH, LV_COLOR_FORMAT_RGB565);
  lv_obj_set_pos(s_canvas, 0, 0);
  lv_obj_add_flag(s_canvas, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(s_canvas, canvas_ev, LV_EVENT_PRESSED, NULL);
  lv_obj_add_event_cb(s_canvas, canvas_ev, LV_EVENT_PRESSING, NULL);

  markers_snapshot();
  for (int i = 0; i < s_mk_n; i++) {
    s_mk_obj[i] = make_dot(scr, 14, s_mk[i].col, 0x0d1218);
    lv_obj_add_flag(s_mk_obj[i], LV_OBJ_FLAG_CLICKABLE);      // tap -> peer card
    lv_obj_set_ext_click_area(s_mk_obj[i], 8);                // enlarge the touch target
    lv_obj_add_event_cb(s_mk_obj[i], marker_click, LV_EVENT_CLICKED, (void*)(intptr_t)i);
  }
  s_self = make_dot(scr, 14, UI_CYAN, 0xffffff);

  // status pill (top-centre) + controls
  lv_obj_t* pill = lv_obj_create(scr);
  lv_obj_remove_flag(pill, LV_OBJ_FLAG_SCROLLABLE | LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_size(pill, LV_SIZE_CONTENT, 24); lv_obj_align(pill, LV_ALIGN_TOP_MID, 0, 8);
  lv_obj_set_style_radius(pill, 12, 0);
  lv_obj_set_style_bg_color(pill, lv_color_hex(0x0d1218), 0); lv_obj_set_style_bg_opa(pill, 190, 0);
  lv_obj_set_style_border_width(pill, 0, 0);
  lv_obj_set_style_pad_hor(pill, 10, 0); lv_obj_set_style_pad_ver(pill, 2, 0);
  s_status = lv_label_create(pill);
  lv_obj_set_style_text_font(s_status, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(s_status, lv_color_hex(UI_TEXT), 0);
  lv_obj_center(s_status);

  ctl_btn(scr, 8, 8, LV_SYMBOL_LEFT, back_ev);
  ctl_btn(scr, MW - 48, 60, LV_SYMBOL_PLUS, zoom_in_ev);
  ctl_btn(scr, MW - 48, 108, LV_SYMBOL_MINUS, zoom_out_ev);
  ctl_btn(scr, 8, MH - 48, LV_SYMBOL_GPS, locate_ev);

  s_gen_seen = lvd_map_fetch_gen();
  lv_ui_set_refresh(map_refresh);   // pop fetched tiles in as they land (cleared on nav away)
  map_redraw();
}
