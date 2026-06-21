#include "ConversationScreen.h"
#include "UITask.h"
#include "UIStyle.h"
#include <stdio.h>
#include <string.h>

using namespace uistyle;
using namespace chat;

namespace {
const int PAD = 4;
int lineH(DisplayDriver& d) { return big(d) ? 12 : 10; }

// distinct colours for channel sender names (hash -> index)
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

// Lay out `text` wrapped to maxw. If draw, renders each line at x,y (y advances).
// Returns line count; *maxLineW (if given) gets the widest line's pixel width.
int layoutWrapped(DisplayDriver& d, int x, int y, int maxw, int lh,
                  const char* text, bool draw, int* maxLineW) {
  char line[160]; int ll = 0; int lines = 0, widest = 0;
  int spacew = d.getTextWidth(" ");
  const char* p = text;
  auto flush = [&]() {
    line[ll] = 0;
    if (ll > 0) {
      int w = d.getTextWidth(line);
      if (w > widest) widest = w;
      if (draw) { d.setCursor(x, y); d.print(line); }
      y += lh; lines++;
    }
    ll = 0;
  };
  while (*p) {
    // extract a word
    const char* ws = p;
    while (*p && *p != ' ') p++;
    int wl = p - ws;
    char word[160];
    if (wl > (int)sizeof(word) - 1) wl = sizeof(word) - 1;
    memcpy(word, ws, wl); word[wl] = 0;
    while (*p == ' ') p++;  // skip spaces

    int ww = d.getTextWidth(word);
    if (ww > maxw) {  // hard-split a too-long word char by char
      for (int i = 0; word[i]; i++) {
        char tmp[2] = {word[i], 0};
        char cand[162];
        memcpy(cand, line, ll); cand[ll] = word[i]; cand[ll + 1] = 0;
        if (d.getTextWidth(cand) > maxw && ll > 0) flush();
        line[ll++] = word[i];
      }
      continue;
    }
    // would the word fit on the current line?
    int curw = 0; { char t[160]; memcpy(t, line, ll); t[ll] = 0; curw = d.getTextWidth(t); }
    if (ll > 0 && curw + spacew + ww > maxw) flush();
    if (ll > 0) line[ll++] = ' ';
    memcpy(line + ll, word, strlen(word)); ll += strlen(word);
  }
  flush();
  if (maxLineW) *maxLineW = widest;
  if (lines == 0) lines = 1;  // empty -> still one line tall
  return lines;
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
}  // namespace

int ConversationScreen::composeBarH(DisplayDriver& d) { return big(d) ? 20 : 12; }
int ConversationScreen::oskHeight(DisplayDriver& d) { return _osk_open ? (d.height() * 48 / 100) : 0; }
int ConversationScreen::threadBottom(DisplayDriver& d) {
  return d.height() - oskHeight(d) - composeBarH(d);
}

int ConversationScreen::render(DisplayDriver& d) {
  int W = d.width(), H = d.height(), hH = headerH(d), lh = lineH(d);
  drawHeaderBar(d, _title, true);
  int tTop = hH;
  int tBot = threadBottom(d);
  col(d, C_HEADER); d.fillRect(0, tTop, W, tBot - tTop);

  int maxBubbleInner = W * 72 / 100 - 2 * PAD;
  int n = _conv ? _conv->count : 0;

  // ordered access oldest..newest within the ring
  auto orderedMsg = [&](int idx) -> Msg* {
    int ri = (_conv->head - (_conv->count - 1) + idx + CHAT_MSGS_PER_CONV * 4) % CHAT_MSGS_PER_CONV;
    return &_conv->ring[ri];
  };
  auto msgHeight = [&](Msg* m) -> int {
    int senderLines = (!m->outgoing && m->sender[0]) ? 1 : 0;
    int tl = layoutWrapped(d, 0, 0, maxBubbleInner, lh, m->text, false, nullptr);
    int foot = m->outgoing ? lh : 0;
    return senderLines * lh + tl * lh + foot + 2 * PAD + 3;  // +gap
  };

  // measure total content height
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
    if (y + mh >= tTop && y < tBot) {  // visible
      int innerW;
      layoutWrapped(d, 0, 0, maxBubbleInner, lh, m->text, false, &innerW);
      int senderW = (!m->outgoing && m->sender[0]) ? d.getTextWidth(m->sender) : 0;
      int bw = (innerW > senderW ? innerW : senderW) + 2 * PAD;
      if (bw > W * 72 / 100) bw = W * 72 / 100;
      int bx = m->outgoing ? (W - 4 - bw) : 4;
      int bh = mh - 3;  // minus gap
      col(d, m->outgoing ? C_CARDSEL : C_CARD);
      d.fillRect(bx, y, bw, bh);
      if (m->outgoing) { col(d, C_ACCENT); d.fillRect(bx, y, 2, bh); }

      int ty = y + PAD;
      if (!m->outgoing && m->sender[0]) {
        col(d, nameColor(m->sender));
        d.setCursor(bx + PAD, ty); d.print(m->sender);
        ty += lh;
      }
      col(d, C_VALUE);
      layoutWrapped(d, bx + PAD, ty, maxBubbleInner, lh, m->text, true, nullptr);
      if (m->outgoing) {
        int tl = layoutWrapped(d, 0, 0, maxBubbleInner, lh, m->text, false, nullptr);
        int fy = y + PAD + tl * lh;
        const char* st = statusText(m->status);
        col(d, m->status == ST_DELIVERED ? C_ACCENT : (m->status == ST_FAILED ? C_WARN : C_MUTED));
        d.drawTextRightAlign(bx + bw - PAD, fy, st);
      }
    }
    y += mh;
  }

