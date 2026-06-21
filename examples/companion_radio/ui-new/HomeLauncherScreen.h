#pragma once

#include <helpers/ui/UIScreen.h>
#include <stdint.h>

class UITask;

// Status data the launcher shows in its top/bottom bars. Filled by the task so
// the screen stays decoupled from the firmware/host specifics.
struct HomeStatus {
  const char* node_name;  // e.g. "Andy"
  const char* device;     // e.g. "TDeck+"
  const char* channel;    // current channel name, e.g. "Public"
  uint32_t    epoch;      // RTC seconds (0 = unknown)
  int         batt_pct;   // 0..100 (-1 = unknown)
  int         bars;       // signal strength 0..4
  int         msgcount;   // unread messages (badge on Chat)
};

// MeshOS-style icon-grid launcher: a 4-column tile grid with a top status bar
// (menu glyph, channel, clock) and a bottom status bar (node, device, signal,
// battery). Active tiles open their screen; future tiles are dimmed.
class HomeLauncherScreen : public UIScreen {
public:
  // tile actions; order matches the MeshOS grid (row-major, 4 cols x 3 rows)
  enum Action {
    A_CHAT, A_CONTACTS, A_REPEATERS, A_FINDER,
    A_HEARD, A_MAP, A_ADVERTISE, A_SETTINGS,
    A_TRACE, A_TERMINAL, A_NOISE, A_SIGNAL,
    A_COUNT
  };

private:
  UITask* _task;
  int _sel;            // selected tile (0..A_COUNT-1)
  bool _menu_open;     // hamburger menu overlay
  int _menu_sel;

  void activate(int tile);
  void activateMenu(int item);
  int  tileAt(int x, int y);          // -1 if none
  void drawTopBar(DisplayDriver& d, const HomeStatus& s);
  void drawBottomBar(DisplayDriver& d, const HomeStatus& s);
  void drawMenu(DisplayDriver& d);

public:
  HomeLauncherScreen(UITask* task) : _task(task), _sel(A_SETTINGS), _menu_open(false), _menu_sel(0) {}

  int render(DisplayDriver& display) override;
  bool handleInput(char c) override;
  bool handleTouch(int x, int y, TouchEvent ev) override;
};
