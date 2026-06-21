#include "ConversationScreen.h"
#include "UITask.h"
#include "UIStyle.h"
#include "RichText.h"
#include "EmojiGlyphs.h"
#include <stdio.h>
#include <string.h>

#ifdef HAVE_QRCODEGEN
  #include "qrcodegen.h"
#endif

using namespace uistyle;
using namespace chat;

namespace {
const int PAD = 4;
int lineH(DisplayDriver& d) { return big(d) ? 12 : 10; }

const RGB NAME_COLORS[] = {
  {63, 199, 232}, {120, 220, 140}, {236, 180, 80},
  {220, 130, 200}, {130, 170, 240}, {235, 130, 110},
};
const int N_NAME_COLORS = sizeof(NAME_COLORS) / sizeof(NAME_COLORS[0]);
const RGB& nameColor(const char* s) {
  uint32_t h = 5381;
  for (const char* p = s; *p; p++) h = ((h << 5) + h) + (uint8_t)*p;
  return NAME_COLORS[h % N_NAME_COLORS];
}
const char* statusText(uint8_t st) {
  switch (st) {
    case ST_SENDING:   return "sending";
    case ST_SENT:      return "sent";
    case ST_DELIVERED: return "delivered";
    case ST_FAILED:    return "failed";
  }
  return "";
}
// find a URL in `s`; copies it into out (cap n). returns true if found.
bool findUrl(const char* s, char* out, int n) {
  const char* u = strstr(s, "http://");
  if (!u) u = strstr(s, "https://");
  if (!u) u = strstr(s, "www.");
  if (!u) return false;
  int i = 0;
  while (u[i] && u[i] != ' ' && u[i] != '\n' && i < n - 1) { out[i] = u[i]; i++; }
  out[i] = 0;
  return i > 0;
}
// distinct emoji glyphs for the picker (dedup repeated shortcodes by bits ptr)
struct PickEntry { const char* code; const uint8_t* bits; };
int buildPicker(PickEntry* out, int maxn) {
  int n = 0;
  for (int i = 0; i < emoji::SHORTCODE_COUNT && n < maxn; i++) {
    bool dup = false;
    for (int j = 0; j < n; j++) if (out[j].bits == emoji::SHORTCODES[i].bits) { dup = true; break; }
    if (dup) continue;
    out[n].code = emoji::SHORTCODES[i].code;
    out[n].bits = emoji::SHORTCODES[i].bits;
    n++;
  }
  return n;
}
}  // namespace

int ConversationScreen::composeBarH(DisplayDriver& d) { return big(d) ? 20 : 12; }
int ConversationScreen::replyStripH(DisplayDriver& d) { return _reply[0] ? (big(d) ? 14 : 10) : 0; }
int ConversationScreen::oskHeight(DisplayDriver& d) {
  return (_osk_open || _emoji_open) ? (d.height() * 48 / 100) : 0;
}
int ConversationScreen::threadBottom(DisplayDriver& d) {
  return d.height() - oskHeight(d) - composeBarH(d) - replyStripH(d);
}

