#include "UITask.h"
#include <helpers/TxtDataHelpers.h>
#include "../MyMesh.h"
#include "target.h"
#include <math.h>
#ifdef WIFI_SSID
  #include <WiFi.h>
#endif

#ifndef UI_DEVICE_LABEL
  #define UI_DEVICE_LABEL ""   // shown after the node name in the home bottom bar (e.g. "TDeck+")
#endif
#ifndef NOISE_SAMPLE_MILLIS
  #define NOISE_SAMPLE_MILLIS 250
#endif
#ifndef BATT_MIN_MILLIVOLTS
  #define BATT_MIN_MILLIVOLTS 3000
#endif
#ifndef BATT_MAX_MILLIVOLTS
  #define BATT_MAX_MILLIVOLTS 4200
#endif

#ifndef AUTO_OFF_MILLIS
  #define AUTO_OFF_MILLIS     15000   // 15 seconds
#endif
#define BOOT_SCREEN_MILLIS   3000   // 3 seconds

#ifdef PIN_STATUS_LED
#define LED_ON_MILLIS     20
#define LED_ON_MSG_MILLIS 200
#define LED_CYCLE_MILLIS  4000
#endif

#define LONG_PRESS_MILLIS   1200

#ifndef UI_RECENT_LIST_SIZE
  #define UI_RECENT_LIST_SIZE 4
#endif

#if UI_HAS_JOYSTICK
  #define PRESS_LABEL "press Enter"
#else
  #define PRESS_LABEL "long press"
#endif

#include "icons.h"

class SplashScreen : public UIScreen {
  UITask* _task;
  unsigned long dismiss_after;
  char _version_info[12];

public:
  SplashScreen(UITask* task) : _task(task) {
    // strip off dash and commit hash by changing dash to null terminator
    // e.g: v1.2.3-abcdef -> v1.2.3
    const char *ver = FIRMWARE_VERSION;
    const char *dash = strchr(ver, '-');

    int len = dash ? dash - ver : strlen(ver);
    if (len >= sizeof(_version_info)) len = sizeof(_version_info) - 1;
    memcpy(_version_info, ver, len);
    _version_info[len] = 0;

    dismiss_after = millis() + BOOT_SCREEN_MILLIS;
  }

  int render(DisplayDriver& display) override {
    // meshcore logo
    display.setColor(DisplayDriver::BLUE);
    int logoWidth = 128;
    display.drawXbm((display.width() - logoWidth) / 2, 3, meshcore_logo, logoWidth, 13);

    // meshcore website
    const char* website = "https://meshcore.io";
    display.setColor(DisplayDriver::LIGHT);
    display.setTextSize(1);
    uint16_t websiteWidth = display.getTextWidth(website);
    display.setCursor((display.width() - websiteWidth) / 2, 22);
    display.print(website);

    // version info
    display.setColor(DisplayDriver::LIGHT);
    display.setTextSize(1);
    display.drawTextCentered(display.width()/2, 35, _version_info);

    display.setTextSize(1);
    display.drawTextCentered(display.width()/2, 48, FIRMWARE_BUILD_DATE);

    return 1000;
  }

  void poll() override {
    if (millis() >= dismiss_after) {
      _task->gotoHomeScreen();
    }
  }
};


class MsgPreviewScreen : public UIScreen {
  UITask* _task;
  mesh::RTCClock* _rtc;

  struct MsgEntry {
    uint32_t timestamp;
    char origin[62];
    char msg[78];
  };
  #define MAX_UNREAD_MSGS   32
  int num_unread;
  int head = MAX_UNREAD_MSGS - 1; // index of latest unread message
  MsgEntry unread[MAX_UNREAD_MSGS];

public:
  MsgPreviewScreen(UITask* task, mesh::RTCClock* rtc) : _task(task), _rtc(rtc) { num_unread = 0; }

  void addPreview(uint8_t path_len, const char* from_name, const char* msg) {
    head = (head + 1) % MAX_UNREAD_MSGS;
    if (num_unread < MAX_UNREAD_MSGS) num_unread++;

    auto p = &unread[head];
    p->timestamp = _rtc->getCurrentTime();
    if (path_len == 0xFF) {
      sprintf(p->origin, "(D) %s:", from_name);
    } else {
      sprintf(p->origin, "(%d) %s:", (uint32_t) path_len, from_name);
    }
    StrHelper::strncpy(p->msg, msg, sizeof(p->msg));
  }

