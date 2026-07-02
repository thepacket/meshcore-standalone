// LVGL microSD file browser: navigate directories, see sizes + card usage, and
// delete files. Bound to the firmware via lvd_sd_* (which drive the target's
// shared-SPI SD helpers and re-claim the radio bus after each access).
#include "lv_ui.h"
#include "lv_data.h"
#include <stdio.h>
#include <string.h>

#define SD_MAX 64
static char       s_path[256] = "/";
static lv_obj_t*  s_list = NULL;
static lv_obj_t*  s_usage = NULL;
static void files_fill(void);

// join the current path with a child directory name (bounded)
static void path_cd(const char* name) {
  int len = strlen(s_path);
  if (strcmp(s_path, "/") == 0) snprintf(s_path, sizeof(s_path), "/%s", name);
  else if (len + 1 + (int)strlen(name) < (int)sizeof(s_path)) {
    strcat(s_path, "/"); strcat(s_path, name);
  }
}
static void path_up(void) {
  char* slash = strrchr(s_path, '/');
  if (!slash || slash == s_path) { strcpy(s_path, "/"); return; }
  *slash = 0;
}
static void full_path(const char* name, char* out, int len) {
  if (strcmp(s_path, "/") == 0) snprintf(out, len, "/%s", name);
  else                          snprintf(out, len, "%s/%s", s_path, name);
}

