#include "SettingsScreen.h"
#include "UITask.h"
#include <stdio.h>
#include <string.h>

// ---- shared layout helpers ----
static const int HEADER_H = 16;
static int rowHeight(DisplayDriver& d) { return d.height() >= 160 ? 22 : 11; }
static int visibleRows(DisplayDriver& d) {
  int vr = (d.height() - HEADER_H) / rowHeight(d);
  return vr < 1 ? 1 : vr;
}
static bool inRect(int x, int y, int rx, int ry, int rw, int rh) {
  return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}

static void formatValue(const Setting& s, char* out, size_t n) {
  switch (s.type) {
    case ST_BOOL:
      snprintf(out, n, "%s", s.getInt() ? "On" : "Off");
      break;
    case ST_ENUM: {
      int32_t v = s.getInt();
      const char* lbl = "?";
      for (int i = 0; i < s.num_opts; i++) {
        if (s.opts[i].value == v) { lbl = s.opts[i].label; break; }
      }
      snprintf(out, n, "%s", lbl);
      break;
    }
    case ST_INT:
      snprintf(out, n, "%ld%s%s", (long)s.getInt(), (s.units && s.units[0]) ? " " : "",
               s.units ? s.units : "");
      break;
    case ST_FLOAT: {
      char nb[16];
      snprintf(nb, sizeof(nb), "%g", (double)s.getFloat());
      snprintf(out, n, "%s%s%s", nb, (s.units && s.units[0]) ? " " : "", s.units ? s.units : "");
      break;
    }
    case ST_STRING:
    case ST_INFO:
      snprintf(out, n, "%s", s.getStr() ? s.getStr() : "");
      break;
    case ST_ACTION:
      out[0] = 0;
      break;
  }
}

// =================== SettingsListScreen ===================
int SettingsListScreen::itemCount() const {
  return _group ? _group->count : SETTINGS_ROOT_COUNT;
}

int SettingsListScreen::render(DisplayDriver& d) {
  d.setTextSize(1);
  int n = itemCount();
  int vr = visibleRows(d);
  int rh = rowHeight(d);

  // keep selection visible
  if (_sel < _scroll_top) _scroll_top = _sel;
  if (_sel >= _scroll_top + vr) _scroll_top = _sel - vr + 1;
  if (_scroll_top > n - vr) _scroll_top = (n > vr) ? n - vr : 0;
  if (_scroll_top < 0) _scroll_top = 0;

  // header
  d.setColor(DisplayDriver::GREEN);
  d.setCursor(2, 2);
  d.print(_group ? _group->title : "Settings");
  d.setColor(DisplayDriver::LIGHT);
  d.drawRect(0, HEADER_H - 2, d.width(), 1);

  char namef[48], val[40], valf[44];
  for (int row = 0; row < vr; row++) {
    int idx = _scroll_top + row;
    if (idx >= n) break;
    int y = HEADER_H + row * rh;
    int ty = y + (rh - 8) / 2;
    bool selected = (idx == _sel);
    if (selected) {
      d.setColor(DisplayDriver::BLUE);
      d.fillRect(0, y, d.width(), rh - 1);
    }
    d.setColor(selected ? DisplayDriver::DARK : DisplayDriver::LIGHT);
    if (_group) {
      const Setting& s = _group->items[idx];
      d.translateUTF8ToBlocks(namef, s.label, sizeof(namef));
      d.setCursor(3, ty);
      d.print(namef);
      if (s.type == ST_ACTION) {
        strcpy(val, ">");
      } else {
        formatValue(s, val, sizeof(val));
      }
      d.translateUTF8ToBlocks(valf, val, sizeof(valf));
      d.drawTextRightAlign(d.width() - 3, ty, valf);
    } else {
      d.translateUTF8ToBlocks(namef, SETTINGS_ROOT[idx].title, sizeof(namef));
      d.setCursor(3, ty);
      d.print(namef);
      d.drawTextRightAlign(d.width() - 3, ty, ">");
    }
  }
  return 3000;
}

