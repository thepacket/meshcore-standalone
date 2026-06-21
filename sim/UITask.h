#pragma once

// Desktop stand-in for the firmware UITask, providing exactly the methods the
// real SettingsScreen.cpp / SettingEditScreen call. This file deliberately
// shadows the firmware's UITask.h during the simulator build (see build.sh),
// so the real screen sources compile against this lightweight host shim instead
// of the MeshCore/Arduino stack.
#include <helpers/ui/UIScreen.h>
#include "SettingsModel.h"
#include "SettingsScreen.h"
#include "PacketMonitorScreen.h"
#include "HomeLauncherScreen.h"
#include "NoiseScopeScreen.h"
#include "LastHeardScreen.h"
#include "SignalScreen.h"
#include "TraceRouteScreen.h"
#include "ChatStore.h"
#include "ChatHomeScreen.h"
#include "ConversationScreen.h"
#include "RepeatersScreen.h"
#include "RepeaterDetailScreen.h"
#include "SimDisplay.h"
#include <string.h>
#include <SDL.h>

class UITask {
  DisplayDriver* _disp;
  HomeLauncherScreen* _home;
  SettingsListScreen* _list;
  SettingEditScreen* _edit;
  PacketMonitorScreen* _pkt;
  NoiseScopeScreen* _noise;
  LastHeardScreen* _heard;
  SignalScreen* _signal;
  TraceRouteScreen* _trace;
  chat::ChatStore _store;
  ChatHomeScreen* _chat;
  ConversationScreen* _conv;
  RepeatersScreen* _rep;
  RepeaterDetailScreen* _repdetail;
  UIScreen* _curr;
  char _alert[80];
  Uint32 _alert_expiry;

public:
  explicit UITask(DisplayDriver* d) : _disp(d), _curr(nullptr), _alert_expiry(0) {
    _alert[0] = 0;
    _home = new HomeLauncherScreen(this);
    _list = new SettingsListScreen(this);
    _edit = new SettingEditScreen(this);
    _pkt = new PacketMonitorScreen(this);
    _noise = new NoiseScopeScreen(this);
    _heard = new LastHeardScreen(this);
    _signal = new SignalScreen(this);
    _trace = new TraceRouteScreen(this);
    _chat = new ChatHomeScreen(this, &_store);
    _conv = new ConversationScreen(this);
    _rep = new RepeatersScreen(this);
    _repdetail = new RepeaterDetailScreen(this);
    _list->showRoot();
    _curr = _home;
  }

  DisplayDriver* getDisplay() { return _disp; }
  void gotoHomeScreen() { _curr = _home; }
  void gotoSettings() { _list->showRoot(); _curr = _list; }
  void gotoPacketMonitor() { _pkt->reset(); _curr = _pkt; }
  void editSetting(const Setting* s) { _edit->begin(s, /*use_osk=*/true); _curr = _edit; }
  void closeSettingEdit() { _curr = _list; }

  // launcher tile destinations
  void gotoHeard()  { _heard->reset(); _curr = _heard; }
  void gotoNoise()  { _curr = _noise; }
  void gotoSignal() { _signal->reset(); _curr = _signal; }
  void gotoTrace()  { _trace->beginPick(); _curr = _trace; }
  void gotoChat()   { _chat->begin(); _curr = _chat; }
  void openConversation(bool is_channel, int channel_idx, const uint8_t* peer6, const char* title) {
    chat::Conv* c = is_channel ? _store.getOrCreateChannel(channel_idx, title)
                               : _store.getOrCreateDm(peer6, title);
    _store.markRead(c);
    _conv->begin(c, title, /*use_osk=*/true);
    _curr = _conv;
  }
  void sendChatText(chat::Conv* c, const char* text) {
    _store.addOutgoing(c, text, 1000, 0, 0);  // sim: no radio, mark sent
  }
  chat::ChatStore& chatStore() { return _store; }   // sim seeder access
  ChatHomeScreen* chatHome() { return _chat; }