  int render(DisplayDriver& display) override {
    char tmp[16];
    display.setCursor(0, 0);
    display.setTextSize(1);
    display.setColor(DisplayDriver::GREEN);
    sprintf(tmp, "Unread: %d", num_unread);
    display.print(tmp);

    auto p = &unread[head];

    int secs = _rtc->getCurrentTime() - p->timestamp;
    if (secs < 60) {
      sprintf(tmp, "%ds", secs);
    } else if (secs < 60*60) {
      sprintf(tmp, "%dm", secs / 60);
    } else {
      sprintf(tmp, "%dh", secs / (60*60));
    }
    display.setCursor(display.width() - display.getTextWidth(tmp) - 2, 0);
    display.print(tmp);

    display.drawRect(0, 11, display.width(), 1);  // horiz line

    display.setCursor(0, 14);
    display.setColor(DisplayDriver::YELLOW);
    char filtered_origin[sizeof(p->origin)];
    display.translateUTF8ToBlocks(filtered_origin, p->origin, sizeof(filtered_origin));
    display.print(filtered_origin);

    display.setCursor(0, 25);
    display.setColor(DisplayDriver::LIGHT);
    char filtered_msg[sizeof(p->msg)];
    display.translateUTF8ToBlocks(filtered_msg, p->msg, sizeof(filtered_msg));
    display.printWordWrap(filtered_msg, display.width());

#if AUTO_OFF_MILLIS==0 // probably e-ink
    return 10000; // 10 s
#else
    return 1000;  // next render after 1000 ms
#endif
  }

  bool handleInput(char c) override {
    if (c == KEY_NEXT || c == KEY_RIGHT) {
      head = (head + MAX_UNREAD_MSGS - 1) % MAX_UNREAD_MSGS;
      num_unread--;
      if (num_unread == 0) {
        _task->gotoHomeScreen();
      }
      return true;
    }
    if (c == KEY_ENTER) {
      num_unread = 0;  // clear unread queue
      _task->gotoHomeScreen();
      return true;
    }
    return false;
  }
};

void UITask::begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs) {
  _display = display;
  _sensors = sensors;
  _auto_off = millis() + AUTO_OFF_MILLIS;

#if defined(PIN_USER_BTN)
  user_btn.begin();
#endif
#if defined(PIN_USER_BTN_ANA)
  analog_btn.begin();
#endif
#if UI_HAS_TRACKBALL
  trackball_up.begin();
  trackball_down.begin();
  trackball_left.begin();
  trackball_right.begin();
#endif
#if UI_HAS_KEYBOARD
  tdeck_keyboard.begin();
#endif

  _node_prefs = node_prefs;

  if (_display != NULL) {
    _display->turnOn();
  }

#ifdef PIN_BUZZER
  buzzer.begin();
  buzzer.quiet(_node_prefs->buzzer_quiet);
  buzzer.startup();
#endif

#ifdef PIN_VIBRATION
  vibration.begin();
#endif

  ui_started_at = millis();
  _alert_expiry = 0;

  splash = new SplashScreen(this);
  home = new HomeLauncherScreen(this);   // MeshOS-style icon-grid launcher
  msg_preview = new MsgPreviewScreen(this, &rtc_clock);
  settings_list = new SettingsListScreen(this);
  setting_edit = new SettingEditScreen(this);
  packet_monitor = new PacketMonitorScreen(this);
  noise_scope = new NoiseScopeScreen(this);
  last_heard = new LastHeardScreen(this);
  signal_scr = new SignalScreen(this);
  trace_scr = new TraceRouteScreen(this);
  chat_home = new ChatHomeScreen(this, &chat_store);
  conversation = new ConversationScreen(this);
  repeaters_scr = new RepeatersScreen(this);
  repeater_detail = new RepeaterDetailScreen(this);
  setCurrScreen(splash);
}

void UITask::showAlert(const char* text, int duration_millis) {
  strcpy(_alert, text);
  _alert_expiry = millis() + duration_millis;
}

void UITask::notify(UIEventType t) {
#if defined(PIN_BUZZER)
switch(t){
  case UIEventType::contactMessage:
    // gemini's pick
    buzzer.play("MsgRcv3:d=4,o=6,b=200:32e,32g,32b,16c7");
    break;
  case UIEventType::channelMessage:
    buzzer.play("kerplop:d=16,o=6,b=120:32g#,32c#");
    break;
  case UIEventType::ack:
    buzzer.play("ack:d=32,o=8,b=120:c");
    break;
  case UIEventType::roomMessage:
  case UIEventType::newContactMessage:
  case UIEventType::none:
  default:
    break;
}
#endif

#ifdef PIN_VIBRATION
  // Trigger vibration for all UI events except none
  if (t != UIEventType::none) {
    vibration.trigger();
  }
#endif
}