void SettingsListScreen::activate(int idx) {
  if (!_group) {
    _group = &SETTINGS_ROOT[idx];
    _sel = 0;
    _scroll_top = 0;
    return;
  }
  const Setting* s = &_group->items[idx];
  if (s->type == ST_BOOL) {
    if (s->setInt) s->setInt(s->getInt() ? 0 : 1);  // toggle inline (op persists)
    return;
  }
  if (s->type == ST_INFO) return;  // not interactive
  _task->editSetting(s);
}

bool SettingsListScreen::handleInput(char c) {
  int n = itemCount();
  if (c == KEY_UP || c == KEY_PREV) {
    if (_sel > 0) _sel--;
    return true;
  }
  if (c == KEY_DOWN || c == KEY_NEXT) {
    if (_sel < n - 1) _sel++;
    return true;
  }
  if (c == KEY_ENTER || c == KEY_SELECT) {
    activate(_sel);
    return true;
  }
  if (c == KEY_CANCEL || c == KEY_LEFT) {
    if (_group) showRoot();
    else _task->gotoHomeScreen();
    return true;
  }
  return false;
}

bool SettingsListScreen::handleTouch(int x, int y, TouchEvent ev) {
  DisplayDriver* d = _task->getDisplay();
  if (!d) return false;
  int rh = rowHeight(*d);
  int vr = visibleRows(*d);
  int n = itemCount();
  if (ev == TouchEvent::press) {
    _press_y = y; _last_y = y; _moved = false; _pressing = true;
    return true;
  }
  if (ev == TouchEvent::move) {
    if (!_pressing) return false;
    while (y - _last_y >= rh) { if (_scroll_top > 0) _scroll_top--; _last_y += rh; _moved = true; }
    while (_last_y - y >= rh) { if (_scroll_top < n - vr) _scroll_top++; _last_y -= rh; _moved = true; }
    return true;
  }
  if (ev == TouchEvent::release) {
    _pressing = false;
    if (_moved) return true;  // it was a scroll, not a tap
    if (y < HEADER_H) {
      if (_group) showRoot();
      else _task->gotoHomeScreen();
      return true;
    }
    int idx = _scroll_top + (y - HEADER_H) / rh;
    if (idx >= 0 && idx < n) { _sel = idx; activate(idx); }
    return true;
  }
  return false;
}

// =================== SettingEditScreen ===================
void SettingEditScreen::begin(const Setting* s, bool use_osk) {
  _s = s;
  _use_osk = use_osk;
  _kb.reset();
  _slen = 0;
  _sbuf[0] = 0;
  if (!s) return;
  switch (s->type) {
    case ST_BOOL:
    case ST_ENUM:
    case ST_INT:
      _ival = s->getInt ? s->getInt() : 0;
      break;
    case ST_FLOAT:
      _fval = s->getFloat ? s->getFloat() : 0;
      break;
    case ST_STRING: {
      const char* cur = s->getStr ? s->getStr() : "";
      strncpy(_sbuf, cur, sizeof(_sbuf) - 1);
      _sbuf[sizeof(_sbuf) - 1] = 0;
      _slen = strlen(_sbuf);
      break;
    }
    default:
      break;
  }
}

int SettingEditScreen::enumIndex() const {
  for (int i = 0; i < _s->num_opts; i++) {
    if (_s->opts[i].value == _ival) return i;
  }
  return 0;
}

void SettingEditScreen::adjust(int dir) {
  switch (_s->type) {
    case ST_BOOL:
      _ival = _ival ? 0 : 1;
      break;
    case ST_ENUM: {
      int i = (enumIndex() + dir + _s->num_opts) % _s->num_opts;
      _ival = _s->opts[i].value;
      break;
    }
    case ST_INT:
      _ival += dir * _s->istep;
      if (_ival < _s->imin) _ival = _s->imin;
      if (_ival > _s->imax) _ival = _s->imax;
      break;
    case ST_FLOAT:
      _fval += dir * _s->fstep;
      if (_fval < _s->fmin) _fval = _s->fmin;
      if (_fval > _s->fmax) _fval = _s->fmax;
      break;
    default:
      break;
  }
}