  // compose bar
  int cbH = composeBarH(d);
  int cbY = H - oskHeight(d) - cbH;
  col(d, C_HEADER); d.fillRect(0, cbY, W, cbH);
  col(d, C_DIV); d.fillRect(0, cbY, W, 1);
  if (_ilen > 0) { col(d, C_VALUE); d.drawTextEllipsized(6, cbY + (cbH - 8) / 2, W - 12, _input); }
  else { col(d, C_MUTED); d.drawTextLeftAlign(6, cbY + (cbH - 8) / 2,
                                              _use_osk ? "Tap to type" : "Type a message"); }

  if (_osk_open) {
    int oy = H - oskHeight(d);
    _kb.render(d, 0, oy, W, oskHeight(d));
  }
  return 1000;
}

void ConversationScreen::send() {
  if (_ilen == 0 || !_conv) return;
  _task->sendChatText(_conv, _input);
  _ilen = 0; _input[0] = 0;
  _scroll = 0;  // jump to newest
}

bool ConversationScreen::handleInput(char c) {
  if (c == KEY_ENTER || c == KEY_SELECT) { send(); return true; }
  if (c == KEY_BACKSPACE) { if (_ilen > 0) _input[--_ilen] = 0; return true; }
  if (c == KEY_UP || c == KEY_PREV) { _scroll += 14; return true; }
  if (c == KEY_DOWN || c == KEY_NEXT) { _scroll -= 14; if (_scroll < 0) _scroll = 0; return true; }
  if (c == KEY_CANCEL || c == KEY_LEFT) {
    if (_ilen > 0) { _ilen = 0; _input[0] = 0; }
    else _task->gotoChat();
    return true;
  }
  if (c >= 32 && c < 127) {  // printable -> append
    if (_ilen < (int)sizeof(_input) - 1) { _input[_ilen++] = c; _input[_ilen] = 0; }
    return true;
  }
  return false;
}

bool ConversationScreen::handleTouch(int x, int y, TouchEvent ev) {
  DisplayDriver* d = _task->getDisplay();
  if (!d) return false;
  int W = d->width(), H = d->height(), hH = headerH(*d);
  int cbH = composeBarH(*d);
  int cbY = H - oskHeight(*d) - cbH;

  if (_osk_open) {
    int oy = H - oskHeight(*d);
    if (ev == TouchEvent::release && y >= oy) {
      char k = _kb.handleTap(x, y, 0, oy, W, oskHeight(*d));
      if (k == OnScreenKeyboard::KEY_OK) send();
      else if (k == OnScreenKeyboard::KEY_BKSP) { if (_ilen > 0) _input[--_ilen] = 0; }
      else if (k >= 32 && k < 127) { if (_ilen < (int)sizeof(_input) - 1) { _input[_ilen++] = k; _input[_ilen] = 0; } }
      return true;
    }
    if (ev == TouchEvent::release && y < hH) { _osk_open = false; return true; }  // header closes OSK
    if (ev == TouchEvent::release && y >= cbY && y < cbY + cbH) { _osk_open = false; return true; }
    return true;
  }

  if (ev == TouchEvent::press) { _last_y = y; _moved = false; _pressing = true; return true; }
  if (ev == TouchEvent::move) {
    if (!_pressing) return false;
    int dy = y - _last_y;
    if (dy > 2 || dy < -2) { _scroll += dy; _last_y = y; _moved = true; }  // drag down = older
    return true;
  }
  if (ev == TouchEvent::release) {
    _pressing = false;
    if (_moved) return true;
    if (y < hH) { _task->gotoChat(); return true; }            // header = back to list
    if (y >= cbY && y < cbY + cbH) {                            // compose bar
      if (_use_osk) _osk_open = true;
      return true;
    }
    return true;
  }
  return false;
}