void UITask::msgRead(int msgcount) {
  _msgcount = msgcount;
  if (msgcount == 0) {
    gotoHomeScreen();
  }
}

void UITask::newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) {
  _msgcount = msgcount;

  ((MsgPreviewScreen *) msg_preview)->addPreview(path_len, from_name, text);
  // don't yank the user out of an open chat screen with the preview popup
  if (curr != chat_home && curr != conversation) setCurrScreen(msg_preview);

  if (_display != NULL) {
    if (!_display->isOn() && !hasConnection()) {
      _display->turnOn();
    }
    if (_display->isOn()) {
    _auto_off = millis() + AUTO_OFF_MILLIS;  // extend the auto-off timer
    _next_refresh = 100;  // trigger refresh
    }
  }
}

void UITask::userLedHandler() {
#ifdef PIN_STATUS_LED
  int cur_time = millis();
  if (cur_time > next_led_change) {
    if (led_state == 0) {
      led_state = 1;
      if (_msgcount > 0) {
        last_led_increment = LED_ON_MSG_MILLIS;
      } else {
        last_led_increment = LED_ON_MILLIS;
      }
      next_led_change = cur_time + last_led_increment;
    } else {
      led_state = 0;
      next_led_change = cur_time + LED_CYCLE_MILLIS - last_led_increment;
    }
    digitalWrite(PIN_STATUS_LED, led_state == LED_STATE_ON);
  }
#endif
}

void UITask::setCurrScreen(UIScreen* c) {
  curr = c;
  _next_refresh = 100;
}

void UITask::gotoSettings() {
  settings_list->showRoot();
  setCurrScreen(settings_list);
}

void UITask::gotoPacketMonitor() {
  packet_monitor->reset();
  setCurrScreen(packet_monitor);
}

void UITask::onRawRx(float snr, float rssi, const uint8_t* raw, int len) {
  packet_monitor->addRaw(rtc_clock.getCurrentTime(), snr, rssi, raw, len);
  _last_rssi = (int)rssi;   // feed the home signal bars
}

void UITask::gotoNoise() {
  setCurrScreen(noise_scope);
}

// great-circle distance (metres) between two lat/lon points, -1 if either is missing
static int32_t haversine_m(double lat1, double lon1, double lat2, double lon2) {
  if ((lat1 == 0 && lon1 == 0) || (lat2 == 0 && lon2 == 0)) return -1;
  const double R = 6371000.0, D2R = 0.017453292519943295;
  double dlat = (lat2 - lat1) * D2R, dlon = (lon2 - lon1) * D2R;
  double a = sin(dlat / 2) * sin(dlat / 2) +
             cos(lat1 * D2R) * cos(lat2 * D2R) * sin(dlon / 2) * sin(dlon / 2);
  double c = 2 * atan2(sqrt(a), sqrt(1 - a));
  return (int32_t)(R * c);
}

void UITask::gotoHeard() {
  AdvertPath rec[16];
  int n = the_mesh.getRecentlyHeard(rec, 16);
  last_heard->clear();
  for (int i = 0; i < n; i++) {
    AdvertPath& a = rec[i];
    if (a.name[0] == 0 && a.recv_timestamp == 0) continue;  // empty slot
    HeardStation st;
    memset(&st, 0, sizeof(st));
    StrHelper::strncpy(st.name, a.name, sizeof(st.name));
    st.hops = a.path_len;
    st.recv_ts = a.recv_timestamp;
    st.snr_q = a.snr_q;
    st.rssi = a.rssi;
    st.dist_m = haversine_m(sensors.node_lat, sensors.node_lon,
                            a.gps_lat / 1000000.0, a.gps_lon / 1000000.0);
    last_heard->addStation(st);
  }
  last_heard->setNow(rtc_clock.getCurrentTime());
  last_heard->reset();
  setCurrScreen(last_heard);
}