void SettingEditScreen::commit() {
  bool ok = true;
  switch (_s->type) {
    case ST_BOOL:
    case ST_ENUM:
    case ST_INT:
      ok = _s->setInt ? _s->setInt(_ival) : false;
      break;
    case ST_FLOAT:
      ok = _s->setFloat ? _s->setFloat(_fval) : false;
      break;
    case ST_STRING:
      ok = _s->setStr ? _s->setStr(_sbuf) : false;
      break;
    default:
      break;
  }
  _task->showAlert(ok ? "Saved" : "Invalid", 1000);
  _task->closeSettingEdit();
}

int SettingEditScreen::render(DisplayDriver& d) {
  if (!_s) return 1000;
  int W = d.width(), H = d.height();

  // header
  d.setTextSize(1);
  d.setColor(DisplayDriver::GREEN);
  d.setCursor(2, 2);
  d.print(_s->label);
  d.setColor(DisplayDriver::LIGHT);
  d.drawRect(0, HEADER_H - 2, W, 1);

  if (_s->type == ST_STRING) {
    // text box
    d.setColor(DisplayDriver::LIGHT);
    d.drawRect(4, HEADER_H + 2, W - 8, 20);
    char shown[68];
    snprintf(shown, sizeof(shown), "%s_", _sbuf);
    d.setColor(DisplayDriver::YELLOW);
    d.drawTextEllipsized(8, HEADER_H + 8, W - 16, shown);
    if (_use_osk) {
      int ky = HEADER_H + 26;
      _kb.render(d, 2, ky, W - 4, H - ky - 2);
    } else {
      d.setColor(DisplayDriver::LIGHT);
      d.drawTextCentered(W / 2, H / 2, "type on keyboard");
    }
    return 1000;
  }

  int bw = (int)(W * 0.28f), bh = (int)(H * 0.18f), by = (int)(H * 0.46f);
  int cw = (int)(W * 0.40f), cy = H - bh - 6;

  if (_s->type == ST_INFO) {
    char val[48];
    formatValue(*_s, val, sizeof(val));
    d.setColor(DisplayDriver::YELLOW);
    d.setTextSize(1);
    d.drawTextCentered(W / 2, H / 3, val);
    d.setColor(DisplayDriver::LIGHT);
    d.drawRect(W / 2 - cw / 2, cy, cw, bh);
    d.drawTextCentered(W / 2, cy + bh / 2 - 4, "Back");
    return 3000;
  }

  if (_s->type == ST_ACTION) {
    d.setColor(DisplayDriver::YELLOW);
    d.drawTextCentered(W / 2, H / 3, "Confirm?");
    d.setColor(DisplayDriver::LIGHT);
    d.drawRect(8, cy, cw, bh);
    d.drawTextCentered(8 + cw / 2, cy + bh / 2 - 4, "Cancel");
    d.drawRect(W - 8 - cw, cy, cw, bh);
    d.drawTextCentered(W - 8 - cw / 2, cy + bh / 2 - 4, "Confirm");
    return 3000;
  }

  // BOOL / ENUM / INT / FLOAT: value + [-]/[+] + Cancel/OK
  char val[40];
  formatValue(*_s, val, sizeof(val));
  // formatValue reads live getters; for the working value, override display:
  if (_s->type == ST_BOOL) snprintf(val, sizeof(val), "%s", _ival ? "On" : "Off");
  else if (_s->type == ST_ENUM) {
    const char* lbl = "?";
    for (int i = 0; i < _s->num_opts; i++) if (_s->opts[i].value == _ival) { lbl = _s->opts[i].label; break; }
    snprintf(val, sizeof(val), "%s", lbl);
  } else if (_s->type == ST_INT) {
    snprintf(val, sizeof(val), "%ld%s%s", (long)_ival, (_s->units && _s->units[0]) ? " " : "", _s->units ? _s->units : "");
  } else if (_s->type == ST_FLOAT) {
    char nb[16]; snprintf(nb, sizeof(nb), "%g", (double)_fval);
    snprintf(val, sizeof(val), "%s%s%s", nb, (_s->units && _s->units[0]) ? " " : "", _s->units ? _s->units : "");
  }
  d.setColor(DisplayDriver::YELLOW);
  d.setTextSize(2);
  d.drawTextCentered(W / 2, (int)(H * 0.24f), val);
  d.setTextSize(1);

  d.setColor(DisplayDriver::LIGHT);
  d.drawRect(8, by, bw, bh);
  d.drawTextCentered(8 + bw / 2, by + bh / 2 - 4, "-");
  d.drawRect(W - 8 - bw, by, bw, bh);
  d.drawTextCentered(W - 8 - bw / 2, by + bh / 2 - 4, "+");
  d.drawRect(8, cy, cw, bh);
  d.drawTextCentered(8 + cw / 2, cy + bh / 2 - 4, "Cancel");
  d.drawRect(W - 8 - cw, cy, cw, bh);
  d.drawTextCentered(W - 8 - cw / 2, cy + bh / 2 - 4, "OK");
  return 3000;
}

