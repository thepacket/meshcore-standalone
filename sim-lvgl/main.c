// LVGL prototype harness.
//   ./lvglsim                      -> live interactive window (mouse = touch)
//   ./lvglsim <screen> <out.ppm>   -> headless: render one screen to a PPM
#include "lvgl.h"
#include "lv_ui.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

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
void lv_repeater_detail_create(lv_obj_t* scr);

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
  else if (!strcmp(name, "repeaters")) lv_repeaters_create(s);
  else if (!strcmp(name, "repeater_detail")) lv_repeater_detail_create(s);
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