int ConversationScreen::render(DisplayDriver& d) {
  if (_qr_open) { drawQR(d); return 1000; }

  int W = d.width(), H = d.height(), hH = headerH(d), lh = lineH(d);
  drawHeaderBar(d, _title, true);
  int tTop = hH;
  int tBot = threadBottom(d);
  col(d, C_HEADER); d.fillRect(0, tTop, W, tBot - tTop);

  int maxBubbleInner = W * 72 / 100 - 2 * PAD;
  int n = _conv ? _conv->count : 0;

  auto orderedMsg = [&](int idx) -> Msg* {
    int ri = (_conv->head - (_conv->count - 1) + idx + CHAT_MSGS_PER_CONV * 4) % CHAT_MSGS_PER_CONV;
    return &_conv->ring[ri];
  };
  auto msgHeight = [&](Msg* m) -> int {
    int senderLines = (!m->outgoing && m->sender[0]) ? 1 : 0;
    int tl = richtext::layout(d, 0, 0, maxBubbleInner, lh, m->text, C_VALUE, false).lines;
    int foot = m->outgoing ? lh : 0;
    return senderLines * lh + tl * lh + foot + 2 * PAD + 3;
  };

  int contentH = 0;
  for (int i = 0; i < n; i++) contentH += msgHeight(orderedMsg(i));

  int viewH = tBot - tTop;
  int maxScroll = contentH > viewH ? contentH - viewH : 0;
  if (_scroll < 0) _scroll = 0;
  if (_scroll > maxScroll) _scroll = maxScroll;
  int y = (contentH <= viewH) ? tTop : (tBot - contentH + _scroll);

  if (n == 0) {
    col(d, C_LABEL);
    d.drawTextCentered(W / 2, (tTop + tBot) / 2 - 4, "No messages yet");
  }

  for (int i = 0; i < n; i++) {
    Msg* m = orderedMsg(i);
    int mh = msgHeight(m);
    if (y + mh >= tTop && y < tBot) {
      richtext::Result rt = richtext::layout(d, 0, 0, maxBubbleInner, lh, m->text, C_VALUE, false);
      int senderW = (!m->outgoing && m->sender[0]) ? d.getTextWidth(m->sender) : 0;
      int bw = (rt.maxw > senderW ? rt.maxw : senderW) + 2 * PAD;
      if (bw > W * 72 / 100) bw = W * 72 / 100;
      int bx = m->outgoing ? (W - 4 - bw) : 4;
      int bh = mh - 3;
      col(d, m->outgoing ? C_CARDSEL : C_CARD);
      d.fillRect(bx, y, bw, bh);
      if (m->outgoing) { col(d, C_ACCENT); d.fillRect(bx, y, 2, bh); }

      int ty = y + PAD;
      if (!m->outgoing && m->sender[0]) {
        col(d, nameColor(m->sender));
        d.setCursor(bx + PAD, ty); d.print(m->sender);
        ty += lh;
      }
      richtext::layout(d, bx + PAD, ty, maxBubbleInner, lh, m->text, C_VALUE, true);
      if (m->outgoing) {
        int fy = ty + rt.lines * lh;
        col(d, m->status == ST_DELIVERED ? C_ACCENT : (m->status == ST_FAILED ? C_WARN : C_MUTED));
        d.drawTextRightAlign(bx + bw - PAD, fy, statusText(m->status));
      }
    }
    y += mh;
  }

  // reply strip (above compose)
  int oskH = oskHeight(d);
  int cbH = composeBarH(d);
  int rH = replyStripH(d);
  int cbY = H - oskH - cbH;
  if (rH > 0) {
    int ry = cbY - rH;
    col(d, C_CARD); d.fillRect(0, ry, W, rH);
    col(d, C_ACCENT); d.fillRect(0, ry, 3, rH);
    col(d, C_LABEL);
    char strip[80]; snprintf(strip, sizeof(strip), "reply: %s", _reply);
    d.drawTextEllipsized(8, ry + (rH - 8) / 2, W - 8 - 14, strip);
    col(d, C_WARN); d.drawTextRightAlign(W - 4, ry + (rH - 8) / 2, "x");  // cancel
  }

  // compose bar with an emoji button at the right
  int emojiBtnW = emoji::EMOJI_W + 8;
  col(d, C_HEADER); d.fillRect(0, cbY, W, cbH);
  col(d, C_DIV); d.fillRect(0, cbY, W, 1);
  int inputRight = W - emojiBtnW;
  if (_ilen > 0) { col(d, C_VALUE); d.drawTextEllipsized(6, cbY + (cbH - 8) / 2, inputRight - 10, _input); }
  else { col(d, C_MUTED); d.drawTextLeftAlign(6, cbY + (cbH - 8) / 2,
                                              _use_osk ? "Tap to type" : "Type a message"); }
  col(d, _emoji_open ? C_ACCENT : C_MUTED);
  d.drawXbm(W - emojiBtnW + 4, cbY + (cbH - emoji::EMOJI_H) / 2, emoji::g_smile,
            emoji::EMOJI_W, emoji::EMOJI_H);

  // overlay: emoji picker or on-screen keyboard
  if (_emoji_open) {
    int oy = H - oskH;
    col(d, C_CARD); d.fillRect(0, oy, W, oskH);
    col(d, C_ACCENT); d.fillRect(0, oy, W, 1);
    PickEntry pk[16]; int np = buildPicker(pk, 16);
    int cols = 8, cell = W / cols;
    for (int i = 0; i < np; i++) {
      int r = i / cols, c = i % cols;
      int gx = c * cell + (cell - emoji::EMOJI_W) / 2;
      int gy = oy + 6 + r * (emoji::EMOJI_H + 8);
      col(d, C_VALUE);
      d.drawXbm(gx, gy, pk[i].bits, emoji::EMOJI_W, emoji::EMOJI_H);
    }
  } else if (_osk_open) {
    int oy = H - oskH;
    _kb.render(d, 0, oy, W, oskH);
  }
  return 1000;
}

