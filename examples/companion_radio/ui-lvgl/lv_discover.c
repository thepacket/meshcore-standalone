// LVGL Discover / Tools hub: mirrors the Android client's Tools tab (ToolRow
// cards — leading icon, title, subtitle). Each row fires a zero-hop self-advert
// filtered to a contact type to find one-hop neighbours (companions / repeaters
// / rooms / sensors); newly heard nodes are auto-added to contacts. A Trace path
// entry routes to the trace tool. The firmware build wires each row to MyMesh
// sendSelfAdvert()/discovery callbacks.
#include "lv_ui.h"
#include <stdio.h>

typedef struct {
  const char* icon; const char* title; const char* sub;
  const char* dest;   // dest != NULL -> navigates instead of "discover"
} Tool;

static const Tool TOOLS[] = {
  {ICON_TRACE,     "Trace path",          "Trace a path through chosen repeaters; see each hop's SNR.", "trace"},
  {ICON_ADVERT,    "Discover nodes",      "Announce, then list nearby nodes not yet in contacts; tap to add.", "disc"},
};
#define N_TOOLS ((int)(sizeof(TOOLS)/sizeof(TOOLS[0])))

static void tool_row(lv_obj_t* list, const Tool* t) {
  lv_obj_t* row = lv_ui_md_card(list);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_ui_clickable(row, t->dest ? t->dest : "discover");

  lv_obj_t* ic = lv_label_create(row);
  lv_label_set_text(ic, t->icon);
  lv_obj_set_style_text_font(ic, &icons_fa, 0);
  lv_obj_set_style_text_color(ic, lv_color_hex(MD_PRIMARY), 0);
  lv_obj_set_style_margin_right(ic, 14, 0);

  lv_obj_t* mid = lv_obj_create(row);
  lv_obj_remove_flag(mid, LV_OBJ_FLAG_SCROLLABLE); lv_obj_remove_flag(mid, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_bg_opa(mid, 0, 0); lv_obj_set_style_border_width(mid, 0, 0);
  lv_obj_set_style_pad_all(mid, 0, 0); lv_obj_set_flex_grow(mid, 1); lv_obj_set_height(mid, LV_SIZE_CONTENT);
  lv_obj_set_flex_flow(mid, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(mid, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_set_style_pad_row(mid, 2, 0);
  lv_obj_t* nm = lv_label_create(mid);
  lv_label_set_text(nm, t->title);
  lv_obj_set_style_text_font(nm, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(nm, lv_color_hex(MD_ON), 0);
  lv_obj_t* sb = lv_label_create(mid);
  lv_label_set_text(sb, t->sub);
  lv_label_set_long_mode(sb, LV_LABEL_LONG_WRAP);
  lv_obj_set_width(sb, lv_pct(100));
  lv_obj_set_style_text_font(sb, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(sb, lv_color_hex(MD_MUTED), 0);
}

void lv_discover_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Tools");
  lv_obj_t* list = lv_ui_md_scroll(scr);
  for (int i = 0; i < N_TOOLS; i++) tool_row(list, &TOOLS[i]);
}
