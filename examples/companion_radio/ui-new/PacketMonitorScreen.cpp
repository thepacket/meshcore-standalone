#include "PacketMonitorScreen.h"
#include "UITask.h"
#include "UIStyle.h"
#include <stdio.h>
#include <string.h>

using namespace uistyle;

namespace {
// short codes so a full line fits the narrow screen
const char* ptypeName(uint8_t t) {
  switch (t) {
    case 0x00: return "REQ";
    case 0x01: return "RSP";
    case 0x02: return "TXT";
    case 0x03: return "ACK";
    case 0x04: return "ADV";
    case 0x05: return "GTX";   // group text
    case 0x06: return "GDA";   // group data
    case 0x07: return "ANR";   // anon req
    case 0x08: return "PTH";   // path
    case 0x09: return "TRC";   // trace
    case 0x0A: return "MPT";   // multipart
    case 0x0B: return "CTL";   // control
    case 0x0F: return "RAW";
    default: return "?";
  }
}
const char* routeName(uint8_t r) {
  switch (r) {
    case 0x00: return "TFL";   // transport-flood
    case 0x01: return "FLD";   // flood
    case 0x02: return "DIR";   // direct
    case 0x03: return "TDI";   // transport-direct
  }
  return "?";
}
// full readable names for the detail view
const char* ptypeFull(uint8_t t) {
  switch (t) {
    case 0x00: return "Request";
    case 0x01: return "Response";
    case 0x02: return "Text message";
    case 0x03: return "Ack";
    case 0x04: return "Advert";
    case 0x05: return "Group text";
    case 0x06: return "Group data";
    case 0x07: return "Anon request";
    case 0x08: return "Path";
    case 0x09: return "Trace";
    case 0x0A: return "Multipart";
    case 0x0B: return "Control";
    case 0x0F: return "Raw custom";
    default: return "Unknown";
  }
}
const char* routeFull(uint8_t r) {
  switch (r) {
    case 0x00: return "Transport flood";
    case 0x01: return "Flood";
    case 0x02: return "Direct";
    case 0x03: return "Transport direct";
  }
  return "Unknown";
}
void ageStr(uint32_t now, uint32_t t, char* b, size_t n) {
  uint32_t s = (now >= t) ? now - t : 0;
  if (s < 60) snprintf(b, n, "%us", (unsigned)s);
  else if (s < 3600) snprintf(b, n, "%um", (unsigned)(s / 60));
  else snprintf(b, n, "%uh", (unsigned)(s / 3600));
}
// SNR as "d.d" without relying on %f (some embedded printf lack float support)
void snrStr(int8_t snr_q, char* b, size_t n) {
  int q = snr_q;
  const char* sign = (q < 0) ? "-" : "";
  int a = q < 0 ? -q : q;
  snprintf(b, n, "%s%d.%d", sign, a / 4, (a % 4) * 25 / 10);  // quarter -> .0/.2/.5/.7
}
int entryH(DisplayDriver& d) { return big(d) ? 13 : 11; }  // one line per packet
}  // namespace

void PacketMonitorScreen::addRaw(uint32_t now, float snr, float rssi, const uint8_t* raw, int len) {
  if (len < 1) return;
  PktInfo p;
  memset(&p, 0, sizeof(p));
  p.t = now;
  p.snr_q = (int8_t)(snr * 4);
  p.rssi = (int8_t)rssi;
  p.len = (uint16_t)len;

  uint8_t header = raw[0];
  p.route = header & 0x03;
  p.ptype = (header >> 2) & 0x0F;
  p.ver = (header >> 6) & 0x03;

  int i = 1;
  p.has_tc = (p.route == 0x00 || p.route == 0x03);  // TRANSPORT_FLOOD / TRANSPORT_DIRECT
  if (p.has_tc && i + 4 <= len) {
    p.tc0 = (uint16_t)(raw[i] | (raw[i + 1] << 8));
    p.tc1 = (uint16_t)(raw[i + 2] | (raw[i + 3] << 8));
    i += 4;
  }
  if (i < len) {
    uint8_t pl = raw[i++];
    p.hashsz = (pl >> 6) + 1;
    p.hops = pl & 63;
    i += p.hops * p.hashsz;  // skip the path hashes
  }
  p.plen = (i <= len) ? (uint16_t)(len - i) : 0;

  _head = (_count == 0) ? 0 : (_head + 1) % CAP;
  _ring[_head] = p;
  if (_count < CAP) _count++;
}

