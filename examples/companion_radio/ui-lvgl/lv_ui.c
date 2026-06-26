#include "lv_ui.h"

lv_nav_fn lv_nav_cb = NULL;

static lv_refresh_fn g_refresh = NULL;
void          lv_ui_set_refresh(lv_refresh_fn fn) { g_refresh = fn; }
lv_refresh_fn lv_ui_get_refresh(void) { return g_refresh; }

static void nav_event(lv_event_t* e) {
  const char* dest = (const char*)lv_event_get_user_data(e);
  if (lv_nav_cb && dest) lv_nav_cb(dest);
}

// Physical-keyboard focus group: the keypad input device (created in UITask)
// routes T-Deck key presses to whatever object is focused here. A text screen
// calls lv_ui_kbd_focus() on its textarea so physical typing lands in it.
static lv_group_t* g_kbd_group = NULL;
lv_group_t* lv_ui_kbd_group(void) {
  if (!g_kbd_group) g_kbd_group = lv_group_create();
  return g_kbd_group;
}
void lv_ui_kbd_focus(lv_obj_t* ta) {
  lv_group_t* g = lv_ui_kbd_group();
  lv_group_add_obj(g, ta);
  lv_group_focus_obj(ta);
}

// shared RF-plot max (home card + scope use the same scale); default -105 dBm
static int g_rf_max = -105;
int  lv_ui_rf_max(void) { return g_rf_max; }
void lv_ui_rf_set_max(int dbm) { g_rf_max = dbm; }

// give a tappable object an obvious pressed look (dim + sink) so every tap is
// visibly acknowledged, even when the action itself produces no screen change.
void lv_ui_press_fx(lv_obj_t* o) {
  lv_obj_set_style_opa(o, 150, LV_STATE_PRESSED);
  lv_obj_set_style_translate_y(o, 1, LV_STATE_PRESSED);
  lv_obj_set_style_transform_width(o, -2, LV_STATE_PRESSED);
  lv_obj_set_style_transform_height(o, -2, LV_STATE_PRESSED);
}

void lv_ui_clickable(lv_obj_t* o, const char* dest) {
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(o, nav_event, LV_EVENT_CLICKED, (void*)dest);
  lv_ui_press_fx(o);
}

// transient toast, auto-removes after ~1.4s. Parented to the display top layer
// (not the active screen) so a screen rebuild/clean or nav can't free it out
// from under the timer; the validity check is a belt-and-braces guard.
static void toast_del_cb(lv_timer_t* t) {
  lv_obj_t* o = (lv_obj_t*)lv_timer_get_user_data(t);
  if (o && lv_obj_is_valid(o)) lv_obj_del(o);
  lv_timer_del(t);
}
void lv_ui_toast(const char* msg) {
  lv_obj_t* top = lv_layer_top();
  if (!top || !msg) return;
  lv_obj_t* t = lv_label_create(top);
  lv_label_set_text(t, msg);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(t, lv_color_hex(0xffffff), 0);
  lv_obj_set_style_bg_color(t, lv_color_hex(MD_SURFACE), 0);
  lv_obj_set_style_bg_opa(t, 245, 0);
  lv_obj_set_style_radius(t, 10, 0);
  lv_obj_set_style_pad_hor(t, 14, 0);
  lv_obj_set_style_pad_ver(t, 9, 0);
  lv_obj_set_style_border_color(t, lv_color_hex(MD_PRIMARY), 0);
  lv_obj_set_style_border_opa(t, 160, 0);
  lv_obj_set_style_border_width(t, 1, 0);
  lv_obj_set_style_shadow_width(t, 16, 0);
  lv_obj_set_style_shadow_color(t, lv_color_hex(0x000000), 0);
  lv_obj_set_style_shadow_opa(t, 120, 0);
  lv_obj_align(t, LV_ALIGN_BOTTOM_MID, 0, -14);
  lv_obj_move_foreground(t);
  lv_timer_create(toast_del_cb, 1400, t);
}

