// LVGL repeaters: Saved/Scan list + detail with an arc battery gauge,
// action buttons, and stat cards.
#include "lv_ui.h"
#include <stdio.h>

// ---- list -------------------------------------------------------------------
typedef struct { const char* name; const char* type; bool fav; bool scan; } Node;

static void rep_row(lv_obj_t* list, const Node* n) {
  lv_obj_t* row = lv_ui_md_card(list);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  lv_ui_clickable(row, "repeater_detail");

  lv_obj_t* ic = lv_label_create(row);
  lv_label_set_text(ic, ICON_REPEATERS);
  lv_obj_set_style_text_font(ic, &icons_fa, 0);
  lv_obj_set_style_text_color(ic, lv_color_hex(n->scan ? MD_PRIMARY : UI_PURPLE), 0);
  lv_obj_set_style_margin_right(ic, 14, 0);

  lv_obj_t* nm = lv_label_create(row);
  lv_label_set_text(nm, n->name);
  lv_obj_set_flex_grow(nm, 1);
  lv_obj_set_style_text_font(nm, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(nm, lv_color_hex(MD_ON), 0);

  if (n->fav) {
    lv_obj_t* star = lv_label_create(row);
    lv_label_set_text(star, LV_SYMBOL_OK);
    lv_obj_set_style_text_color(star, lv_color_hex(UI_AMBER), 0);
    lv_obj_set_style_margin_right(star, 8, 0);
  }
  lv_obj_t* tp = lv_ui_pill(row, n->scan ? "+ ADD" : n->type, n->scan ? MD_PRIMARY : UI_PURPLE);
  (void)tp;
}

static int g_rep_tab = 0;   // 0 = Saved, 1 = Scan
void lv_repeaters_set_tab(int t) { g_rep_tab = (t == 1) ? 1 : 0; }

static void seg_tab(lv_obj_t* parent, const char* txt, bool active, const char* dest) {
  lv_obj_t* t = lv_obj_create(parent);
  lv_obj_remove_flag(t, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_grow(t, 1); lv_obj_set_height(t, lv_pct(100));
  lv_obj_set_style_radius(t, 8, 0);
  lv_obj_set_style_bg_color(t, lv_color_hex(MD_PRIMARY), 0);
  lv_obj_set_style_bg_opa(t, active ? 200 : 0, 0);
  lv_obj_set_style_border_width(t, 0, 0); lv_obj_set_style_pad_all(t, 0, 0);
  if (!active && dest) lv_ui_clickable(t, dest);   // tap the other tab to switch
  lv_obj_t* l = lv_label_create(t);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(active ? 0xffffff : UI_MUTED), 0);
  lv_obj_center(l);
}

void lv_repeaters_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "Repeaters");

  lv_obj_t* seg = lv_obj_create(scr);
  lv_obj_remove_flag(seg, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_size(seg, 320 - 24, 28); lv_obj_set_pos(seg, 12, 42);
  lv_obj_set_style_radius(seg, 10, 0);
  lv_obj_set_style_bg_color(seg, lv_color_hex(MD_SURFACE), 0); lv_obj_set_style_bg_opa(seg, LV_OPA_COVER, 0);
  lv_obj_set_style_border_width(seg, 0, 0); lv_obj_set_style_pad_all(seg, 3, 0);
  lv_obj_set_flex_flow(seg, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(seg, 4, 0);
  seg_tab(seg, "Saved", g_rep_tab == 0, "repeaters");
  seg_tab(seg, "Scan",  g_rep_tab == 1, "scan");

  lv_obj_t* list = lv_obj_create(scr);
  lv_obj_set_pos(list, 0, 78); lv_obj_set_size(list, 320, 240 - 78);
  lv_obj_set_style_bg_opa(list, 0, 0); lv_obj_set_style_border_width(list, 0, 0);
  lv_obj_set_style_pad_hor(list, 12, 0); lv_obj_set_style_pad_ver(list, 2, 0);
  lv_obj_set_flex_flow(list, LV_FLEX_FLOW_COLUMN); lv_obj_set_style_pad_row(list, 8, 0);

  if (g_rep_tab == 0) {
    Node nodes[] = {
      {"GW-Hertford", "RPT", true, false},
      {"Hilltop-Relay", "RPT", false, false},
      {"Town Square", "ROOM", false, false},
    };
    for (unsigned i = 0; i < sizeof(nodes)/sizeof(nodes[0]); i++) rep_row(list, &nodes[i]);
  } else {
    Node nodes[] = {                                   // heard but not yet saved
      {"New-Repeater-9", "RPT", false, true},
      {"Field-Node", "RPT", false, true},
    };
    for (unsigned i = 0; i < sizeof(nodes)/sizeof(nodes[0]); i++) rep_row(list, &nodes[i]);
  }
}

// ---- detail -----------------------------------------------------------------
static void action_btn(lv_obj_t* parent, const char* txt, uint32_t color) {
  lv_obj_t* b = lv_obj_create(parent);
  lv_obj_remove_flag(b, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_flex_grow(b, 1); lv_obj_set_height(b, 30);
  lv_obj_set_style_radius(b, 8, 0);
  lv_obj_set_style_bg_color(b, lv_color_hex(color), 0); lv_obj_set_style_bg_opa(b, 48, 0);
  lv_obj_set_style_border_color(b, lv_color_hex(color), 0); lv_obj_set_style_border_opa(b, 200, 0);
  lv_obj_set_style_border_width(b, 1, 0); lv_obj_set_style_pad_all(b, 0, 0);
  lv_obj_t* l = lv_label_create(b);
  lv_label_set_text(l, txt);
  lv_obj_set_style_text_font(l, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(l, lv_color_hex(0xffffff), 0);
  lv_obj_center(l);
}

static void stat_card(lv_obj_t* grid, const char* k, const char* v, uint32_t color) {
  lv_obj_t* c = lv_ui_md_card(grid);
  lv_obj_set_size(c, 100, 44);
  lv_obj_set_style_pad_all(c, 4, 0);
  lv_obj_set_flex_flow(c, LV_FLEX_FLOW_COLUMN);
  lv_obj_set_flex_align(c, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_START);
  lv_obj_t* vl = lv_label_create(c);
  lv_label_set_text(vl, v);
  lv_obj_set_style_text_font(vl, &lv_font_montserrat_16, 0);
  lv_obj_set_style_text_color(vl, lv_color_hex(color), 0);
  lv_obj_t* kl = lv_label_create(c);
  lv_label_set_text(kl, k);
  lv_obj_set_style_text_font(kl, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(kl, lv_color_hex(UI_MUTED), 0);
}

void lv_repeater_detail_create(lv_obj_t* scr) {
  lv_ui_screen_bg(scr);
  lv_ui_md_topbar(scr, "GW-Hertford");

  // action buttons
  lv_obj_t* acts = lv_obj_create(scr);
  lv_obj_remove_flag(acts, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(acts, 6, 36); lv_obj_set_size(acts, 320 - 12, 34);
  lv_obj_set_style_bg_opa(acts, 0, 0); lv_obj_set_style_border_width(acts, 0, 0);
  lv_obj_set_style_pad_all(acts, 0, 0);
  lv_obj_set_flex_flow(acts, LV_FLEX_FLOW_ROW); lv_obj_set_style_pad_column(acts, 5, 0);
  action_btn(acts, "Status", UI_BLUE);
  action_btn(acts, "Login", UI_INDIGO);
  action_btn(acts, "Advert", UI_PINK);
  action_btn(acts, "Sync", UI_AMBER);

  // battery arc gauge
  lv_obj_t* bcard = lv_ui_card(scr, 6, 76, 100, 156 - 76 + 76);
  lv_obj_set_size(bcard, 100, 90);
  lv_obj_t* arc = lv_arc_create(bcard);
  lv_obj_set_size(arc, 74, 74);
  lv_obj_align(arc, LV_ALIGN_TOP_MID, 0, 0);
  lv_arc_set_rotation(arc, 135); lv_arc_set_bg_angles(arc, 0, 270);
  lv_arc_set_range(arc, 0, 100); lv_arc_set_value(arc, 80);
  lv_obj_remove_style(arc, NULL, LV_PART_KNOB);
  lv_obj_remove_flag(arc, LV_OBJ_FLAG_CLICKABLE);
  lv_obj_set_style_arc_color(arc, lv_color_hex(0x2a3343), LV_PART_MAIN);
  lv_obj_set_style_arc_color(arc, lv_color_hex(UI_GREEN), LV_PART_INDICATOR);
  lv_obj_set_style_arc_width(arc, 8, LV_PART_MAIN);
  lv_obj_set_style_arc_width(arc, 8, LV_PART_INDICATOR);
  lv_obj_t* bv = lv_label_create(arc);
  lv_label_set_text(bv, "4.05V");
  lv_obj_set_style_text_font(bv, &lv_font_montserrat_14, 0);
  lv_obj_set_style_text_color(bv, lv_color_hex(UI_TEXT), 0);
  lv_obj_center(bv);
  lv_obj_t* bcap = lv_label_create(bcard);
  lv_label_set_text(bcap, "Battery");
  lv_obj_set_style_text_font(bcap, &lv_font_montserrat_12, 0);
  lv_obj_set_style_text_color(bcap, lv_color_hex(UI_MUTED), 0);
  lv_obj_align(bcap, LV_ALIGN_BOTTOM_MID, 0, 0);

  // stat cards (right column)
  lv_obj_t* grid = lv_obj_create(scr);
  lv_obj_remove_flag(grid, LV_OBJ_FLAG_SCROLLABLE);
  lv_obj_set_pos(grid, 112, 76); lv_obj_set_size(grid, 320 - 112 - 6, 158);
  lv_obj_set_style_bg_opa(grid, 0, 0); lv_obj_set_style_border_width(grid, 0, 0);
  lv_obj_set_style_pad_all(grid, 0, 0);
  lv_obj_set_flex_flow(grid, LV_FLEX_FLOW_ROW_WRAP);
  lv_obj_set_style_pad_row(grid, 6, 0); lv_obj_set_style_pad_column(grid, 6, 0);
  stat_card(grid, "Uptime", "4d 7h", UI_TEXT);
  stat_card(grid, "Packets", "150k", UI_TEXT);
  stat_card(grid, "Airtime tx", "70m", UI_TEXT);
  stat_card(grid, "Last SNR", "7.0 dB", UI_GREEN);
}