  // repeater management (sim stand-ins)
  void gotoRepeaters() { _rep->setTab(0); _curr = _rep; }
  void gotoFinder() { _rep->setTab(1); _curr = _rep; }
  void openRepeater(const uint8_t* pk, const char* name, uint8_t type) {
    _repdetail->begin(pk, name, type, false, /*use_osk=*/true); _curr = _repdetail;
  }
  void addCandidate(const uint8_t*) { showAlert("Added", 1000); }
  void requestStatus(const uint8_t*) {
    uint8_t b[56]; memset(b, 0, sizeof(b));
    auto w16 = [&](int o, uint16_t v) { b[o] = v; b[o + 1] = v >> 8; };
    auto w32 = [&](int o, uint32_t v) { b[o] = v; b[o + 1] = v >> 8; b[o + 2] = v >> 16; b[o + 3] = v >> 24; };
    w16(0, 4050); w16(2, 2); w16(4, (uint16_t)(int16_t)-118); w16(6, (uint16_t)(int16_t)-92);
    w32(8, 150234); w32(12, 88291); w32(16, 4210); w32(20, 372640);
    w32(24, 41000); w32(28, 47291); w32(32, 90000); w32(36, 60234);
    w16(40, 3); w16(42, (uint16_t)(int16_t)(7 * 4)); w16(44, 11); w16(46, 27);
    w32(48, 3120); w32(52, 5);
    _repdetail->onStatus(b, 56);
  }
  void startLogin(const uint8_t*, const char*) { _repdetail->onLogin(true, 1); }
  void sendTrigger(const uint8_t*, const char* cmd) {
    _repdetail->onReply(cmd[0] == 'a' ? "OK - Advert sent" : "OK - clock set");
  }
  void toggleFavourite(const uint8_t*, bool) {}
  RepeatersScreen* repeaters() { return _rep; }   // sim seeder access

  void doAdvertise(bool flood) { showAlert(flood ? "Flood advert sent" : "One-hop advert sent", 1200); }
  void toggleBluetooth() { showAlert("Bluetooth toggled", 1000); }
  void shutdown(bool restart) { showAlert(restart ? "Rebooting" : "Shutting down", 1200); }
  uint32_t startTrace(int idx) { return 0xCAFEBABE; }  // sim: pretend a trace was sent

  // sim seeders for previewing the diagnostic screens
  void simNoiseSample(int dbm) { _noise->addSample(dbm); }
  void simAddHeard(const HeardStation& s) { _heard->addStation(s); }
  void simAddRepeater(const RepeaterSignal& r) { _signal->addRepeater(r); }
  void simAddTraceTarget(const char* name, int idx) { _trace->addTarget(name, idx); }
  void simTraceResult(const char* name, const uint8_t* hashes, const uint8_t* snrs,
                      uint8_t n, int8_t final_q) {
    _trace->onTraceStarted(0xCAFEBABE, name);
    _trace->onResult(0xCAFEBABE, hashes, snrs, n, 0, final_q);
  }
  void simSetDiagNow(uint32_t now) {
    _heard->setNow(now); _signal->setNow(now); _trace->setNow(now);
  }

  // status for the launcher's top/bottom bars (fake data for preview)
  void getHomeStatus(HomeStatus& s) {
    s.node_name = "Andy";
    s.device = "TDeck+";
    s.channel = "Public";
    s.epoch = 15 * 3600 + 34 * 60;  // 15:34
    s.batt_pct = 78;
    s.bars = 3;
    s.msgcount = 2;
  }

  // sim helpers to preview the packet monitor with fake traffic
  void simAddRaw(uint32_t now, float snr, float rssi, const uint8_t* raw, int len) {
    _pkt->addRaw(now, snr, rssi, raw, len);
  }
  void simSetPacketNow(uint32_t now) { _pkt->setNow(now); }
  void showAlert(const char* t, int ms) {
    strncpy(_alert, t, sizeof(_alert) - 1);
    _alert[sizeof(_alert) - 1] = 0;
    _alert_expiry = SDL_GetTicks() + ms;
  }

  void onKey(char c) { if (_curr) _curr->handleInput(c); }
  void onTouch(int x, int y, TouchEvent ev) { if (_curr) _curr->handleTouch(x, y, ev); }

  void render() {
    if (!_curr) return;
    _disp->startFrame();
    _curr->render(*_disp);
    if (SDL_GetTicks() < _alert_expiry) {
      int w = _disp->width(), h = _disp->height();
      _disp->setColor(DisplayDriver::DARK);
      _disp->fillRect(w / 8, h / 3, w * 3 / 4, h / 5);
      _disp->setColor(DisplayDriver::LIGHT);
      _disp->drawRect(w / 8, h / 3, w * 3 / 4, h / 5);
      _disp->setTextSize(1);
      _disp->drawTextCentered(w / 2, h / 3 + h / 10 - 4, _alert);
    }
    _disp->endFrame();
  }
};