void UITask::gotoSignal() {
  AdvertPath rec[16];
  int nrec = the_mesh.getRecentlyHeard(rec, 16);
  signal_scr->clear();
  int nc = the_mesh.getNumContacts();
  for (int i = 0; i < nc; i++) {
    ContactInfo c;
    if (!the_mesh.getContactByIdx(i, c)) continue;
    if (c.type != ADV_TYPE_REPEATER) continue;
    RepeaterSignal rp;
    memset(&rp, 0, sizeof(rp));
    StrHelper::strncpy(rp.name, c.name, sizeof(rp.name));
    rp.recv_ts = c.last_advert_timestamp;
    // match a recent advert (by pubkey prefix) for live SNR/RSSI
    for (int j = 0; j < nrec; j++) {
      if (memcmp(rec[j].pubkey_prefix, c.id.pub_key, sizeof(rec[j].pubkey_prefix)) == 0) {
        rp.snr_q = rec[j].snr_q;
        rp.rssi = rec[j].rssi;
        rp.recv_ts = rec[j].recv_timestamp;
        rp.heard = true;
        break;
      }
    }
    signal_scr->addRepeater(rp);
  }
  signal_scr->setNow(rtc_clock.getCurrentTime());
  signal_scr->reset();
  setCurrScreen(signal_scr);
}

void UITask::gotoTrace() {
  trace_scr->clearTargets();
  int nc = the_mesh.getNumContacts();
  for (int i = 0; i < nc; i++) {
    ContactInfo c;
    if (!the_mesh.getContactByIdx(i, c)) continue;
    if (c.out_path_len == OUT_PATH_UNKNOWN || c.out_path_len == 0) continue;  // need a path
    trace_scr->addTarget(c.name, i);
  }
  trace_scr->setNow(rtc_clock.getCurrentTime());
  trace_scr->beginPick();
  setCurrScreen(trace_scr);
}

uint32_t UITask::startTrace(int contact_idx) {
  ContactInfo c;
  if (!the_mesh.getContactByIdx(contact_idx, c)) return 0;
  uint32_t tag = 0;
  if (!the_mesh.sendTrace(c, tag)) return 0;
  return tag;
}

void UITask::onTraceResult(uint32_t tag, const uint8_t* path_hashes, const uint8_t* path_snrs,
                           uint8_t path_len, uint8_t path_sz, int8_t final_snr_q) {
  trace_scr->onResult(tag, path_hashes, path_snrs, path_len, path_sz, final_snr_q);
  _next_refresh = 100;
}

void UITask::doAdvertise(bool flood) {
  notify(UIEventType::ack);
  bool ok = flood ? the_mesh.advertFlood() : the_mesh.advert();
  showAlert(ok ? (flood ? "Flood advert sent" : "One-hop advert sent") : "Advert failed..", 1000);
}

void UITask::toggleBluetooth() {
  if (isSerialEnabled()) disableSerial(); else enableSerial();
  showAlert(isSerialEnabled() ? "Bluetooth: ON" : "Bluetooth: OFF", 800);
}

void UITask::getHomeStatus(HomeStatus& s) {
  static char name_buf[sizeof(((NodePrefs*)0)->node_name)];
  StrHelper::strncpy(name_buf, _node_prefs->node_name, sizeof(name_buf));
  s.node_name = name_buf;
  s.device = UI_DEVICE_LABEL;
  s.channel = "Public";
  s.epoch = rtc_clock.getCurrentTime();
  uint16_t mv = getBattMilliVolts();
  int pct = ((int)mv - BATT_MIN_MILLIVOLTS) * 100 / (BATT_MAX_MILLIVOLTS - BATT_MIN_MILLIVOLTS);
  s.batt_pct = mv == 0 ? -1 : (pct < 0 ? 0 : (pct > 100 ? 100 : pct));
  int r = _last_rssi;
  s.bars = (r == 0) ? 0 : (r >= -70 ? 4 : (r >= -85 ? 3 : (r >= -100 ? 2 : (r >= -112 ? 1 : 0))));
  s.msgcount = _msgcount;
}

bool UITask::composeUsesOSK() const {
  bool has_phys_kb =
#ifdef UI_HAS_KEYBOARD
      true;
#else
      false;
#endif
  return (_display != NULL) && _display->hasTouch() && !has_phys_kb;
}

void UITask::gotoChat() {
  chat_home->clear();
#ifdef MAX_GROUP_CHANNELS
  for (int i = 0; i < MAX_GROUP_CHANNELS; i++) {
    ChannelDetails cd;
    if (the_mesh.getChannel(i, cd) && cd.name[0]) chat_home->addChannel(i, cd.name);
  }
#endif
  int nc = the_mesh.getNumContacts();
  for (int i = 0; i < nc; i++) {
    ContactInfo c;
    if (!the_mesh.getContactByIdx(i, c)) continue;
    if (c.type == ADV_TYPE_CHAT || c.type == ADV_TYPE_ROOM)
      chat_home->addDm(c.id.pub_key, c.name);
  }
  chat_home->setNow(rtc_clock.getCurrentTime());
  chat_home->begin();
  setCurrScreen(chat_home);
}

