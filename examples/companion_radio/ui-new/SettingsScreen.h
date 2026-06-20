#pragma once

#include <helpers/ui/UIScreen.h>
#include "SettingsModel.h"
#include "OnScreenKeyboard.h"

class UITask;

// Scrollable list of categories (root) or settings within a category.
// Works with touch (tap to select/open, drag to scroll) and keycodes
// (UP/DOWN/ENTER/CANCEL). BOOL settings toggle inline; others open the editor.
class SettingsListScreen : public UIScreen {
  UITask* _task;
  const SettingsGroup* _group;  // NULL => show root categories
  int _sel;
  int _scroll_top;
  // touch drag tracking
  int _press_y, _last_y;
  bool _moved, _pressing;

  int itemCount() const;
  void activate(int idx);
  void scrollToSel(DisplayDriver& d);

public:
  SettingsListScreen(UITask* task)
      : _task(task), _group(nullptr), _sel(0), _scroll_top(0),
        _press_y(0), _last_y(0), _moved(false), _pressing(false) {}

  void showRoot() { _group = nullptr; _sel = 0; _scroll_top = 0; }

  int render(DisplayDriver& display) override;
  bool handleInput(char c) override;
  bool handleTouch(int x, int y, TouchEvent ev) override;
};

// Edits a single Setting. Renders type-appropriate controls (toggle, +/- for
// numeric/enum, on-screen keyboard for strings, confirm for actions), driven by
// touch or keycodes. Commits via the Setting's setter (which persists + applies).
class SettingEditScreen : public UIScreen {
  UITask* _task;
  const Setting* _s;
  char _sbuf[64];    // text-entry working buffer (STRING/INT/FLOAT)
  int _slen;
  OnScreenKeyboard _kb;
  bool _use_osk;     // render the on-screen keyboard for text entry

  void commit();

public:
  SettingEditScreen(UITask* task)
      : _task(task), _s(nullptr), _slen(0), _use_osk(true) {}

  void begin(const Setting* s, bool use_osk);

  int render(DisplayDriver& display) override;
  bool handleInput(char c) override;
  bool handleTouch(int x, int y, TouchEvent ev) override;
};