void ConversationScreen::drawQR(DisplayDriver& d) {
  int W = d.width(), H = d.height();
  col(d, C_HEADER); d.fillRect(0, 0, W, H);
#ifdef HAVE_QRCODEGEN
  uint8_t qr[qrcodegen_BUFFER_LEN_MAX], tmp[qrcodegen_BUFFER_LEN_MAX];
  if (qrcodegen_encodeText(_qr_url, tmp, qr, qrcodegen_Ecc_MEDIUM,
                           qrcodegen_VERSION_MIN, qrcodegen_VERSION_MAX,
                           qrcodegen_Mask_AUTO, true)) {
    int size = qrcodegen_getSize(qr);
    int avail = (H < W ? H : W) - 40;
    int scale = avail / size; if (scale < 1) scale = 1;
    int qpx = size * scale;
    int ox = (W - qpx) / 2, oy = 6;
    col(d, C_VALUE); d.fillRect(ox - 4, oy - 4, qpx + 8, qpx + 8);  // white quiet zone
    d.setColorRGB(0, 0, 0);
    for (int yy = 0; yy < size; yy++)
      for (int xx = 0; xx < size; xx++)
        if (qrcodegen_getModule(qr, xx, yy))
          d.fillRect(ox + xx * scale, oy + yy * scale, scale, scale);
    col(d, C_VALUE);
    d.drawTextCentered(W / 2, oy + qpx + 8, _qr_url);
    col(d, C_MUTED);
    d.drawTextCentered(W / 2, H - 12, "tap to close");
  } else
#endif
  {
    col(d, C_LABEL);
    d.drawTextCentered(W / 2, H / 2 - 4, "QR unavailable");
  }
}

void ConversationScreen::tapMessage(DisplayDriver& d, int tapY) {
  if (!_conv) return;
  int hH = headerH(d), lh = lineH(d), tBot = threadBottom(d);
  int maxBubbleInner = d.width() * 72 / 100 - 2 * PAD;
  int n = _conv->count;
  auto orderedMsg = [&](int idx) -> Msg* {
    int ri = (_conv->head - (_conv->count - 1) + idx + CHAT_MSGS_PER_CONV * 4) % CHAT_MSGS_PER_CONV;
    return &_conv->ring[ri];
  };
  auto msgHeight = [&](Msg* m) -> int {
    int senderLines = (!m->outgoing && m->sender[0]) ? 1 : 0;
    int tl = richtext::layout(d, 0, 0, maxBubbleInner, lh, m->text, C_VALUE, false).lines;
    return senderLines * lh + tl * lh + (m->outgoing ? lh : 0) + 2 * PAD + 3;
  };
  int contentH = 0;
  for (int i = 0; i < n; i++) contentH += msgHeight(orderedMsg(i));
  int viewH = tBot - hH;
  int y = (contentH <= viewH) ? hH : (tBot - contentH + _scroll);
  for (int i = 0; i < n; i++) {
    Msg* m = orderedMsg(i);
    int mh = msgHeight(m);
    if (tapY >= y && tapY < y + mh) {
      char url[128];
      if (findUrl(m->text, url, sizeof(url))) {
#ifdef HAVE_QRCODEGEN
        strncpy(_qr_url, url, sizeof(_qr_url) - 1); _qr_url[sizeof(_qr_url) - 1] = 0;
        _qr_open = true; _osk_open = _emoji_open = false;
        return;
#endif
      }
      // otherwise set up a reply quote
      if (m->sender[0]) snprintf(_reply, sizeof(_reply), "%s: %s", m->sender, m->text);
      else snprintf(_reply, sizeof(_reply), "%s", m->text);
      return;
    }
    y += mh;
  }
}

void ConversationScreen::send() {
  if (_ilen == 0 || !_conv) return;
  char full[180];
  if (_reply[0]) snprintf(full, sizeof(full), "> %s\n%s", _reply, _input);
  else snprintf(full, sizeof(full), "%s", _input);
  _task->sendChatText(_conv, full);
  _ilen = 0; _input[0] = 0; _reply[0] = 0;
  _scroll = 0;
}

