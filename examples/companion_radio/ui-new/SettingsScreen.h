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
  int32_t _ival;     // working value for BOOL/ENUM/INT
  float _fval;       // working value for FLOAT
  char _sbuf[64];    // working value for STRING
  int _slen;
  OnScreenKeyboard _kb;
  bool _use_osk;     // render on-screen keyboard for STRING entry

  void adjust(int dir);
  void commit();
  int enumIndex() const;

public:
  SettingEditScreen(UITask* task)
      : _task(task), _s(nullptr), _ival(0), _fval(0), _slen(0), _use_osk(true) {}

  void begin(const Setting* s, bool use_osk);

  int render(DisplayDriver& display) override;
  bool handleInput(char c) override;
  bool handleTouch(int x, int y, TouchEvent ev) override;
};
