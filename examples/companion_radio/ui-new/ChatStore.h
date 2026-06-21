#pragma once

#include <stdint.h>

// Volatile in-RAM message store for the on-device chat UI. The companion keeps
// no message history, so this holds recent threads only (lost on reboot; SD/flash
// persistence is a later milestone). Conversation *lists* come from live mesh
// enumeration (channels / contacts) -- this just backs the message threads.

namespace chat {

#ifndef CHAT_MAX_CONVS
  #define CHAT_MAX_CONVS 8        // max simultaneously-remembered threads (LRU-evicted)
#endif
#ifndef CHAT_MSGS_PER_CONV
  #define CHAT_MSGS_PER_CONV 16   // ring depth per thread
#endif
#ifndef CHAT_TEXT_LEN
  #define CHAT_TEXT_LEN 100
#endif
#ifndef CHAT_NAME_LEN
  #define CHAT_NAME_LEN 20
#endif

enum Status : uint8_t { ST_RECV = 0, ST_SENDING, ST_SENT, ST_DELIVERED, ST_FAILED };

struct Msg {
  uint32_t ts;          // sender timestamp (RTC secs)
  uint32_t ack;         // outgoing only: expected ack CRC (0 = none/channel)
  uint32_t deadline;    // outgoing only: millis() delivery deadline (0 = none)
  uint8_t  status;      // Status
  bool     outgoing;
  char     sender[CHAT_NAME_LEN];  // incoming channel msgs: who sent it
  char     text[CHAT_TEXT_LEN];
};

struct Conv {
  bool     is_channel;
  int      channel_idx;            // valid when is_channel
  uint8_t  peer[6];                // valid when !is_channel (pubkey prefix)
  char     title[CHAT_NAME_LEN];
  Msg      ring[CHAT_MSGS_PER_CONV];
  uint8_t  count;                  // number of messages held (<= CHAT_MSGS_PER_CONV)
  uint8_t  head;                   // ring index of newest message
  uint8_t  unread;
  uint32_t last_ts;                // for ordering + LRU eviction
  bool     used;
};

class ChatStore {
  Conv _convs[CHAT_MAX_CONVS];
public:
  ChatStore();

  Conv* findChannel(int idx);
  Conv* findDm(const uint8_t* peer6);
  Conv* getOrCreateChannel(int idx, const char* title);
  Conv* getOrCreateDm(const uint8_t* peer6, const char* title);

  // append helpers (advance the ring, fill the new slot)
  void addIncoming(Conv* c, const char* sender, const char* text, uint32_t ts);
  Msg* addOutgoing(Conv* c, const char* text, uint32_t ts, uint32_t ack, uint32_t deadline);

  void markRead(Conv* c) { if (c) c->unread = 0; }
  void markDelivered(uint32_t ack, uint32_t trip_millis);  // search all threads
  void expireSends(uint32_t now_millis);                   // SENDING past deadline -> FAILED

  // message access in display order (0 = oldest held, count-1 = newest)
  Msg* msgAt(Conv* c, int display_idx);
  int  msgCount(Conv* c) { return c ? c->count : 0; }

private:
  Conv* allocConv();   // reuse a free slot or evict the oldest
  Msg*  pushSlot(Conv* c);
};

}  // namespace chat