void UITask::openConversation(bool is_channel, int channel_idx, const uint8_t* peer6, const char* title) {
  chat::Conv* c = is_channel ? chat_store.getOrCreateChannel(channel_idx, title)
                             : chat_store.getOrCreateDm(peer6, title);
  chat_store.markRead(c);
  conversation->setNow(rtc_clock.getCurrentTime());
  conversation->begin(c, title, composeUsesOSK());
  setCurrScreen(conversation);
}

void UITask::sendChatText(chat::Conv* c, const char* text) {
  if (!c || !text || !text[0]) return;
  uint32_t now = rtc_clock.getCurrentTime();
  if (c->is_channel) {
    bool ok = the_mesh.sendChannelText(c->channel_idx, text);
    chat::Msg* m = chat_store.addOutgoing(c, text, now, 0, 0);
    if (m && !ok) m->status = chat::ST_FAILED;
  } else {
    ContactInfo* ct = the_mesh.lookupContactByPubKey(c->peer, 6);
    uint32_t ack = 0, to = 0;
    bool ok = ct && the_mesh.sendTextTo(*ct, text, ack, to);
    chat::Msg* m = chat_store.addOutgoing(c, text, now, ok ? ack : 0,
                                          ok && to ? millis() + to : 0);
    if (m && !ok) m->status = chat::ST_FAILED;
  }
  chat_store.markRead(c);
  _next_refresh = 100;
}

void UITask::onTextMessage(bool is_channel, int channel_idx, const uint8_t* dm_prefix6,
                           const char* dm_name, const char* text, uint32_t timestamp,
                           uint8_t path_len, int8_t snr_q) {
  if (is_channel) {
    ChannelDetails cd;
    const char* cname = the_mesh.getChannel(channel_idx, cd) ? cd.name : "Channel";
    chat::Conv* c = chat_store.getOrCreateChannel(channel_idx, cname);
    // channel text arrives as "sender: body" -- split it for the bubble
    char sender[24] = {0};
    const char* body = text;
    const char* sep = strstr(text, ": ");
    if (sep && (sep - text) < (int)sizeof(sender)) {
      int n = sep - text;
      memcpy(sender, text, n); sender[n] = 0;
      body = sep + 2;
    }
    chat_store.addIncoming(c, sender, body, timestamp);
  } else {
    chat::Conv* c = chat_store.getOrCreateDm(dm_prefix6, dm_name ? dm_name : "");
    chat_store.addIncoming(c, "", text, timestamp);
  }
  _next_refresh = 100;
}

void UITask::onMsgSendConfirmed(uint32_t ack, uint32_t trip_millis) {
  chat_store.markDelivered(ack, trip_millis);
  _next_refresh = 100;
}

// ---- repeater management (M5) ----
void UITask::gotoRepeaters() {
  repeaters_scr->clear();
  int nc = the_mesh.getNumContacts();
  for (int i = 0; i < nc; i++) {
    ContactInfo c;
    if (!the_mesh.getContactByIdx(i, c)) continue;
    if (c.type == ADV_TYPE_REPEATER || c.type == ADV_TYPE_ROOM)
      repeaters_scr->addSaved(c.id.pub_key, c.name, c.type, (c.flags & 0x01) != 0);
  }
  ContactInfo cand[8];
  int nh = the_mesh.getHeardCandidates(cand, 8);
  for (int i = 0; i < nh; i++) {
    if (cand[i].type == ADV_TYPE_REPEATER || cand[i].type == ADV_TYPE_ROOM)
      repeaters_scr->addScan(cand[i].id.pub_key, cand[i].name, cand[i].type);
  }
  repeaters_scr->begin();
  setCurrScreen(repeaters_scr);
}

void UITask::gotoFinder() { gotoRepeaters(); repeaters_scr->setTab(1); }

void UITask::openRepeater(const uint8_t* pubkey6, const char* name, uint8_t type) {
  bool fav = false;
  ContactInfo* c = the_mesh.lookupContactByPubKey(pubkey6, 6);
  if (c) fav = (c->flags & 0x01) != 0;
  repeater_detail->begin(pubkey6, name, type, fav, composeUsesOSK());
  setCurrScreen(repeater_detail);
}

void UITask::addCandidate(const uint8_t* pubkey6) {
  showAlert(the_mesh.addHeardContact(pubkey6) ? "Added" : "Add failed", 1000);
  gotoRepeaters();  // refresh lists (moves it from Scan to Saved)
}

void UITask::requestStatus(const uint8_t* pubkey6) {
  uint32_t to;
  if (!the_mesh.uiRequestStatus(pubkey6, to)) showAlert("Status req failed", 1000);
}

