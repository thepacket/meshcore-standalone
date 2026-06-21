#include "ChatHomeScreen.h"
#include "UITask.h"
#include "UIStyle.h"
#include <stdio.h>
#include <string.h>

using namespace uistyle;
using namespace chat;

namespace { int entryH(DisplayDriver& d) { return big(d) ? 30 : 14; }
            int tabH(DisplayDriver& d)   { return big(d) ? 20 : 12; } }

void ChatHomeScreen::addChannel(int idx, const char* title) {
  if (_nch >= MAXE) return;
  Entry& e = _ch[_nch++];
  e.is_channel = true; e.channel_idx = idx;
  strncpy(e.title, title, sizeof(e.title) - 1); e.title[sizeof(e.title) - 1] = 0;
}

void ChatHomeScreen::addDm(const uint8_t* peer6, const char* title) {
  if (_ndm >= MAXE) return;
  Entry& e = _dm[_ndm++];
  e.is_channel = false; memcpy(e.peer, peer6, 6);
  strncpy(e.title, title, sizeof(e.title) - 1); e.title[sizeof(e.title) - 1] = 0;
}

ChatHomeScreen::Entry* ChatHomeScreen::curList(int& n) {
  if (_tab == 0) { n = _nch; return _ch; }
  n = _ndm; return _dm;
}

Conv* ChatHomeScreen::convFor(const Entry& e) {
  if (!_store) return nullptr;
  return e.is_channel ? _store->findChannel(e.channel_idx) : _store->findDm(e.peer);
}

void ChatHomeScreen::open(const Entry& e) {
  _task->openConversation(e.is_channel, e.channel_idx, e.peer, e.title);
}

int ChatHomeScreen::render(DisplayDriver& d) {
  int W = d.width(), hH = headerH(d), th = tabH(d), eh = entryH(d);
  drawHeaderBar(d, "Chat", true);

  // tab bar
  int ty = hH;
  const char* labels[2] = {"Channels", "DMs"};
  for (int t = 0; t < 2; t++) {
    int tx = t * (W / 2);
    col(d, _tab == t ? C_CARDSEL : C_CARD);
    d.fillRect(tx, ty, W / 2, th);
    col(d, _tab == t ? C_ACCENT : C_LABEL);
    d.setTextSize(1);
    d.drawTextCentered(tx + W / 4, ty + (th - 8) / 2, labels[t]);
    if (_tab == t) { col(d, C_ACCENT); d.fillRect(tx, ty + th - 2, W / 2, 2); }
  }
  col(d, C_DIV); d.fillRect(0, ty + th, W, 1);

  int listY = hH + th + 1;
  col(d, C_HEADER); d.fillRect(0, listY, W, d.height() - listY);

  int n; Entry* list = curList(n);
  int& sel = _sel[_tab]; int& top = _top[_tab];

  if (n == 0) {
    col(d, C_LABEL);
    d.drawTextCentered(W / 2, listY + (d.height() - listY) / 2 - 4,
                       _tab == 0 ? "No channels" : "No contacts");
    return 1000;
  }

  int vis = (d.height() - listY) / eh;
  if (vis < 1) vis = 1;
  if (sel < 0) sel = 0; if (sel > n - 1) sel = n - 1;
  if (sel < top) top = sel;
  if (sel >= top + vis) top = sel - vis + 1;
  if (top < 0) top = 0;

  char preview[64];
  for (int row = 0; row < vis; row++) {
    int pos = top + row;
    if (pos >= n) break;
    Entry& e = list[pos];
    int y = listY + row * eh;
    bool seld = (pos == sel);
    col(d, seld ? C_CARDSEL : C_CARD);
    d.fillRect(0, y, W, eh - 1);
    col(d, C_DIV); d.fillRect(0, y + eh - 1, W, 1);
    if (seld) { col(d, C_ACCENT); d.fillRect(0, y, 3, eh - 1); }

    Conv* c = convFor(e);
    int unread = c ? c->unread : 0;

    // unread badge (right)
    int rightLimit = W - 6;
    if (unread > 0) {
      char b[6]; snprintf(b, sizeof(b), "%d", unread > 99 ? 99 : unread);
      int bw = d.getTextWidth(b) + 8;
      int bx = W - bw - 4, by = y + 4, bh = 12;
      col(d, C_BADGE); d.fillRect(bx, by, bw, bh);
      col(d, C_VALUE); d.drawTextCentered(bx + bw / 2, by + 2, b);
      rightLimit = bx - 4;
    }

    if (big(d)) {
      col(d, C_VALUE);
      d.drawTextEllipsized(6, y + 3, rightLimit - 6, e.title);
      // preview
      preview[0] = 0;
      if (c && c->count > 0) {
        Msg* m = _store->msgAt(c, c->count - 1);
        if (m) {
          if (m->outgoing) snprintf(preview, sizeof(preview), "You: %s", m->text);
          else if (m->sender[0]) snprintf(preview, sizeof(preview), "%s: %s", m->sender, m->text);
          else snprintf(preview, sizeof(preview), "%s", m->text);
        }
      }
      col(d, C_LABEL);
      d.drawTextEllipsized(6, y + 16, W - 10, preview);
    } else {
      col(d, C_VALUE);
      d.drawTextEllipsized(6, y + (eh - 8) / 2, rightLimit - 6, e.title);
    }
  }
  return 1000;
}

bool ChatHomeScreen::handleInput(char c) {
  int n; Entry* list = curList(n);
  int& sel = _sel[_tab];
  if (c == KEY_LEFT) { if (_tab == 1) { _tab = 0; } else { _task->gotoHomeScreen(); } return true; }
  if (c == KEY_RIGHT) { _tab = 1; return true; }
  if (c == KEY_UP || c == KEY_PREV) { if (sel > 0) sel--; return true; }
  if (c == KEY_DOWN || c == KEY_NEXT) { if (sel < n - 1) sel++; return true; }
  if (c == KEY_ENTER || c == KEY_SELECT) { if (n > 0) open(list[sel]); return true; }
  if (c == KEY_CANCEL) { _task->gotoHomeScreen(); return true; }
  return false;
}

bool ChatHomeScreen::handleTouch(int x, int y, TouchEvent ev) {
  DisplayDriver* d = _task->getDisplay();
  if (!d) return false;
  int W = d->width(), hH = headerH(*d), th = tabH(*d), eh = entryH(*d);
  int listY = hH + th + 1;
  int& sel = _sel[_tab]; int& top = _top[_tab];

  if (ev == TouchEvent::press) { _last_y = y; _moved = false; _pressing = true; return true; }
  if (ev == TouchEvent::move) {
    if (!_pressing || y < listY) return _pressing;
    while (y - _last_y >= eh) { if (top > 0) top--; _last_y += eh; _moved = true; }
    while (_last_y - y >= eh) { top++; _last_y -= eh; _moved = true; }
    return true;
  }
  if (ev == TouchEvent::release) {
    _pressing = false;
    if (_moved) return true;
    if (y < hH) { _task->gotoHomeScreen(); return true; }     // header = back home
    if (y < hH + th) { _tab = (x < W / 2) ? 0 : 1; return true; }  // tab bar
    int n; Entry* list = curList(n);
    int pos = top + (y - listY) / eh;
    if (pos >= 0 && pos < n) { sel = pos; open(list[pos]); }
    return true;
  }
  return false;
}
