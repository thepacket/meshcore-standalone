// Headless LVGL prototype harness: renders a screen into an in-memory buffer
// and writes it to a PPM (no SDL needed), so we can screenshot the LVGL UI.
#include "lvgl.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#define W 320
#define H 240

static uint8_t fb[W * H * 3];   // RGB framebuffer for the PPM

static uint32_t millis_cb(void) {
  struct timespec ts; clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint32_t)(ts.tv_sec * 1000 + ts.tv_nsec / 1000000);
}

// LVGL flushes ARGB8888 (little-endian: B,G,R,A) regions; copy into fb as RGB.
static void flush_cb(lv_display_t* disp, const lv_area_t* area, uint8_t* px_map) {
  for (int y = area->y1; y <= area->y2; y++) {
    for (int x = area->x1; x <= area->x2; x++) {
      const uint8_t* p = px_map + (((y - area->y1) * (area->x2 - area->x1 + 1)) + (x - area->x1)) * 4;
      uint8_t* o = fb + (y * W + x) * 3;
      o[0] = p[2]; o[1] = p[1]; o[2] = p[0];   // R,G,B
    }
  }
  lv_display_flush_ready(disp);
}

void lv_home_create(lv_obj_t* scr);
void lv_chat_list_create(lv_obj_t* scr);
void lv_chat_conv_create(lv_obj_t* scr);

int main(int argc, char** argv) {
  const char* screen = argc > 1 ? argv[1] : "home";
  const char* out = argc > 2 ? argv[2] : "sim-lvgl/shots/home.ppm";

  lv_init();
  lv_tick_set_cb(millis_cb);

  lv_display_t* disp = lv_display_create(W, H);
  static uint8_t draw_buf[W * H * 4];
  lv_display_set_buffers(disp, draw_buf, NULL, sizeof(draw_buf), LV_DISPLAY_RENDER_MODE_PARTIAL);
  lv_display_set_flush_cb(disp, flush_cb);

  lv_obj_t* s = lv_screen_active();
  if (!strcmp(screen, "chatlist")) lv_chat_list_create(s);
  else if (!strcmp(screen, "chat")) lv_chat_conv_create(s);
  else lv_home_create(s);

  // let LVGL lay out + render a couple of frames, then force a full refresh
  for (int i = 0; i < 3; i++) { lv_timer_handler(); }
  lv_refr_now(disp);

  FILE* f = fopen(out, "wb");
  if (!f) { fprintf(stderr, "cannot open %s\n", out); return 1; }
  fprintf(f, "P6\n%d %d\n255\n", W, H);
  fwrite(fb, 1, sizeof(fb), f);
  fclose(f);
  printf("wrote %s\n", out);
  return 0;
}