void UITask::startLogin(const uint8_t* pubkey6, const char* pw) {
  uint32_t to;
  if (!the_mesh.uiLogin(pubkey6, pw, to)) showAlert("Login send failed", 1000);
}

void UITask::sendTrigger(const uint8_t* pubkey6, const char* cmd) {
  uint32_t to;
  if (!the_mesh.uiSendCommand(pubkey6, cmd, to)) showAlert("Send failed", 1000);
}

void UITask::toggleFavourite(const uint8_t* pubkey6, bool fav) {
  the_mesh.setContactFavourite(pubkey6, fav);
}

static bool prefixMatch(const uint8_t* a, const uint8_t* b) { return memcmp(a, b, 6) == 0; }

void UITask::onLoginResult(const uint8_t* prefix6, bool success, uint8_t perms) {
  if (curr == repeater_detail && prefixMatch(prefix6, repeater_detail->pubkey()))
    repeater_detail->onLogin(success, perms);
  _next_refresh = 100;
}
void UITask::onStatusResponse(const uint8_t* prefix6, const uint8_t* data, uint8_t len) {
  if (curr == repeater_detail && prefixMatch(prefix6, repeater_detail->pubkey()))
    repeater_detail->onStatus(data, len);
  _next_refresh = 100;
}
void UITask::onCommandReply(const uint8_t* prefix6, const char* text) {
  if (curr == repeater_detail && prefixMatch(prefix6, repeater_detail->pubkey()))
    repeater_detail->onReply(text);
  _next_refresh = 100;
}

void UITask::editSetting(const Setting* s) {
  bool has_phys_kb =
#ifdef UI_HAS_KEYBOARD
      true;
#else
      false;
#endif
  // use the on-screen keyboard only on touch boards that lack a physical keyboard
  bool use_osk = (_display != NULL) && _display->hasTouch() && !has_phys_kb;
  setting_edit->begin(s, use_osk);
  setCurrScreen(setting_edit);
}

void UITask::closeSettingEdit() {
  setCurrScreen(settings_list);   // back to the list (retains group + selection)
}

void UITask::pollTouch() {
  if (_display == NULL || !_display->hasTouch()) return;
  if (millis() < next_touch_check) return;
  next_touch_check = millis() + 20;

  int tx, ty;
  bool down = _display->getTouch(&tx, &ty);

  if (!_display->isOn()) {            // first touch just wakes the display
    if (down) { _display->turnOn(); _auto_off = millis() + AUTO_OFF_MILLIS; }
    _touching = false;
    return;
  }
  if (down && !_touching) {
    _touching = true; _touch_x = tx; _touch_y = ty;
    if (curr) curr->handleTouch(tx, ty, TouchEvent::press);
    _auto_off = millis() + AUTO_OFF_MILLIS; _next_refresh = 100;
  } else if (down && _touching) {
    _touch_x = tx; _touch_y = ty;
    if (curr) curr->handleTouch(tx, ty, TouchEvent::move);
    _auto_off = millis() + AUTO_OFF_MILLIS; _next_refresh = 100;
  } else if (!down && _touching) {
    _touching = false;
    if (curr) curr->handleTouch(_touch_x, _touch_y, TouchEvent::release);
    _auto_off = millis() + AUTO_OFF_MILLIS; _next_refresh = 100;
  }
}

void UITask::pollKeyboard(char& c) {
#ifdef UI_HAS_KEYBOARD
  if (millis() < next_kb_check) return;
  next_kb_check = millis() + 20;
  char kc = tdeck_keyboard.read();   // 0 if no key
  if (kc == 0) return;
  if (kc == 127) kc = KEY_BACKSPACE;  // DEL -> backspace
  c = checkDisplayOn(kc);
#else
  (void)c;
#endif
}

/*
  hardware-agnostic pre-shutdown activity should be done here
*/
void UITask::shutdown(bool restart){

  #ifdef PIN_BUZZER
  /* note: we have a choice here -
     we can do a blocking buzzer.loop() with non-deterministic consequences
     or we can set a flag and delay the shutdown for a couple of seconds
     while a non-blocking buzzer.loop() plays out in UITask::loop()
  */
  buzzer.shutdown();
  uint32_t buzzer_timer = millis(); // fail-safe shutdown
  while (buzzer.isPlaying() && (millis() - 2500) < buzzer_timer)
    buzzer.loop();

  #endif // PIN_BUZZER

  if (restart) {
    _board->reboot();
  } else {
    _display->turnOff();
    radio_driver.powerOff();
    _board->powerOff();
  }
}

