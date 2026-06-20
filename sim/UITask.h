#pragma once

// Desktop stand-in for the firmware UITask, providing exactly the methods the
// real SettingsScreen.cpp / SettingEditScreen call. This file deliberately
// shadows the firmware's UITask.h during the simulator build (see build.sh),
// so the real screen sources compile against this lightweight host shim instead
// of the MeshCore/Arduino stack.
#include <helpers/ui/UIScreen.h>
#include "SettingsModel.h"
#include "SettingsScreen.h"
#include "SimDisplay.h"
#include <string.h>
#include <SDL.h>

class UITask {
  DisplayDriver* _disp;
  SettingsListScreen* _list;
  SettingEditScreen* _edit;
  UIScreen* _curr;
  char _alert[80];
  Uint32 _alert_expiry;

public:
  explicit UITask(DisplayDriver* d) : _disp(d), _curr(nullptr), _alert_expiry(0) {
    _alert[0] = 0;
    _list = new SettingsListScreen(this);
    _edit = new SettingEditScreen(this);
    _list->showRoot();
    _curr = _list;
  }

  DisplayDriver* getDisplay() { return _disp; }
  void gotoHomeScreen() { _list->showRoot(); _curr = _list; }
  void gotoSettings() { _list->showRoot(); _curr = _list; }
  void editSetting(const Setting* s) { _edit->begin(s, /*use_osk=*/true); _curr = _edit; }
  void closeSettingEdit() { _curr = _list; }
  void showAlert(const char* t, int ms) {
    strncpy(_alert, t, sizeof(_alert) - 1);
    _alert[sizeof(_alert) - 1] = 0;
    _alert_expiry = SDL_GetTicks() + ms;
  }

  void onKey(char c) { if (_curr) _curr->handleInput(c); }
  void onTouch(int x, int y, TouchEvent ev) { if (_curr) _curr->handleTouch(x, y, ev); }

  void render() {
    if (!_curr) return;
    _disp->startFrame();
    _curr->render(*_disp);
    if (SDL_GetTicks() < _alert_expiry) {
      int w = _disp->width(), h = _disp->height();
      _disp->setColor(DisplayDriver::DARK);
      _disp->fillRect(w / 8, h / 3, w * 3 / 4, h / 5);
      _disp->setColor(DisplayDriver::LIGHT);
      _disp->drawRect(w / 8, h / 3, w * 3 / 4, h / 5);
      _disp->setTextSize(1);
      _disp->drawTextCentered(w / 2, h / 3 + h / 10 - 4, _alert);
    }
    _disp->endFrame();
  }
};