void PacketMonitorScreen::openDetail(int displayPos) {
  if (displayPos < 0 || displayPos >= _count) return;
  _detail_ring = (_head - displayPos + CAP) % CAP;
  _detail = true;
}

int PacketMonitorScreen::render(DisplayDriver& d) {
  return _detail ? renderDetail(d) : renderList(d);
}

int PacketMonitorScreen::renderList(DisplayDriver& d) {
  int W = d.width(), hH = headerH(d), eh = entryH(d);
  drawHeaderBar(d, "Packets", true);

  if (_count == 0) {
    col(d, C_LABEL);
    d.drawTextCentered(W / 2, d.height() / 2, "Listening...");
    return 1000;
  }

  int vis = (d.height() - hH) / eh;
  if (vis < 1) vis = 1;
  if (_sel < 0) _sel = 0;
  if (_sel > _count - 1) _sel = _count - 1;
  if (_sel < _scroll_top) _scroll_top = _sel;
  if (_sel >= _scroll_top + vis) _scroll_top = _sel - vis + 1;
  int maxTop = (_count > vis) ? _count - vis : 0;
  if (_scroll_top > maxTop) _scroll_top = maxTop;
  if (_scroll_top < 0) _scroll_top = 0;

  bool overflow = _count > vis;
  int right = W - (overflow ? 6 : 0);

  char line[96], age[12], snr[12];
  for (int row = 0; row < vis; row++) {
    int pos = _scroll_top + row;
    if (pos >= _count) break;
    int idx = (_head - pos + CAP) % CAP;
    const PktInfo& p = _ring[idx];
    int y = hH + row * eh;
    bool selected = (pos == _sel);

    col(d, selected ? C_CARDSEL : C_CARD);
    d.fillRect(0, y, W, eh - 1);
    col(d, C_DIV);
    d.fillRect(0, y + eh - 1, W, 1);
    if (selected) { col(d, C_ACCENT); d.fillRect(0, y, 3, eh - 1); }

    snrStr(p.snr_q, snr, sizeof(snr));
    ageStr(_now, p.t, age, sizeof(age));

    if (big(d)) {
      int k = snprintf(line, sizeof(line), "ty:%s rt:%s h:%u pl:%u rs:%d sn:%s ag:%s",
                       ptypeName(p.ptype), routeName(p.route), p.hops, p.plen, p.rssi, snr, age);
      if (p.has_tc && k > 0 && k < (int)sizeof(line))
        snprintf(line + k, sizeof(line) - k, " tc:%u", p.tc0);
    } else {
      snprintf(line, sizeof(line), "%s %s h:%u rs:%d", ptypeName(p.ptype), routeName(p.route),
               p.hops, p.rssi);
    }
    col(d, C_VALUE);
    d.drawTextEllipsized(5, y + (eh - 8) / 2, right - 5, line);
  }

  if (overflow) {
    int trackX = W - 4, trackY = hH, trackH = vis * eh;
    col(d, C_DIV);
    d.fillRect(trackX, trackY, 2, trackH);
    int thumbH = trackH * vis / _count;
    if (thumbH < 8) thumbH = 8;
    int denom = maxTop > 0 ? maxTop : 1;
    int thumbY = trackY + (trackH - thumbH) * _scroll_top / denom;
    col(d, C_ACCENT);
    d.fillRect(trackX, thumbY, 2, thumbH);
  }
  return 1000;  // refresh ~1s so ages tick
}