bool UITask::isButtonPressed() const {
#ifdef PIN_USER_BTN
  return user_btn.isPressed();
#else
  return false;
#endif
}

void UITask::loop() {
  char c = 0;
#if UI_HAS_JOYSTICK
  int ev = user_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_ENTER);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_ENTER);  // REVISIT: could be mapped to different key code
  }
  ev = joystick_left.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_LEFT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_LEFT);
  }
  ev = joystick_right.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_RIGHT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_RIGHT);
  }
  ev = back_btn.check();
  if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    c = handleTripleClick(KEY_SELECT);
  }
#elif defined(PIN_USER_BTN)
  int ev = user_btn.check();
  if (ev == BUTTON_EVENT_CLICK) {
    c = checkDisplayOn(KEY_NEXT);
  } else if (ev == BUTTON_EVENT_LONG_PRESS) {
    c = handleLongPress(KEY_ENTER);
  } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
    c = handleDoubleClick(KEY_PREV);
  } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
    c = handleTripleClick(KEY_SELECT);
  }
#endif
#if defined(PIN_USER_BTN_ANA)
  if (abs(millis() - _analogue_pin_read_millis) > 10) {
    int ev = analog_btn.check();
    if (ev == BUTTON_EVENT_CLICK) {
      c = checkDisplayOn(KEY_NEXT);
    } else if (ev == BUTTON_EVENT_LONG_PRESS) {
      c = handleLongPress(KEY_ENTER);
    } else if (ev == BUTTON_EVENT_DOUBLE_CLICK) {
      c = handleDoubleClick(KEY_PREV);
    } else if (ev == BUTTON_EVENT_TRIPLE_CLICK) {
      c = handleTripleClick(KEY_SELECT);
    }
    _analogue_pin_read_millis = millis();
  }
#endif
#if defined(BACKLIGHT_BTN)
  if (millis() > next_backlight_btn_check) {
    bool touch_state = digitalRead(PIN_BUTTON2);
#if defined(DISP_BACKLIGHT)
    digitalWrite(DISP_BACKLIGHT, !touch_state);
#elif defined(EXP_PIN_BACKLIGHT)
    expander.digitalWrite(EXP_PIN_BACKLIGHT, !touch_state);
#endif
    next_backlight_btn_check = millis() + 300;
  }
#endif

#if UI_HAS_TRACKBALL
  {
    int tev;
    tev = trackball_up.check();    if (tev == BUTTON_EVENT_CLICK) c = checkDisplayOn(KEY_UP);
    tev = trackball_down.check();  if (tev == BUTTON_EVENT_CLICK) c = checkDisplayOn(KEY_DOWN);
    tev = trackball_left.check();  if (tev == BUTTON_EVENT_CLICK) c = checkDisplayOn(KEY_LEFT);
    tev = trackball_right.check(); if (tev == BUTTON_EVENT_CLICK) c = checkDisplayOn(KEY_RIGHT);
  }
#endif

#if UI_HAS_KEYBOARD
  pollKeyboard(c);
#endif

  if (c != 0 && curr) {
    curr->handleInput(c);
    _auto_off = millis() + AUTO_OFF_MILLIS;   // extend auto-off timer
    _next_refresh = 100;  // trigger refresh
  }

  pollTouch();   // coordinate input (no-op on boards without a touch display)

  userLedHandler();

#ifdef PIN_BUZZER
  if (buzzer.isPlaying())  buzzer.loop();
#endif

  if (curr) curr->poll();
  uint32_t now_s = rtc_clock.getCurrentTime();
  packet_monitor->setNow(now_s);
  last_heard->setNow(now_s);
  signal_scr->setNow(now_s);
  trace_scr->setNow(now_s);
  chat_home->setNow(now_s);
  conversation->setNow(now_s);
  chat_store.expireSends(millis());

  // sample the RF noise floor continuously so the scope has history when opened
  if (millis() >= next_noise_sample) {
    next_noise_sample = millis() + NOISE_SAMPLE_MILLIS;
    noise_scope->addSample(radio_driver.getNoiseFloor());
  }

  if (_display != NULL && _display->isOn()) {
    if (millis() >= _next_refresh && curr) {
      _display->startFrame();
      int delay_millis = curr->render(*_display);
      if (millis() < _alert_expiry) {  // render alert popup
        _display->setTextSize(1);
        int y = _display->height() / 3;
        int p = _display->height() / 32;
        _display->setColor(DisplayDriver::DARK);
        _display->fillRect(p, y, _display->width() - p*2, y);
        _display->setColor(DisplayDriver::LIGHT);  // draw box border
        _display->drawRect(p, y, _display->width() - p*2, y);
        _display->drawTextCentered(_display->width() / 2, y + p*3, _alert);
        _next_refresh = _alert_expiry;   // will need refresh when alert is dismissed
      } else {
        _next_refresh = millis() + delay_millis;
      }
      _display->endFrame();
    }
#if AUTO_OFF_MILLIS > 0
#ifdef KEEP_DISPLAY_ON_USB
    // Opt-in: refresh the auto-off deadline while externally powered, so the
    // timer counts from the moment external power is removed. Off by default
    // because OLED panels burn in quickly; only enable for LCD targets or
    // where the display is replaceable.
    if (board.isExternalPowered()) {
      _auto_off = millis() + AUTO_OFF_MILLIS;
    }
#endif
    if (millis() > _auto_off) {
      _display->turnOff();
    }
#endif
  }

