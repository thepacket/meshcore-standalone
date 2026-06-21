#include "lv_ui.h"

lv_nav_fn lv_nav_cb = NULL;

static void nav_event(lv_event_t* e) {
  const char* dest = (const char*)lv_event_get_user_data(e);
  if (lv_nav_cb && dest) lv_nav_cb(dest);
}

void lv_ui_clickable(lv_obj_t* o, const char* dest) {
  lv_obj_add_flag(o, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(o, nav_event, LV_EVENT_CLICKED, (void*)dest);
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
