// LVGL Discover / Tools hub: mirrors the Android client's Tools tab (ToolRow
// cards — leading icon, title, subtitle). Each row fires a zero-hop self-advert
// filtered to a contact type to find one-hop neighbours (companions / repeaters
// / rooms / sensors); newly heard nodes are auto-added to contacts. A Trace path
// entry routes to the trace tool. The firmware build wires each row to MyMesh
// sendSelfAdvert()/discovery callbacks.
#include "lv_ui.h"
#include "lv_data.h"
#include <stdio.h>

typedef struct {
  const char* icon; const char* title; const char* sub;
  const char* dest;        // navigate here on tap (NULL if this row is an action)
  void (*action)(void);    // or fire this instead of navigating (NULL if dest row)
  const char* toast;       // confirmation shown after an action
} Tool;

// announce ourselves; zero-hop reaches direct neighbours, flood relays mesh-wide.
static void act_advert_zero(void)  { lvd_disc_announce(); }
static void act_advert_flood(void) { lvd_disc_announce_flood(); }

static const Tool TOOLS[] = {
  // Trace path lives on the home grid (amber TRACE tile); not duplicated here.
  {ICON_ADVERT, "Discover nodes",        "Announce, then list nearby nodes not yet in contacts; tap to add.", "disc", NULL, NULL},
  {ICON_ADVERT, "Advert - Zero Hop",     "Advertise this node to direct neighbours only (one hop).",          NULL, act_advert_zero,  "Zero-hop advert sent"},
  {ICON_ADVERT, "Advert - Flood Routed", "Advertise this node mesh-wide; relayed across repeaters.",           NULL, act_advert_flood, "Flood advert sent"},
};
#define N_TOOLS ((int)(sizeof(TOOLS)/sizeof(TOOLS[0])))

static void tool_action_cb(lv_event_t* e) {
  const Tool* t = (const Tool*)lv_event_get_user_data(e);
  if (t && t->action) t->action();
  if (t && t->toast) lv_ui_toast(t->toast);
}

static void tool_row(lv_obj_t* list, const Tool* t) {
  lv_obj_t* row = lv_ui_md_card(list);
  lv_obj_set_flex_flow(row, LV_FLEX_FLOW_ROW);
  lv_obj_set_flex_align(row, LV_FLEX_ALIGN_START, LV_FLEX_ALIGN_CENTER, LV_FLEX_ALIGN_CENTER);
  if (t->action) {   // action row: fire the callback + toast instead of navigating
    lv_obj_add_flag(row, LV_OBJ_FLAG_CLICKABLE);
    lv_obj_add_event_cb(row, tool_action_cb, LV_EVENT_CLICKED, (void*)t);
    lv_ui_press_fx(row);
  } else {
    lv_ui_clickable(row, t->dest ? t->dest : "discover");
  }

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