void lv_ui_screen_bg(lv_obj_t* scr) {
  lv_obj_set_style_bg_color(scr, lv_color_hex(0x0a0e14), 0);
  lv_obj_set_style_bg_grad_color(scr, lv_color_hex(0x000000), 0);
  lv_obj_set_style_bg_grad_dir(scr, LV_GRAD_DIR_VER, 0);
  lv_obj_remove_flag(scr, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_style_pad_all(scr, 0, 0);
}

lv_obj_t* lv_ui_card(lv_obj_t* parent, int x, int y, int w, int h) {
  lv_obj_t* c = lv_obj_create(parent);
  lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  if (w > 0) lv_obj_set_size(c, w, h);
  if (x >= 0) lv_obj_set_pos(c, x, y);
  lv_obj_set_style_radius(c, 16, 0);
  lv_obj_set_style_bg_color(c, lv_color_hex(UI_CARD), 0);
  lv_obj_set_style_bg_opa(c, 18, 0);
  lv_obj_set_style_border_color(c, lv_color_hex(UI_CARD), 0);
  lv_obj_set_style_border_opa(c, 28, 0);
  lv_obj_set_style_border_width(c, 1, 0);
  lv_obj_set_style_shadow_width(c, 8, 0);
  lv_obj_set_style_shadow_color(c, lv_color_hex(0x000000), 0);
  lv_obj_set_style_shadow_opa(c, 80, 0);
  lv_obj_set_style_pad_all(c, 6, 0);
  return c;
}

// --- Material (Android-style) panel kit -------------------------------------
lv_obj_t* lv_ui_md_topbar(lv_obj_t* scr, const char* title) {
  lv_obj_t* bar = lv_obj_create(scr);
  lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(bar, 320, 34);
  lv_obj_set_pos(bar, 0, 0);
  lv_obj_set_style_bg_color(bar, lv_color_hex(MD_SURFACE), 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_style_radius(bar, 0, 0);
  lv_obj_set_style_pad_all(bar, 0, 0);
  // flat M3 app bar: a faint hairline at the bottom, no colour accent
  lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_color(bar, lv_color_hex(MD_ON), 0);
  lv_obj_set_style_border_opa(bar, 20, 0);
  lv_obj_set_style_border_width(bar, 1, 0);

  lv_obj_t* back = lv_obj_create(bar);
  lv_obj_remove_flag(back, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(back, 34, 34);
  lv_obj_align(back, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_set_style_bg_opa(back, 0, 0);
  lv_obj_set_style_border_width(back, 0, 0);
  lv_obj_set_style_pad_all(back, 0, 0);
  lv_obj_t* bl = lv_label_create(back);
  lv_label_set_text(bl, LV_SYMBOL_LEFT);
  lv_obj_set_style_text_color(bl, lv_color_hex(MD_ON), 0);
  lv_obj_center(bl);
  lv_ui_clickable(back, "back");

  lv_obj_t* t = lv_label_create(bar);
  lv_label_set_text(t, title);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(t, lv_color_hex(MD_ON), 0);
  lv_obj_align(t, LV_ALIGN_LEFT_MID, 36, 0);
  return bar;
}

lv_obj_t* lv_ui_md_scroll(lv_obj_t* scr) {
  lv_obj_t* list = lv_obj_create(scr);
  lv_obj_set_pos(list, 0, 34);
  lv_obj_set_size(list, 320, 240 - 34);
  lv_obj_set_style_bg_opa(list, 0, 0);
  lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_style_pad_all(list, 12, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_style_pad_row(list, 10, 0);
  return list;
}

lv_obj_t* lv_ui_md_card(lv_obj_t* parent) {
  lv_obj_t* c = lv_obj_create(parent);
  lv_obj_remove_flag(c, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_width(c, lv_pct(100));
  lv_obj_set_height(c, LV_SIZE_CONTENT);
  lv_obj_set_style_min_height(c, 0, 0);
  lv_obj_set_style_radius(c, 12, 0);
  lv_obj_set_style_bg_color(c, lv_color_hex(MD_SURFACE), 0);
  lv_obj_set_style_bg_opa(c, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(c, 0, 0);
  lv_obj_set_style_shadow_width(c, 0, 0);
  lv_obj_set_style_pad_all(c, 14, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(c, 6, 0);
  return c;
}

lv_obj_t* lv_ui_md_section(lv_obj_t* parent, const char* title, uint32_t accent) {
  lv_obj_t* c = lv_ui_md_card(parent);
  lv_obj_t* h = lv_label_create(c);
  lv_label_set_text(h, title);
  lv_obj_set_style_text_font(h, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(h, lv_color_hex(accent ? accent : MD_ON), 0);
  lv_obj_set_style_pad_bottom(h, 2, 0);
  return c;
}

void lv_ui_md_row(lv_obj_t* card, const char* label, const char* value, uint32_t value_color) {
  lv_ui_md_row_v(card, label, value, value_color);
}

lv_obj_t* lv_ui_md_row_v(lv_obj_t* card, const char* label, const char* value, uint32_t value_color) {
  lv_obj_t* row = lv_obj_create(card);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_width(row, lv_pct(100));
  lv_obj_set_height(row, LV_SIZE_CONTENT);
  lv_obj_set_style_min_height(row, 0, 0);
  lv_obj_set_style_bg_opa(row, 0, 0);
  lv_obj_set_style_border_width(row, 0, 0);
  lv_obj_set_style_pad_all(row, 0, 0);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_SPACE_BETWEEN, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_t* k = lv_label_create(row);
  lv_label_set_text(k, label);
  lv_obj_set_style_text_font(k, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(k, lv_color_hex(MD_MUTED), 0);
  lv_obj_t* v = lv_label_create(row);
  lv_label_set_text(v, value);
  lv_obj_set_style_text_font(v, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(v, lv_color_hex(value_color ? value_color : MD_ON), 0);
  return v;
}

lv_obj_t* lv_ui_pill(lv_obj_t* parent, const char* text, uint32_t color) {
  lv_obj_t* p = lv_obj_create(parent);
  lv_obj_remove_flag(p, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(p, LV_OBJ_FLAG_CLICKABLE);  // decorative
  lv_obj_set_style_radius(p, LV_RADIUS_CIRCLE, 0);
  lv_obj_set_style_bg_color(p, lv_color_hex(color), 0);
  lv_obj_set_style_bg_opa(p, 56, 0);
  lv_obj_set_style_border_color(p, lv_color_hex(color), 0);
  lv_obj_set_style_border_opa(p, 180, 0);
  lv_obj_set_style_border_width(p, 1, 0);
  lv_obj_set_style_pad_hor(p, 8, 0);
  lv_obj_set_style_pad_ver(p, 2, 0);
  lv_obj_t* l = lv_label_create(p);
  lv_label_set_text(l, text);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(0xffffff), 0);
  lv_obj_center(l);
  lv_obj_set_size(p, LV_SIZE_CONTENT, 20);
  return p;
}

lv_obj_t* lv_ui_chip(lv_obj_t* parent, uint32_t color, const char* icon, int size, bool enabled) {
  lv_obj_t* chip = lv_obj_create(parent);
  lv_obj_remove_flag(chip, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_remove_flag(chip, LV_OBJ_FLAG_CLICKABLE);  // decorative: let taps reach the row
  lv_obj_set_size(chip, size, size);
  lv_obj_set_style_radius(chip, LV_RADIUS_CIRCLE, 0);
  uint32_t cc = enabled ? color : 0x39414f;
  lv_obj_set_style_bg_color(chip, lv_color_hex(cc), 0);
  lv_obj_set_style_bg_grad_color(chip, lv_color_darken(lv_color_hex(cc), 90), 0);
  lv_obj_set_style_bg_grad_dir(chip, LV_GRAD_DIR_VER, 0);
  lv_obj_set_style_border_width(chip, 0, 0);
  lv_obj_set_style_pad_all(chip, 0, 0);
  lv_obj_set_style_shadow_color(chip, lv_color_hex(cc), 0);
  lv_obj_set_style_shadow_opa(chip, enabled ? 130 : 0, 0);
  lv_obj_set_style_shadow_width(chip, enabled ? 14 : 0, 0);
  lv_obj_t* g = lv_label_create(chip);
  lv_label_set_text(g, icon);
  lv_obj_set_style_text_font(g, &icons_fa, 0);
  lv_obj_set_style_text_color(g, lv_color_hex(enabled ? 0xffffff : 0x9aa3b2), 0);
  lv_obj_center(g);
  return chip;
}

lv_obj_t* lv_ui_topbar(lv_obj_t* scr, const char* title, uint32_t accent, lv_obj_t** back_btn) {
  lv_obj_t* bar = lv_obj_create(scr);
  lv_obj_remove_flag(bar, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(bar, 320, 30);
  lv_obj_set_pos(bar, 0, 0);
  lv_obj_set_style_bg_color(bar, lv_color_hex(0x0e141d), 0);
  lv_obj_set_style_bg_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(bar, 0, 0);
  lv_obj_set_style_radius(bar, 0, 0);
  lv_obj_set_style_pad_all(bar, 0, 0);
  // accent underline
  lv_obj_set_style_border_side(bar, LV_BORDER_SIDE_BOTTOM, 0);
  lv_obj_set_style_border_color(bar, lv_color_hex(accent), 0);
  lv_obj_set_style_border_opa(bar, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(bar, 2, 0);

  // larger invisible hit area for the back chevron
  lv_obj_t* back = lv_obj_create(bar);
  lv_obj_remove_flag(back, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(back, 30, 30);
  lv_obj_align(back, LV_ALIGN_LEFT_MID, 0, 0);
  lv_obj_set_style_bg_opa(back, 0, 0);
  lv_obj_set_style_border_width(back, 0, 0);
  lv_obj_set_style_pad_all(back, 0, 0);
  lv_obj_t* bl = lv_label_create(back);
  lv_label_set_text(bl, LV_SYMBOL_LEFT);
  lv_obj_set_style_text_color(bl, lv_color_hex(accent), 0);
  lv_obj_center(bl);
  lv_ui_clickable(back, "back");
  if (back_btn) *back_btn = back;

  lv_obj_t* t = lv_label_create(bar);
  lv_label_set_text(t, title);
  lv_obj_set_style_text_font(t, &lv_font_montserrat_18, 0);
  lv_obj_set_style_text_color(t, lv_color_hex(UI_TEXT), 0);
  lv_obj_align(t, LV_ALIGN_LEFT_MID, 30, 0);
  return bar;
}
