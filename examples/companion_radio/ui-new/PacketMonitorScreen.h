#pragma once

#include <helpers/ui/UIScreen.h>
#include <stdint.h>

class UITask;

// Decoded (non-encrypted) fields of a received packet. The payload itself is
// encrypted, so only its length is kept -- never its bytes.
struct PktInfo {
  uint32_t t;       // capture time (RTC seconds)
  int8_t   snr_q;   // SNR * 4
  int8_t   rssi;    // dBm
  uint8_t  route;   // ROUTE_TYPE_* (0..3)
  uint8_t  ptype;   // PAYLOAD_TYPE_* (0..15)
  uint8_t  ver;     // payload version (0..3)
  uint8_t  hops;    // path hash count
  uint8_t  hashsz;  // path hash size
  bool     has_tc;  // transport codes present
  uint16_t tc0, tc1;
  uint16_t plen;    // (encrypted) payload length
  uint16_t len;     // total on-air length
};

// Live monitor of received packets, decoded into readable fields (no hex dump).
class PacketMonitorScreen : public UIScreen {
  UITask* _task;
  uint32_t _now;
  static const int CAP = 48;
  PktInfo _ring[CAP];
  int _count, _head, _scroll_top, _sel;
  int _press_y, _last_y;
  bool _moved, _pressing;
  bool _detail;       // showing the full-field detail view
  int _detail_ring;   // ring index of the packet being detailed

  void openDetail(int displayPos);
  int renderList(DisplayDriver& d);
  int renderDetail(DisplayDriver& d);

public:
  PacketMonitorScreen(UITask* task)
      : _task(task), _now(0), _count(0), _head(0), _scroll_top(0), _sel(0),
        _press_y(0), _last_y(0), _moved(false), _pressing(false),
        _detail(false), _detail_ring(0) {}

  void setNow(uint32_t now) { _now = now; }
  void reset() { _scroll_top = 0; _sel = 0; _detail = false; }
  // parse a raw received frame and store its decoded fields (newest first)
  void addRaw(uint32_t now, float snr, float rssi, const uint8_t* raw, int len);

  int render(DisplayDriver& display) override;
  bool handleInput(char c) override;
  bool handleTouch(int x, int y, TouchEvent ev) override;
};