#ifdef PIN_VIBRATION
  vibration.loop();
#endif

#ifdef AUTO_SHUTDOWN_MILLIVOLTS
  if (millis() > next_batt_chck) {
    uint16_t milliVolts = getBattMilliVolts();
    if (milliVolts > 0 && milliVolts < AUTO_SHUTDOWN_MILLIVOLTS) {
      if(!board.isExternalPowered()) {
        if (_display != NULL) {
          _display->startFrame();
          _display->setTextSize(2);
          _display->setColor(DisplayDriver::RED);
          _display->drawTextCentered(_display->width() / 2, 20, "Low Battery.");
          _display->drawTextCentered(_display->width() / 2, 40, "Shutting Down!");
          _display->endFrame();
          if (_display->isEink() == false) { delay(3000); }
        }
        shutdown();
      }
    }
    next_batt_chck = millis() + 8000;
  }
#endif
}

char UITask::checkDisplayOn(char c) {
  if (_display != NULL) {
    if (!_display->isOn()) {
      _display->turnOn();   // turn display on and consume event
      c = 0;
    }
    _auto_off = millis() + AUTO_OFF_MILLIS;   // extend auto-off timer
    _next_refresh = 0;  // trigger refresh
  }
  return c;
}

char UITask::handleLongPress(char c) {
  if (millis() - ui_started_at < 8000) {   // long press in first 8 seconds since startup -> CLI/rescue
    the_mesh.enterCLIRescue();
    c = 0;   // consume event
  }
  return c;
}

char UITask::handleDoubleClick(char c) {
  MESH_DEBUG_PRINTLN("UITask: double-click triggered");
  checkDisplayOn(c);
  return c;
}

char UITask::handleTripleClick(char c) {
  MESH_DEBUG_PRINTLN("UITask: triple click triggered");
  checkDisplayOn(c);
  toggleBuzzer();
  c = 0;
  return c;
}

bool UITask::getGPSState() {
  if (_sensors != NULL) {
    int num = _sensors->getNumSettings();
    for (int i = 0; i < num; i++) {
      if (strcmp(_sensors->getSettingName(i), "gps") == 0) {
        return !strcmp(_sensors->getSettingValue(i), "1");
      }
    }
  }
  return false;
}

void UITask::toggleGPS() {
    if (_sensors != NULL) {
    // toggle GPS on/off
    int num = _sensors->getNumSettings();
    for (int i = 0; i < num; i++) {
      if (strcmp(_sensors->getSettingName(i), "gps") == 0) {
        if (strcmp(_sensors->getSettingValue(i), "1") == 0) {
          _sensors->setSettingValue("gps", "0");
          _node_prefs->gps_enabled = 0;
          notify(UIEventType::ack);
        } else {
          _sensors->setSettingValue("gps", "1");
          _node_prefs->gps_enabled = 1;
          notify(UIEventType::ack);
        }
        the_mesh.savePrefs();
        showAlert(_node_prefs->gps_enabled ? "GPS: Enabled" : "GPS: Disabled", 800);
        _next_refresh = 0;
        break;
      }
    }
  }
}

void UITask::toggleBuzzer() {
    // Toggle buzzer quiet mode
  #ifdef PIN_BUZZER
    if (buzzer.isQuiet()) {
      buzzer.quiet(false);
      notify(UIEventType::ack);
    } else {
      buzzer.quiet(true);
    }
    _node_prefs->buzzer_quiet = buzzer.isQuiet();
    the_mesh.savePrefs();
    showAlert(buzzer.isQuiet() ? "Buzzer: OFF" : "Buzzer: ON", 800);
    _next_refresh = 0;  // trigger refresh
  #endif
}