// ---- delete confirm --------------------------------------------------------
static char s_del_target[320];
static void del_cancel(lv_event_t* e) { lv_obj_del((lv_obj_t*)lv_event_get_user_data(e)); }
static void del_confirm(lv_event_t* e) {
  lv_obj_del((lv_obj_t*)lv_event_get_user_data(e));
  lv_ui_toast(lvd_sd_remove(s_del_target) ? "Deleted" : "Delete failed");
  files_fill();
}
static void ask_delete(const char* name) {
  full_path(name, s_del_target, sizeof(s_del_target));
  lv_obj_t* m = lv_obj_create(lv_screen_active());
  lv_obj_set_size(m, 320, 240); lv_obj_set_pos(m, 0, 0);
  lv_obj_set_style_bg_color(m, lv_color_hex(0x000000), 0); lv_obj_set_style_bg_opa(m, 160, 0);
  lv_obj_set_style_border_width(m, 0, 0); lv_obj_add_flag(m, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* card = lv_obj_create(m);
  lv_obj_set_size(card, 280, 120); lv_obj_center(card);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x18202c), 0);
  lv_obj_set_style_border_color(card, lv_color_hex(UI_RED), 0);
  lv_obj_set_style_border_width(card, 1, 0); lv_obj_set_style_radius(card, 12, 0);

  lv_obj_t* q = lv_label_create(card);
  lv_label_set_text_fmt(q, "Delete\n%s ?", name);
  lv_label_set_long_mode(q, LV_LABEL_LONG_WRAP); lv_obj_set_width(q, 256);
  lv_obj_set_style_text_color(q, lv_color_hex(UI_TEXT), 0);
  lv_obj_align(q, LV_ALIGN_TOP_MID, 0, 0);

  lv_obj_t* c = lv_button_create(card);
  lv_obj_set_size(c, 110, 34); lv_obj_align(c, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_bg_color(c, lv_color_hex(0x2a3343), 0);
  lv_obj_add_event_cb(c, del_cancel, LV_EVENT_CLICKED, m);
  lv_obj_t* cl = lv_label_create(c); lv_label_set_text(cl, "Cancel"); lv_obj_center(cl);

  lv_obj_t* d = lv_button_create(card);
  lv_obj_set_size(d, 110, 34); lv_obj_align(d, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_set_style_bg_color(d, lv_color_hex(UI_RED), 0);
  lv_obj_add_event_cb(d, del_confirm, LV_EVENT_CLICKED, m);
  lv_obj_t* dl = lv_label_create(d); lv_label_set_text(dl, "Delete"); lv_obj_center(dl);
}

// ---- format confirm (FAT32, destructive) -----------------------------------
static void fmt_cancel(lv_event_t* e) { lv_obj_del((lv_obj_t*)lv_event_get_user_data(e)); }
static void fmt_confirm(lv_event_t* e) {
  lv_obj_del((lv_obj_t*)lv_event_get_user_data(e));
  lv_ui_toast("Formatting...");
  int ok = lvd_sd_format();
  lv_ui_toast(ok ? "Formatted FAT32" : "Format failed");
  strcpy(s_path, "/");
  files_fill();
}
static void ask_format(lv_event_t* e) {
  (void)e;
  lv_obj_t* m = lv_obj_create(lv_screen_active());
  lv_obj_set_size(m, 320, 240); lv_obj_set_pos(m, 0, 0);
  lv_obj_set_style_bg_color(m, lv_color_hex(0x000000), 0); lv_obj_set_style_bg_opa(m, 170, 0);
  lv_obj_set_style_border_width(m, 0, 0); lv_obj_add_flag(m, LV_OBJ_FLAG_CLICKABLE);

  lv_obj_t* card = lv_obj_create(m);
  lv_obj_set_size(card, 290, 138); lv_obj_center(card);
  lv_obj_set_style_bg_color(card, lv_color_hex(0x18202c), 0);
  lv_obj_set_style_border_color(card, lv_color_hex(UI_RED), 0);
  lv_obj_set_style_border_width(card, 1, 0); lv_obj_set_style_radius(card, 12, 0);

  lv_obj_t* q = lv_label_create(card);
  lv_label_set_text(q, "Format card as FAT32?\nThis ERASES everything on the microSD.");
  lv_label_set_long_mode(q, LV_LABEL_LONG_WRAP); lv_obj_set_width(q, 266);
  lv_obj_set_style_text_color(q, lv_color_hex(UI_TEXT), 0);
  lv_obj_align(q, LV_ALIGN_TOP_MID, 0, 0);

  lv_obj_t* c = lv_button_create(card);
  lv_obj_set_size(c, 116, 36); lv_obj_align(c, LV_ALIGN_BOTTOM_LEFT, 0, 0);
  lv_obj_set_style_bg_color(c, lv_color_hex(0x2a3343), 0);
  lv_obj_add_event_cb(c, fmt_cancel, LV_EVENT_CLICKED, m);
  lv_obj_t* cl = lv_label_create(c); lv_label_set_text(cl, "Cancel"); lv_obj_center(cl);

  lv_obj_t* f = lv_button_create(card);
  lv_obj_set_size(f, 116, 36); lv_obj_align(f, LV_ALIGN_BOTTOM_RIGHT, 0, 0);
  lv_obj_set_style_bg_color(f, lv_color_hex(UI_RED), 0);
  lv_obj_add_event_cb(f, fmt_confirm, LV_EVENT_CLICKED, m);
  lv_obj_t* fl = lv_label_create(f); lv_label_set_text(fl, "Format"); lv_obj_center(fl);
}

// ---- rows ------------------------------------------------------------------
// snapshot for row callbacks; ~4.6KB, so it lives on the lv_malloc heap
// (PSRAM on device) instead of static DRAM
static lvd_sd_t* s_ent = NULL;
static int       s_ent_n = 0;

static void row_clicked(lv_event_t* e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  if (i < 0 || i >= s_ent_n || !s_ent[i].is_dir) return;   // only dirs navigate
  path_cd(s_ent[i].name);
  files_fill();
}
static void row_long(lv_event_t* e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  if (i >= 0 && i < s_ent_n && !s_ent[i].is_dir) ask_delete(s_ent[i].name);   // delete files only
}
static void up_clicked(lv_event_t* e) { (void)e; path_up(); files_fill(); }

static void fmt_size(uint32_t b, char* out, int len) {
  if (b >= 1048576) snprintf(out, len, "%.1f MB", b / 1048576.0);
  else if (b >= 1024) snprintf(out, len, "%.1f KB", b / 1024.0);
  else snprintf(out, len, "%u B", (unsigned)b);
}

static void file_row(int i) {
  const lvd_sd_t* en = &s_ent[i];
  lv_obj_t* row = lv_ui_md_card(s_list);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(row, row_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)i);
  lv_obj_add_event_cb(row, row_long, LV_EVENT_LONG_PRESSED, (void*)(intptr_t)i);
  lv_ui_press_fx(row);

  lv_obj_t* ic = lv_label_create(row);
  lv_label_set_text(ic, en->is_dir ? LV_SYMBOL_DIRECTORY : LV_SYMBOL_FILE);
  lv_obj_set_style_text_color(ic, lv_color_hex(en->is_dir ? UI_AMBER : MD_MUTED), 0);
  lv_obj_set_style_margin_right(ic, 12, 0);

  lv_obj_t* nm = lv_label_create(row);
  lv_label_set_text(nm, en->name);
  lv_obj_set_style_text_font(nm, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(nm, lv_color_hex(MD_ON), 0);
  lv_obj_set_flex_grow(nm, 1);

  if (!en->is_dir) {
    char sz[16]; fmt_size(en->size, sz, sizeof(sz));
    lv_obj_t* s = lv_label_create(row);
    lv_label_set_text(s, sz);
    lv_obj_set_style_text_font(s, &lv_font_montserrat_12, 0);
    lv_obj_set_style_text_color(s, lv_color_hex(MD_MUTED), 0);
  }
}

static void files_fill(void) {
  if (!s_list) return;
  lv_obj_clean(s_list);
  if (s_usage) { char u[32]; lvd_sd_usage(u, sizeof(u)); lv_label_set_text(s_usage, u); }

  if (!s_ent) {
    s_ent = (lvd_sd_t*)lv_malloc(sizeof(lvd_sd_t) * SD_MAX);
    if (!s_ent) { s_ent_n = 0; return; }
  }
  s_ent_n = lvd_sd_list(s_path, s_ent, SD_MAX);
  if (s_ent_n < 0) {
    lv_obj_t* h = lv_label_create(s_list);
    lv_label_set_text(h, "No SD card detected.\nInsert a FAT32 card and re-open.");
    lv_label_set_long_mode(h, LV_LABEL_LONG_WRAP); lv_obj_set_width(h, lv_pct(100));
    lv_obj_set_style_text_color(h, lv_color_hex(MD_MUTED), 0);
    return;
  }
  if (s_ent_n == 0) {
    lv_obj_t* h = lv_label_create(s_list);
    lv_label_set_text(h, "(empty folder)");
    lv_obj_set_style_text_color(h, lv_color_hex(MD_MUTED), 0);
    return;
  }
  for (int pass = 0; pass < 2; pass++)        // directories first, then files
    for (int i = 0; i < s_ent_n; i++)
      if ((pass == 0) == (s_ent[i].is_dir != 0)) file_row(i);
}

void lv_files_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  strcpy(s_path, "/");
  lv_ui_md_topbar(scr, "Files");

  // header: up button + current path + card usage
  lv_obj_t* up = lv_ui_card(scr, 8, 38, 44, 28);
  lv_obj_set_style_pad_all(up, 0, 0); lv_obj_set_style_radius(up, 8, 0);
  lv_obj_set_style_bg_color(up, lv_color_hex(MD_PRIMARY), 0); lv_obj_set_style_bg_opa(up, 40, 0);
  lv_obj_add_flag(up, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(up, up_clicked, LV_EVENT_CLICKED, NULL);
  lv_ui_press_fx(up);
  lv_obj_t* ul = lv_label_create(up); lv_label_set_text(ul, LV_SYMBOL_UP); lv_obj_center(ul);

  s_usage = lv_label_create(scr);
  lv_label_set_text(s_usage, "");
  lv_obj_set_style_text_font(s_usage, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(s_usage, lv_color_hex(MD_MUTED), 0);
  lv_obj_set_pos(s_usage, 60, 46);

  // Format (FAT32) button, top-right
  lv_obj_t* fb = lv_ui_card(scr, 320 - 8 - 84, 38, 84, 28);
  lv_obj_set_style_pad_all(fb, 0, 0); lv_obj_set_style_radius(fb, 8, 0);
  lv_obj_set_style_bg_color(fb, lv_color_hex(UI_RED), 0); lv_obj_set_style_bg_opa(fb, 40, 0);
  lv_obj_set_style_border_color(fb, lv_color_hex(UI_RED), 0); lv_obj_set_style_border_opa(fb, 200, 0);
  lv_obj_set_style_border_width(fb, 1, 0);
  lv_obj_add_flag(fb, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(fb, ask_format, LV_EVENT_CLICKED, NULL);
  lv_ui_press_fx(fb);
  lv_obj_t* fbl = lv_label_create(fb); lv_label_set_text(fbl, "Format");
  lv_obj_set_style_text_font(fbl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(fbl, lv_color_hex(0xffffff), 0); lv_obj_center(fbl);

  s_list = lv_obj_create(scr);
  lv_obj_set_pos(s_list, 4, 72); lv_obj_set_size(s_list, 320 - 8, 240 - 72 - 4);
  lv_obj_set_style_bg_opa(s_list, 0, 0); lv_obj_set_style_border_width(s_list, 0, 0);
  lv_obj_set_style_pad_all(s_list, 2, 0);
  lv_obj_set_flex_flow(s_list, LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_row(s_list, 6, 0);
  files_fill();
}
