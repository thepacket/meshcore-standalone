// LVGL Discover results: announces this node (zero-hop self-advert) on entry,
// then lists recently-heard nodes not yet saved as contacts. Tap a row to add it
// to contacts (it then moves to the Contacts screen). Bound to the firmware via
// the lvd_disc_* bridge (getHeardCandidates / addHeardContact).
#include "lv_ui.h"
#include "lv_data.h"
#include <stdio.h>

static lv_obj_t* s_list = NULL;
static int       s_last = -1;

static void disc_fill(lv_obj_t* list);   // fwd

static uint32_t type_color(int t) {
  return t == 2 ? UI_PURPLE : t == 3 ? UI_CYAN : t == 4 ? UI_ORANGE : UI_BLUE;
}
static const char* type_icon(int t) {
  return t == 2 ? ICON_REPEATERS : t == 3 ? ICON_CHAT : t == 4 ? ICON_NOISE : ICON_CONTACTS;
}

static void add_clicked(lv_event_t* e) {
  int i = (int)(intptr_t)lv_event_get_user_data(e);
  lvd_disc_add(i);
  if (s_list) { lv_obj_clean(s_list); disc_fill(s_list); }   // it leaves the candidate list
}

static void disc_row(lv_obj_t* list, const lvd_disc_t* d, int idx) {
  lv_obj_t* row = lv_ui_md_card(list);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_add_event_cb(row, add_clicked, LV_EVENT_CLICKED, (void*)(intptr_t)idx);

  lv_obj_t* ic = lv_label_create(row);
  lv_label_set_text(ic, type_icon(d->type));
  lv_obj_set_style_text_font(ic, &icons_fa, 0);
  lv_obj_set_style_text_color(ic, lv_color_hex(type_color(d->type)), 0);
  lv_obj_set_style_margin_right(ic, 14, 0);

  lv_obj_t* mid = lv_obj_create(row);
  lv_obj_remove_flag(mid, LV_OBJ_FLAG_SCROLLABLE); lv_obj_remove_flag(mid, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(mid, 0, 0); lv_obj_set_style_border_width(mid, 0, 0);
  lv_obj_set_style_pad_all(mid, 0, 0); lv_obj_set_flex_grow(mid, 1); lv_obj_set_height(mid, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(mid, 2, 0);
  lv_obj_t* nm = lv_label_create(mid);
  lv_label_set_text(nm, d->name);
  lv_obj_set_style_text_font(nm, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(nm, lv_color_hex(MD_ON), 0);
  lv_obj_t* sb = lv_label_create(mid);
  lv_label_set_text(sb, d->subtitle);
  lv_obj_set_style_text_font(sb, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(sb, lv_color_hex(MD_MUTED), 0);

  lv_obj_t* plus = lv_label_create(row);
  lv_label_set_text(plus, LV_SYMBOL_PLUS);
  lv_obj_set_style_text_color(plus, lv_color_hex(MD_PRIMARY), 0);
}

static void disc_fill(lv_obj_t* list) {
  int n = lvd_disc_count();
  s_last = n;
  if (n <= 0) {
    lv_obj_t* hint = lv_label_create(list);
    lv_label_set_text(hint, "No new nodes heard\n(everything heard is already a contact)");
    lv_obj_set_style_text_color(hint, lv_color_hex(MD_MUTED), 0);
    return;
  }
  lvd_disc_t d;
  for (int i = 0; i < n; i++) if (lvd_disc_get(i, &d)) disc_row(list, &d, i);
}

static void disc_tick(void) {
  if (s_list && lvd_disc_count() != s_last) { lv_obj_clean(s_list); disc_fill(s_list); }
}

void lv_disc_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Discover");
  lvd_disc_announce();              // announce ourselves so neighbours respond
  s_list = lv_ui_md_scroll(scr);
  disc_fill(s_list);
  lv_ui_set_refresh(disc_tick);
}
