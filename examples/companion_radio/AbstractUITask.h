#pragma once

#include <MeshCore.h>
#include <helpers/ui/DisplayDriver.h>
#include <helpers/ui/UIScreen.h>
#include <helpers/SensorManager.h>
#include <helpers/BaseSerialInterface.h>
#include <Arduino.h>

#ifdef PIN_BUZZER
  #include <helpers/ui/buzzer.h>
#endif

#include "NodePrefs.h"

enum class UIEventType {
    none,
    contactMessage,
    channelMessage,
    roomMessage,
    newContactMessage,
    ack
};

class AbstractUITask {
protected:
  mesh::MainBoard* _board;
  BaseSerialInterface* _serial;
  bool _connected;

  AbstractUITask(mesh::MainBoard* board, BaseSerialInterface* serial) : _board(board), _serial(serial) {
    _connected = false;
  }

public:
  void setHasConnection(bool connected) { _connected = connected; }
  bool hasConnection() const { return _connected; }
  uint16_t getBattMilliVolts() const { return _board->getBattMilliVolts(); }
  bool isSerialEnabled() const { return _serial->isEnabled(); }
  void enableSerial() { _serial->enable(); }
  void disableSerial() { _serial->disable(); }
  virtual void msgRead(int msgcount) = 0;
  virtual void newMsg(uint8_t path_len, const char* from_name, const char* text, int msgcount) = 0;
  // Rich incoming-message hook for the on-device chat store. For channel messages
  // is_channel=true and channel_idx is set (dm_* are NULL); for direct messages
  // dm_prefix6 (6-byte pubkey prefix) and dm_name identify the sender. txt_type is
  // the TXT_TYPE_* of the message; for signed messages (room-server posts)
  // author_prefix4 is the original poster's 4-byte pubkey prefix (else NULL).
  // Default no-op.
  virtual void onTextMessage(bool is_channel, int channel_idx, const uint8_t* dm_prefix6,
                             const char* dm_name, const char* text, uint32_t timestamp,
                             uint8_t path_len, int8_t snr_q, uint8_t txt_type,
                             const uint8_t* author_prefix4) {}
  // An outgoing message was acknowledged (delivery confirmed). ack matches the
  // expected_ack returned when sending. Default no-op.
  virtual void onMsgSendConfirmed(uint32_t ack, uint32_t trip_millis) {}
  // --- remote repeater/room management (M5) ---
  // Result of a login attempt to a repeater/room. perms = permission bits (admin etc).
  virtual void onLoginResult(const uint8_t* pubkey_prefix6, bool success, uint8_t perms) {}
  // A status/stats response from a repeater/room (the RepeaterStats blob, len bytes).
  virtual void onStatusResponse(const uint8_t* pubkey_prefix6, const uint8_t* data, uint8_t len) {}
  // A telemetry response from a contact (CayenneLPP blob, len bytes).
  virtual void onTelemetryResponse(const uint8_t* pubkey_prefix6, const uint8_t* data, uint8_t len) {}
  // A CLI reply (TXT_TYPE_CLI_DATA) from a contact, e.g. the result of a remote command.
  virtual void onCommandReply(const uint8_t* pubkey_prefix6, const char* text) {}
  virtual void notify(UIEventType t = UIEventType::none) = 0;
  // raw received packet (pre-decryption) for the on-device packet monitor; default no-op
  virtual void onRawRx(float snr, float rssi, const uint8_t* raw, int len) {}
  // result of an on-device trace-route; default no-op. path_snrs has (path_len >> path_sz)
  // entries; final_snr_q is the trace's SNR (*4) as heard by us. tag matches sendTrace().
  virtual void onTraceResult(uint32_t tag, const uint8_t* path_hashes, const uint8_t* path_snrs,
                             uint8_t path_len, uint8_t path_sz, int8_t final_snr_q) {}
  virtual void loop() = 0;
};