int PacketMonitorScreen::renderDetail(DisplayDriver& d) {
  int W = d.width(), hH = headerH(d);
  drawHeaderBar(d, "Packet", true);
  const PktInfo& p = _ring[_detail_ring];

  char snr[12], age[12], val[24];
  snrStr(p.snr_q, snr, sizeof(snr));
  ageStr(_now, p.t, age, sizeof(age));

  int lh = big(d) ? 19 : 11;
  int y = hH + 1;
  auto row = [&](const char* label, const char* value) {
    col(d, C_CARD);
    d.fillRect(0, y, W, lh - 1);
    col(d, C_LABEL);
    d.setCursor(6, y + (lh - 8) / 2);
    d.print(label);
    col(d, C_VALUE);
    d.drawTextRightAlign(W - 6, y + (lh - 8) / 2, value);
    col(d, C_DIV);
    d.fillRect(0, y + lh - 1, W, 1);
    y += lh;
  };

  row("Type", ptypeFull(p.ptype));
  row("Route", routeFull(p.route));
  snprintf(val, sizeof(val), "%u", p.ver);    row("Version", val);
  snprintf(val, sizeof(val), "%u", p.hops);   row("Hops", val);
  snprintf(val, sizeof(val), "%u", p.hashsz); row("Hash size", val);
  snprintf(val, sizeof(val), "%u B", p.plen); row("Payload", val);
  snprintf(val, sizeof(val), "%u B", p.len);  row("Total", val);
  if (p.has_tc) { snprintf(val, sizeof(val), "%u / %u", p.tc0, p.tc1); row("Transport", val); }
  snprintf(val, sizeof(val), "%d dBm", p.rssi); row("RSSI", val);
  snprintf(val, sizeof(val), "%s dB", snr);     row("SNR", val);
  row("Age", age);
  return 1000;
}

bool PacketMonitorScreen::handleInput(char c) {
  if (_detail) {
    if (c == KEY_CANCEL || c == KEY_LEFT || c == KEY_ENTER || c == KEY_SELECT) _detail = false;
    return true;
  }
  if (c == KEY_UP || c == KEY_PREV) { if (_sel > 0) _sel--; return true; }
  if (c == KEY_DOWN || c == KEY_NEXT) { if (_sel < _count - 1) _sel++; return true; }
  if (c == KEY_ENTER || c == KEY_SELECT) { openDetail(_sel); return true; }
  if (c == KEY_CANCEL || c == KEY_LEFT) { _task->gotoHomeScreen(); return true; }
  return false;
}

bool PacketMonitorScreen::handleTouch(int x, int y, TouchEvent ev) {
  DisplayDriver* d = _task->getDisplay();
  if (!d) return false;
  int hH = headerH(*d), eh = entryH(*d);

  if (_detail) {
    if (ev == TouchEvent::release) _detail = false;  // tap anywhere returns to the list
    return true;
  }
  if (ev == TouchEvent::press) {
    _press_y = y; _last_y = y; _moved = false; _pressing = true;
    return true;
  }
  if (ev == TouchEvent::move) {
    if (!_pressing) return false;
    while (y - _last_y >= eh) { if (_scroll_top > 0) _scroll_top--; _last_y += eh; _moved = true; }
    while (_last_y - y >= eh) { _scroll_top++; _last_y -= eh; _moved = true; }
    return true;
  }
  if (ev == TouchEvent::release) {
    _pressing = false;
    if (_moved) return true;
    if (y < hH) { _task->gotoHomeScreen(); return true; }  // tap header = back
    int pos = _scroll_top + (y - hH) / eh;
    if (pos >= 0 && pos < _count) { _sel = pos; openDetail(pos); }  // tap row = detail
    return true;
  }
  return false;
}
