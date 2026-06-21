#include "ChatStore.h"
#include <string.h>

namespace chat {

static void copyStr(char* dst, const char* src, int n) {
  if (!src) { dst[0] = 0; return; }
  int i = 0;
  for (; i < n - 1 && src[i]; i++) dst[i] = src[i];
  dst[i] = 0;
}

ChatStore::ChatStore() {
  memset(_convs, 0, sizeof(_convs));
}

Conv* ChatStore::findChannel(int idx) {
  for (int i = 0; i < CHAT_MAX_CONVS; i++)
    if (_convs[i].used && _convs[i].is_channel && _convs[i].channel_idx == idx)
      return &_convs[i];
  return nullptr;
}

Conv* ChatStore::findDm(const uint8_t* peer6) {
  for (int i = 0; i < CHAT_MAX_CONVS; i++)
    if (_convs[i].used && !_convs[i].is_channel &&
        memcmp(_convs[i].peer, peer6, 6) == 0)
      return &_convs[i];
  return nullptr;
}

Conv* ChatStore::allocConv() {
  // free slot first
  for (int i = 0; i < CHAT_MAX_CONVS; i++)
    if (!_convs[i].used) { memset(&_convs[i], 0, sizeof(Conv)); return &_convs[i]; }
  // else evict the least-recently-active thread
  int oldest = 0;
  for (int i = 1; i < CHAT_MAX_CONVS; i++)
    if (_convs[i].last_ts < _convs[oldest].last_ts) oldest = i;
  memset(&_convs[oldest], 0, sizeof(Conv));
  return &_convs[oldest];
}

Conv* ChatStore::getOrCreateChannel(int idx, const char* title) {
  Conv* c = findChannel(idx);
  if (c) return c;
  c = allocConv();
  c->used = true;
  c->is_channel = true;
  c->channel_idx = idx;
  copyStr(c->title, title, CHAT_NAME_LEN);
  return c;
}

Conv* ChatStore::getOrCreateDm(const uint8_t* peer6, const char* title) {
  Conv* c = findDm(peer6);
  if (c) return c;
  c = allocConv();
  c->used = true;
  c->is_channel = false;
  memcpy(c->peer, peer6, 6);
  copyStr(c->title, title, CHAT_NAME_LEN);
  return c;
}

Msg* ChatStore::pushSlot(Conv* c) {
  c->head = (c->count == 0) ? 0 : (c->head + 1) % CHAT_MSGS_PER_CONV;
  if (c->count < CHAT_MSGS_PER_CONV) c->count++;
  Msg* m = &c->ring[c->head];
  memset(m, 0, sizeof(Msg));
  return m;
}

void ChatStore::addIncoming(Conv* c, const char* sender, const char* text, uint32_t ts) {
  if (!c) return;
  Msg* m = pushSlot(c);
  m->outgoing = false;
  m->status = ST_RECV;
  m->ts = ts;
  copyStr(m->sender, sender, CHAT_NAME_LEN);
  copyStr(m->text, text, CHAT_TEXT_LEN);
  c->last_ts = ts;
  c->unread++;
}

Msg* ChatStore::addOutgoing(Conv* c, const char* text, uint32_t ts, uint32_t ack, uint32_t deadline) {
  if (!c) return nullptr;
  Msg* m = pushSlot(c);
  m->outgoing = true;
  m->status = (ack != 0) ? ST_SENDING : ST_SENT;  // channels have no ack -> SENT
  m->ts = ts;
  m->ack = ack;
  m->deadline = deadline;
  m->sender[0] = 0;
  copyStr(m->text, text, CHAT_TEXT_LEN);
  c->last_ts = ts;
  return m;
}

void ChatStore::markDelivered(uint32_t ack, uint32_t trip_millis) {
  if (ack == 0) return;
  for (int i = 0; i < CHAT_MAX_CONVS; i++) {
    Conv& c = _convs[i];
    if (!c.used) continue;
    for (int j = 0; j < c.count; j++) {
      Msg& m = c.ring[j];
      if (m.outgoing && m.ack == ack && m.status == ST_SENDING) {
        m.status = ST_DELIVERED;
        m.ack = 0;
        return;
      }
    }
  }
}

void ChatStore::expireSends(uint32_t now_millis) {
  for (int i = 0; i < CHAT_MAX_CONVS; i++) {
    Conv& c = _convs[i];
    if (!c.used) continue;
    for (int j = 0; j < c.count; j++) {
      Msg& m = c.ring[j];
      if (m.outgoing && m.status == ST_SENDING && m.deadline != 0 &&
          (int32_t)(now_millis - m.deadline) > 0) {
        m.status = ST_FAILED;
      }
    }
  }
}

Msg* ChatStore::msgAt(Conv* c, int display_idx) {
  if (!c || display_idx < 0 || display_idx >= c->count) return nullptr;
  // oldest held is at (head - count + 1); display_idx 0 = oldest
  int idx = (c->head - (c->count - 1) + display_idx + CHAT_MSGS_PER_CONV * 4) % CHAT_MSGS_PER_CONV;
  return &c->ring[idx];
}

}  // namespace chat