bool ConversationScreen::handleInput(char c) {
  if (_qr_open) { if (c) _qr_open = false; return true; }
  if (c == KEY_ENTER || c == KEY_SELECT) { send(); return true; }
  if (c == KEY_BACKSPACE) { if (_ilen > 0) _input[--_ilen] = 0; return true; }
  if (c == KEY_UP || c == KEY_PREV) { _scroll += 14; return true; }
  if (c == KEY_DOWN || c == KEY_NEXT) { _scroll -= 14; if (_scroll < 0) _scroll = 0; return true; }
  if (c == KEY_CANCEL || c == KEY_LEFT) {
    if (_emoji_open) _emoji_open = false;
    else if (_reply[0]) _reply[0] = 0;
    else if (_ilen > 0) { _ilen = 0; _input[0] = 0; }
    else _task->gotoChat();
    return true;
  }
  if (c >= 32 && c < 127) {
    if (_ilen < (int)sizeof(_input) - 1) { _input[_ilen++] = c; _input[_ilen] = 0; }
    return true;
  }
  return false;
}

bool ConversationScreen::handleTouch(int x, int y, TouchEvent ev) {
  DisplayDriver* d = _task->getDisplay();
  if (!d) return false;
  int W = d->width(), H = d->height(), hH = headerH(*d);

  if (_qr_open) { if (ev == TouchEvent::release) _qr_open = false; return true; }

  int oskH = oskHeight(*d);
  int cbH = composeBarH(*d);
  int rH = replyStripH(*d);
  int cbY = H - oskH - cbH;
  int emojiBtnW = emoji::EMOJI_W + 8;

  // emoji picker taps
  if (_emoji_open && ev == TouchEvent::release) {
    int oy = H - oskH;
    if (y >= oy) {
      PickEntry pk[16]; int np = buildPicker(pk, 16);
      int cols = 8, cell = W / cols;
      int c = x / cell, r = (y - oy - 6) / (emoji::EMOJI_H + 8);
      int idx = r * cols + c;
      if (idx >= 0 && idx < np) {
        const char* code = pk[idx].code;
        int l = strlen(code);
        if (_ilen + l < (int)sizeof(_input) - 1) { memcpy(_input + _ilen, code, l); _ilen += l; _input[_ilen] = 0; }
      }
      return true;
    }
    if (y < hH || (y >= cbY && y < cbY + cbH)) { _emoji_open = false; return true; }
    return true;
  }

  if (_osk_open && ev == TouchEvent::release) {
    int oy = H - oskH;
    if (y >= oy) {
      char k = _kb.handleTap(x, y, 0, oy, W, oskH);
      if (k == OnScreenKeyboard::KEY_OK) send();
      else if (k == OnScreenKeyboard::KEY_BKSP) { if (_ilen > 0) _input[--_ilen] = 0; }
      else if (k >= 32 && k < 127) { if (_ilen < (int)sizeof(_input) - 1) { _input[_ilen++] = k; _input[_ilen] = 0; } }
      return true;
    }
    if (y < hH || (y >= cbY && y < cbY + cbH)) { _osk_open = false; return true; }
    return true;
  }

  if (ev == TouchEvent::press) { _last_y = y; _moved = false; _pressing = true; return true; }
  if (ev == TouchEvent::move) {
    if (!_pressing) return false;
    int dy = y - _last_y;
    if (dy > 2 || dy < -2) { _scroll += dy; _last_y = y; _moved = true; }
    return true;
  }
  if (ev == TouchEvent::release) {
    _pressing = false;
    if (_moved) return true;
    if (y < hH) { _task->gotoChat(); return true; }
    // reply-cancel hit area
    if (rH > 0) {
      int ry = cbY - rH;
      if (y >= ry && y < cbY && x > W - 18) { _reply[0] = 0; return true; }
    }
    if (y >= cbY && y < cbY + cbH) {           // compose bar
      if (x >= W - emojiBtnW) { _emoji_open = true; _osk_open = false; }
      else if (_use_osk) { _osk_open = true; _emoji_open = false; }
      return true;
    }
    int tBot = threadBottom(*d);
    if (y >= hH && y < tBot) tapMessage(*d, y);  // tap a bubble -> reply / QR
    return true;
  }
  return false;
}
