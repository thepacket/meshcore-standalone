#pragma once

#include <MeshCore.h>
#include <helpers/ui/DisplayDriver.h>
#include <helpers/ui/UIScreen.h>
#include <helpers/SensorManager.h>
#include <helpers/BaseSerialInterface.h>
#include <Arduino.h>
#include <helpers/sensors/LPPDataHelpers.h>

#ifndef LED_STATE_ON
  #define LED_STATE_ON 1
#endif

#ifdef PIN_BUZZER
  #include <helpers/ui/buzzer.h>
#endif
#ifdef PIN_VIBRATION
  #include <helpers/ui/GenericVibration.h>
#endif

#include "../AbstractUITask.h"
#include "../NodePrefs.h"
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

class UITask : public AbstractUITask {
  DisplayDriver* _display;
  SensorManager* _sensors;
#ifdef PIN_BUZZER
  genericBuzzer buzzer;
#endif
#ifdef PIN_VIBRATION
  GenericVibration vibration;
#endif
  unsigned long _next_refresh, _auto_off;
  NodePrefs* _node_prefs;
  char _alert[80];
  unsigned long _alert_expiry;
  int _msgcount;
  unsigned long ui_started_at, next_batt_chck;
  int next_backlight_btn_check = 0;
#ifdef PIN_STATUS_LED
  int led_state = 0;
  int next_led_change = 0;
  int last_led_increment = 0;
#endif

#ifdef PIN_USER_BTN_ANA
  unsigned long _analogue_pin_read_millis = millis();
#endif

  UIScreen* splash;
  UIScreen* home;
  UIScreen* msg_preview;
  UIScreen* curr;
  SettingsListScreen* settings_list;
  SettingEditScreen*  setting_edit;
  PacketMonitorScreen* packet_monitor;
  NoiseScopeScreen* noise_scope;
  LastHeardScreen* last_heard;
  SignalScreen* signal_scr;
  TraceRouteScreen* trace_scr;
  chat::ChatStore chat_store;
  ChatHomeScreen* chat_home;
  ConversationScreen* conversation;
  RepeatersScreen* repeaters_scr;
  RepeaterDetailScreen* repeater_detail;
  unsigned long next_noise_sample = 0;
  int _last_rssi = 0;   // last received RSSI, for the home signal bars (0 = unknown)

  bool composeUsesOSK() const;

  // touch + keyboard polling state
  unsigned long next_touch_check = 0;
  unsigned long next_kb_check = 0;
  bool _touching = false;
  int _touch_x = 0, _touch_y = 0;

  void userLedHandler();
  void pollTouch();
  void pollKeyboard(char& c);

  // Button action handlers
  char checkDisplayOn(char c);
  char handleLongPress(char c);
  char handleDoubleClick(char c);
  char handleTripleClick(char c);

  void setCurrScreen(UIScreen* c);

public:

  UITask(mesh::MainBoard* board, BaseSerialInterface* serial) : AbstractUITask(board, serial), _display(NULL), _sensors(NULL) {
    next_batt_chck = _next_refresh = 0;
    ui_started_at = 0;
    curr = NULL;
  }
  void begin(DisplayDriver* display, SensorManager* sensors, NodePrefs* node_prefs);

  void gotoHomeScreen() { setCurrScreen(home); }
  void gotoSettings();
  void gotoPacketMonitor();
  void gotoNoise();
  void gotoHeard();
  void gotoSignal();
  void gotoTrace();
  void doAdvertise(bool flood);
  void toggleBluetooth();
  uint32_t startTrace(int contact_idx);   // returns trace tag (0 = failed)
  void getHomeStatus(HomeStatus& s);
  void gotoChat();                        // chat home (Channels/DMs tabs)
  void openConversation(bool is_channel, int channel_idx, const uint8_t* peer6, const char* title);
  void sendChatText(chat::Conv* c, const char* text);
  // repeater management (M5)
  void gotoRepeaters();                   // saved tab
  void gotoFinder();                      // scan tab
  void openRepeater(const uint8_t* pubkey6, const char* name, uint8_t type);
  void addCandidate(const uint8_t* pubkey6);
  void requestStatus(const uint8_t* pubkey6);
  void startLogin(const uint8_t* pubkey6, const char* pw);
  void sendTrigger(const uint8_t* pubkey6, const char* cmd);
  void toggleFavourite(const uint8_t* pubkey6, bool fav);
  void editSetting(const Setting* s);
  void closeSettingEdit();
  DisplayDriver* getDisplay() { return _display; }
  void showAlert(const char* text, int duration_millis);
  int  getMsgCount() const { return _msgcount; }
  bool hasDisplay() const { return _display != NULL; }
  bool isButtonPressed() const;

  bool isBuzzerQuiet() { 
#ifdef PIN_BUZZER
    return buzzer.isQuiet();
#else
    return true;
#endif
  }

  void toggleBuzzer();
  bool getGPSState();
  void toggleGPS();


  // from AbstractUITask
  void msgRead(int msgcount) override;
  void newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) override;
  void notify(UIEventType t = UIEventType::none) override;
  void onRawRx(float snr, float rssi, const uint8_t* raw, int len) override;
  void onTraceResult(uint32_t tag, const uint8_t* path_hashes, const uint8_t* path_snrs,
                     uint8_t path_len, uint8_t path_sz, int8_t final_snr_q) override;
  void onTextMessage(bool is_channel, int channel_idx, const uint8_t* dm_prefix6,
                     const char* dm_name, const char* text, uint32_t timestamp,
                     uint8_t path_len, int8_t snr_q, uint8_t txt_type,
                     const uint8_t* author_prefix4) override;
  void onMsgSendConfirmed(uint32_t ack, uint32_t trip_millis) override;
  void onLoginResult(const uint8_t* prefix6, bool success, uint8_t perms) override;
  void onStatusResponse(const uint8_t* prefix6, const uint8_t* data, uint8_t len) override;
  void onCommandReply(const uint8_t* prefix6, const char* text) override;
  void loop() override;

  void shutdown(bool restart = false);
};