bool SettingEditScreen::handleInput(char c) {
  if (!_s) return false;
  switch (_s->type) {
    case ST_BOOL:
    case ST_ENUM:
    case ST_INT:
    case ST_FLOAT:
      if (c == KEY_UP || c == KEY_RIGHT) { adjust(+1); return true; }
      if (c == KEY_DOWN || c == KEY_LEFT) { adjust(-1); return true; }
      if (c == KEY_ENTER || c == KEY_SELECT) { commit(); return true; }
      if (c == KEY_CANCEL) { _task->closeSettingEdit(); return true; }
      return true;
    case ST_STRING:
      if (c == KEY_ENTER || c == KEY_SELECT) { commit(); return true; }
      if (c == KEY_CANCEL) { _task->closeSettingEdit(); return true; }
      if (c == KEY_BACKSPACE) { if (_slen > 0) _sbuf[--_slen] = 0; return true; }
      if ((unsigned char)c >= 32 && (unsigned char)c < 127) {
        if (_slen < (int)sizeof(_sbuf) - 1) { _sbuf[_slen++] = c; _sbuf[_slen] = 0; }
        return true;
      }
      return true;
    case ST_ACTION:
      if (c == KEY_ENTER || c == KEY_SELECT) { if (_s->action) _s->action(_task); _task->closeSettingEdit(); return true; }
      if (c == KEY_CANCEL) { _task->closeSettingEdit(); return true; }
      return true;
    case ST_INFO:
      _task->closeSettingEdit();
      return true;
  }
  return false;
}

bool SettingEditScreen::handleTouch(int x, int y, TouchEvent ev) {
  if (!_s) return false;
  if (ev != TouchEvent::release) return true;  // act on tap (release)
  DisplayDriver* d = _task->getDisplay();
  if (!d) return false;
  int W = d->width(), H = d->height();

  if (y < HEADER_H) { _task->closeSettingEdit(); return true; }  // header tap = back

  if (_s->type == ST_STRING) {
    if (_use_osk) {
      int ky = HEADER_H + 26;
      char k = _kb.handleTap(x, y, 2, ky, W - 4, H - ky - 2);
      if (k == OnScreenKeyboard::KEY_OK) commit();
      else if (k == OnScreenKeyboard::KEY_BKSP) { if (_slen > 0) _sbuf[--_slen] = 0; }
      else if (k >= 32 && k < 127) {
        if (_slen < (int)sizeof(_sbuf) - 1) { _sbuf[_slen++] = k; _sbuf[_slen] = 0; }
      }
    }
    return true;
  }

  int bw = (int)(W * 0.28f), bh = (int)(H * 0.18f), by = (int)(H * 0.46f);
  int cw = (int)(W * 0.40f), cy = H - bh - 6;

  if (_s->type == ST_INFO) { _task->closeSettingEdit(); return true; }

  if (_s->type == ST_ACTION) {
    if (inRect(x, y, 8, cy, cw, bh)) { _task->closeSettingEdit(); return true; }
    if (inRect(x, y, W - 8 - cw, cy, cw, bh)) { if (_s->action) _s->action(_task); _task->closeSettingEdit(); return true; }
    return true;
  }

  // BOOL / ENUM / INT / FLOAT
  if (inRect(x, y, 8, by, bw, bh)) { adjust(-1); return true; }
  if (inRect(x, y, W - 8 - bw, by, bw, bh)) { adjust(+1); return true; }
  if (inRect(x, y, 8, cy, cw, bh)) { _task->closeSettingEdit(); return true; }
  if (inRect(x, y, W - 8 - cw, cy, cw, bh)) { commit(); return true; }
  return true;
}
