#include "SettingsScreen.h"
#include "UITask.h"
#include <stdio.h>
#include <string.h>

// ---- styling (modeled on the MeshCore Android app) -------------------------
// Colors are set via setColorRGB(); on mono/limited displays they degrade to the
// nearest palette color automatically (see DisplayDriver::setColorRGB).
namespace {
struct RGB { uint8_t r, g, b; };
const RGB C_HEADER  = {38, 48, 63};    // navy header bar
const RGB C_CARD    = {42, 46, 53};    // row background
const RGB C_CARDSEL = {56, 62, 74};    // selected row background
const RGB C_DIV     = {64, 68, 78};    // row divider
const RGB C_LABEL   = {150, 160, 172}; // secondary (label) text
const RGB C_VALUE   = {236, 238, 242}; // primary (value) text
const RGB C_ACCENT  = {91, 141, 239};  // blue accent (selection, checks)
const RGB C_MUTED   = {120, 128, 140}; // affordance glyphs

inline void col(DisplayDriver& d, const RGB& c) { d.setColorRGB(c.r, c.g, c.b); }
inline bool big(DisplayDriver& d) { return d.height() >= 160; }
int headerH(DisplayDriver& d) { return big(d) ? 28 : 14; }
int rowH(DisplayDriver& d) { return big(d) ? 30 : 13; }
int visibleRows(DisplayDriver& d) {
  int vr = (d.height() - headerH(d)) / rowH(d);
  return vr < 1 ? 1 : vr;
}
bool inRect(int x, int y, int rx, int ry, int rw, int rh) {
  return x >= rx && y >= ry && x < rx + rw && y < ry + rh;
}

// small affordance triangles
void triRight(DisplayDriver& d, int xLeft, int cy, int half) {
  for (int i = 0; i <= half; i++) d.fillRect(xLeft + i, cy - (half - i), 1, 2 * (half - i) + 1);
}
void triLeft(DisplayDriver& d, int xLeft, int cy, int half) {
  for (int i = 0; i <= half; i++) d.fillRect(xLeft + i, cy - i, 1, 2 * i + 1);
}
void triDown(DisplayDriver& d, int cx, int yTop, int half) {
  for (int i = 0; i <= half; i++) d.fillRect(cx - half + i, yTop + i, 2 * (half - i) + 1, 1);
}
void drawCheckbox(DisplayDriver& d, int x, int y, int sz, bool on) {
  if (on) {
    col(d, C_ACCENT);
    d.fillRect(x, y, sz, sz);
    col(d, C_VALUE);  // V-shaped check
    d.fillRect(x + sz * 3 / 16, y + sz * 8 / 16, 2, 2);
    d.fillRect(x + sz * 4 / 16, y + sz * 9 / 16, 2, 2);
    d.fillRect(x + sz * 5 / 16, y + sz * 10 / 16, 2, 2);
    d.fillRect(x + sz * 6 / 16, y + sz * 9 / 16, 2, 2);
    d.fillRect(x + sz * 7 / 16, y + sz * 8 / 16, 2, 2);
    d.fillRect(x + sz * 8 / 16, y + sz * 7 / 16, 2, 2);
    d.fillRect(x + sz * 9 / 16, y + sz * 6 / 16, 2, 2);
    d.fillRect(x + sz * 10 / 16, y + sz * 5 / 16, 2, 2);
    d.fillRect(x + sz * 11 / 16, y + sz * 4 / 16, 2, 2);
  } else {
    col(d, C_MUTED);
    d.drawRect(x, y, sz, sz);
  }
}

void drawHeaderBar(DisplayDriver& d, const char* title, bool back) {
  int hH = headerH(d), W = d.width();
  col(d, C_HEADER);
  d.fillRect(0, 0, W, hH);
  int titleX = 4;
  if (back) {
    col(d, C_VALUE);
    triLeft(d, 5, hH / 2, big(d) ? 5 : 3);
    titleX = big(d) ? 20 : 12;
  }
  col(d, C_VALUE);
  d.setTextSize(big(d) ? 2 : 1);
  d.setCursor(titleX, (hH - (big(d) ? 14 : 8)) / 2);
  d.print(title);
  d.setTextSize(1);
}

// A field is "cyclable" (tap advances in place, no editor) when it has only a
// few accepted values: any ENUM, or an INT whose range spans few steps.
const int CYCLE_MAX_STEPS = 12;
bool isCyclable(const Setting& s) {
  if (s.type == ST_ENUM && s.num_opts > 0) return true;
  if (s.type == ST_INT && s.istep != 0) {
    long steps = (long)(s.imax - s.imin) / s.istep + 1;
    return steps > 1 && steps <= CYCLE_MAX_STEPS;
  }
  return false;
}

void formatValue(const Setting& s, char* out, size_t n) {
  switch (s.type) {
    case ST_BOOL: snprintf(out, n, "%s", s.getInt() ? "On" : "Off"); break;
    case ST_ENUM: {
      int32_t v = s.getInt();
      const char* lbl = "?";
      for (int i = 0; i < s.num_opts; i++)
        if (s.opts[i].value == v) { lbl = s.opts[i].label; break; }
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
    case ST_INFO: snprintf(out, n, "%s", s.getStr() ? s.getStr() : ""); break;
    case ST_ACTION: out[0] = 0; break;
  }
}
}  // namespace

// =================== SettingsListScreen ===================
int SettingsListScreen::itemCount() const {
  return _group ? _group->count : SETTINGS_ROOT_COUNT;
}

int SettingsListScreen::render(DisplayDriver& d) {
  int W = d.width();
  int hH = headerH(d), rh = rowH(d), vr = visibleRows(d), n = itemCount();

  if (_sel < _scroll_top) _scroll_top = _sel;
  if (_sel >= _scroll_top + vr) _scroll_top = _sel - vr + 1;
  if (_scroll_top > n - vr) _scroll_top = (n > vr) ? n - vr : 0;
  if (_scroll_top < 0) _scroll_top = 0;

  drawHeaderBar(d, _group ? _group->title : "Settings", _group != nullptr);

  bool overflow = n > vr;
  int rightX = W - (overflow ? 12 : 8);
  char nm[44], val[40], valf[44];

  for (int row = 0; row < vr; row++) {
    int idx = _scroll_top + row;
    if (idx >= n) break;
    int y = hH + row * rh;
    bool selected = (idx == _sel);

    col(d, selected ? C_CARDSEL : C_CARD);
    d.fillRect(0, y, W, rh - 1);
    col(d, C_DIV);
    d.fillRect(0, y + rh - 1, W, 1);
    if (selected) { col(d, C_ACCENT); d.fillRect(0, y, 3, rh - 1); }

    int padX = 8;

    if (!_group) {  // category navigation row
      col(d, C_VALUE);
      d.translateUTF8ToBlocks(nm, SETTINGS_ROOT[idx].title, sizeof(nm));
      d.setCursor(padX, y + (rh - 8) / 2);
      d.print(nm);
      col(d, C_MUTED);
      triRight(d, rightX - 4, y + rh / 2, 4);
      continue;
    }

    const Setting& s = _group->items[idx];
    d.translateUTF8ToBlocks(nm, s.label, sizeof(nm));

    if (s.type == ST_BOOL) {
      col(d, C_VALUE);
      d.setCursor(padX, y + (rh - 8) / 2);
      d.print(nm);
      int cb = big(d) ? 16 : 9;
      drawCheckbox(d, rightX - cb, y + (rh - cb) / 2, cb, s.getInt());
    } else if (s.type == ST_ACTION) {
      col(d, C_ACCENT);
      d.setCursor(padX, y + (rh - 8) / 2);
      d.print(nm);
      col(d, C_MUTED);
      triRight(d, rightX - 4, y + rh / 2, 4);
    } else {  // ENUM / INT / FLOAT / STRING / INFO -> label over value
      formatValue(s, val, sizeof(val));
      d.translateUTF8ToBlocks(valf, val, sizeof(valf));
      bool drop = isCyclable(s);  // show the dropdown affordance on tap-to-cycle fields
      int avail = rightX - padX - (drop ? 12 : 2);
      if (big(d)) {
        col(d, C_LABEL);
        d.drawTextEllipsized(padX, y + 4, avail, nm);
        col(d, C_VALUE);
        d.drawTextEllipsized(padX, y + rh - 12, avail, valf);
        if (drop) { col(d, C_MUTED); triDown(d, rightX - 4, y + rh - 11, 4); }
      } else {
        col(d, C_LABEL);
        d.drawTextEllipsized(padX, y + (rh - 8) / 2, avail / 2, nm);
        col(d, C_VALUE);
        d.drawTextRightAlign(rightX - (drop ? 8 : 0), y + (rh - 8) / 2, valf);
        if (drop) { col(d, C_MUTED); triDown(d, rightX - 4, y + rh / 2 - 1, 3); }
      }
    }
  }

  if (overflow) {
    int trackX = W - 4, trackY = hH, trackH = vr * rh;
    col(d, C_DIV);
    d.fillRect(trackX, trackY, 2, trackH);
    int thumbH = trackH * vr / n;
    if (thumbH < 8) thumbH = 8;
    int denom = (n - vr) > 0 ? (n - vr) : 1;
    int thumbY = trackY + (trackH - thumbH) * _scroll_top / denom;
    col(d, C_ACCENT);
    d.fillRect(trackX, thumbY, 2, thumbH);
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
    if (s->setInt) s->setInt(s->getInt() ? 0 : 1);
    return;
  }
  if (s->type == ST_ENUM && s->num_opts > 0) {
    // cycle to the next option in place (no editor popup); options with a
    // negative value (e.g. a "Custom" sentinel) are skipped while cycling.
    int cur = 0;
    int32_t v = s->getInt ? s->getInt() : 0;
    for (int i = 0; i < s->num_opts; i++)
      if (s->opts[i].value == v) { cur = i; break; }
    int nxt = cur, guard = 0;
    do { nxt = (nxt + 1) % s->num_opts; guard++; } while (s->opts[nxt].value < 0 && guard <= s->num_opts);
    if (s->setInt) s->setInt(s->opts[nxt].value);
    return;
  }
  if (s->type == ST_INT && isCyclable(*s)) {
    // small-range integer: advance one step in place, wrapping at the top
    int32_t v = s->getInt ? s->getInt() : s->imin;
    v += s->istep;
    if (v > s->imax) v = s->imin;
    if (s->setInt) s->setInt(v);
    return;
  }
  if (s->type == ST_ACTION) {
    if (s->imin != 0) _task->editSetting(s);   // needs confirmation -> confirm screen
    else if (s->action) s->action(_task);      // fire immediately on tap
    return;
  }
  if (s->type == ST_INFO) return;
  _task->editSetting(s);  // INT / FLOAT / STRING
}

bool SettingsListScreen::handleInput(char c) {
  int n = itemCount();
  if (c == KEY_UP || c == KEY_PREV) { if (_sel > 0) _sel--; return true; }
  if (c == KEY_DOWN || c == KEY_NEXT) { if (_sel < n - 1) _sel++; return true; }
  if (c == KEY_ENTER || c == KEY_SELECT) { activate(_sel); return true; }
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
  int hH = headerH(*d), rh = rowH(*d), vr = visibleRows(*d), n = itemCount();
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
    if (_moved) return true;
    if (y < hH) {
      if (_group) showRoot();
      else _task->gotoHomeScreen();
      return true;
    }
    int idx = _scroll_top + (y - hH) / rh;
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
    case ST_INT: _ival = s->getInt ? s->getInt() : 0; break;
    case ST_FLOAT: _fval = s->getFloat ? s->getFloat() : 0; break;
    case ST_STRING: {
      const char* cur = s->getStr ? s->getStr() : "";
      strncpy(_sbuf, cur, sizeof(_sbuf) - 1);
      _sbuf[sizeof(_sbuf) - 1] = 0;
      _slen = strlen(_sbuf);
      break;
    }
    default: break;
  }
}

int SettingEditScreen::enumIndex() const {
  for (int i = 0; i < _s->num_opts; i++)
    if (_s->opts[i].value == _ival) return i;
  return 0;
}

void SettingEditScreen::adjust(int dir) {
  switch (_s->type) {
    case ST_BOOL: _ival = _ival ? 0 : 1; break;
    case ST_ENUM: { int i = (enumIndex() + dir + _s->num_opts) % _s->num_opts; _ival = _s->opts[i].value; break; }
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
    default: break;
  }
}

void SettingEditScreen::commit() {
  bool ok = true;
  switch (_s->type) {
    case ST_BOOL:
    case ST_ENUM:
    case ST_INT: ok = _s->setInt ? _s->setInt(_ival) : false; break;
    case ST_FLOAT: ok = _s->setFloat ? _s->setFloat(_fval) : false; break;
    case ST_STRING: ok = _s->setStr ? _s->setStr(_sbuf) : false; break;
    default: break;
  }
  _task->showAlert(ok ? "Saved" : "Invalid", 1000);
  _task->closeSettingEdit();
}

static void workingValue(const Setting* s, int32_t ival, float fval, char* out, size_t n) {
  if (s->type == ST_BOOL) snprintf(out, n, "%s", ival ? "On" : "Off");
  else if (s->type == ST_ENUM) {
    const char* lbl = "?";
    for (int i = 0; i < s->num_opts; i++)
      if (s->opts[i].value == ival) { lbl = s->opts[i].label; break; }
    snprintf(out, n, "%s", lbl);
  } else if (s->type == ST_INT) {
    snprintf(out, n, "%ld%s%s", (long)ival, (s->units && s->units[0]) ? " " : "", s->units ? s->units : "");
  } else if (s->type == ST_FLOAT) {
    char nb[16];
    snprintf(nb, sizeof(nb), "%g", (double)fval);
    snprintf(out, n, "%s%s%s", nb, (s->units && s->units[0]) ? " " : "", s->units ? s->units : "");
  } else out[0] = 0;
}

// rounded-ish filled button helper
static void drawButton(DisplayDriver& d, int x, int y, int w, int h, const char* label, const RGB& bg,
                       const RGB& fg) {
  col(d, bg);
  d.fillRect(x, y, w, h);
  col(d, C_DIV);
  d.drawRect(x, y, w, h);
  col(d, fg);
  d.drawTextCentered(x + w / 2, y + h / 2 - 4, label);
}

int SettingEditScreen::render(DisplayDriver& d) {
  if (!_s) return 1000;
  int W = d.width(), H = d.height();
  drawHeaderBar(d, _s->label, true);

  if (_s->type == ST_STRING) {
    int boxY = headerH(d) + 4;
    col(d, C_CARD);
    d.fillRect(4, boxY, W - 8, 20);
    col(d, C_DIV);
    d.drawRect(4, boxY, W - 8, 20);
    char shown[68];
    snprintf(shown, sizeof(shown), "%s_", _sbuf);
    col(d, C_VALUE);
    d.drawTextEllipsized(9, boxY + 6, W - 18, shown);
    if (_use_osk) {
      int ky = boxY + 26;
      _kb.render(d, 2, ky, W - 4, H - ky - 2);
    } else {
      col(d, C_LABEL);
      d.drawTextCentered(W / 2, H / 2, "type on keyboard");
    }
    return 1000;
  }

  int bw = (int)(W * 0.28f), bh = (int)(H * 0.18f), by = (int)(H * 0.46f);
  int cw = (int)(W * 0.40f), cy = H - bh - 6;

  if (_s->type == ST_INFO) {
    char val[48];
    formatValue(*_s, val, sizeof(val));
    col(d, C_VALUE);
    d.drawTextCentered(W / 2, H / 3, val);
    drawButton(d, W / 2 - cw / 2, cy, cw, bh, "Back", C_CARD, C_VALUE);
    return 3000;
  }

  if (_s->type == ST_ACTION) {
    col(d, C_VALUE);
    d.drawTextCentered(W / 2, H / 3, "Confirm?");
    drawButton(d, 8, cy, cw, bh, "Cancel", C_CARD, C_VALUE);
    drawButton(d, W - 8 - cw, cy, cw, bh, "Confirm", C_ACCENT, C_VALUE);
    return 3000;
  }

  // BOOL / ENUM / INT / FLOAT
  char val[40];
  workingValue(_s, _ival, _fval, val, sizeof(val));
  col(d, C_VALUE);
  d.setTextSize(2);
  d.drawTextCentered(W / 2, (int)(H * 0.24f), val);
  d.setTextSize(1);

  if ((_s->type == ST_INT || _s->type == ST_FLOAT) && H >= 160) {
    float frac;
    if (_s->type == ST_INT) {
      float span = (float)(_s->imax - _s->imin);
      frac = span > 0 ? (float)(_ival - _s->imin) / span : 0;
    } else {
      float span = _s->fmax - _s->fmin;
      frac = span > 0 ? (_fval - _s->fmin) / span : 0;
    }
    if (frac < 0) frac = 0;
    if (frac > 1) frac = 1;
    int barW = (int)(W * 0.7f), barX = (W - barW) / 2, barY = (int)(H * 0.34f), barH = 5;
    col(d, C_CARD);
    d.fillRect(barX, barY, barW, barH);
    col(d, C_ACCENT);
    d.fillRect(barX, barY, (int)(barW * frac), barH);
    char lo[14], hi[14];
    if (_s->type == ST_INT) {
      snprintf(lo, sizeof(lo), "%ld", (long)_s->imin);
      snprintf(hi, sizeof(hi), "%ld", (long)_s->imax);
    } else {
      snprintf(lo, sizeof(lo), "%g", (double)_s->fmin);
      snprintf(hi, sizeof(hi), "%g", (double)_s->fmax);
    }
    col(d, C_LABEL);
    d.drawTextLeftAlign(barX, barY + barH + 3, lo);
    d.drawTextRightAlign(barX + barW, barY + barH + 3, hi);
  }

  drawButton(d, 8, by, bw, bh, "-", C_CARD, C_VALUE);
  drawButton(d, W - 8 - bw, by, bw, bh, "+", C_CARD, C_VALUE);
  drawButton(d, 8, cy, cw, bh, "Cancel", C_CARD, C_VALUE);
  drawButton(d, W - 8 - cw, cy, cw, bh, "OK", C_ACCENT, C_VALUE);
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
  if (ev != TouchEvent::release) return true;
  DisplayDriver* d = _task->getDisplay();
  if (!d) return false;
  int W = d->width(), H = d->height();

  if (y < headerH(*d)) { _task->closeSettingEdit(); return true; }

  if (_s->type == ST_STRING) {
    if (_use_osk) {
      int ky = headerH(*d) + 4 + 26;
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

  if (inRect(x, y, 8, by, bw, bh)) { adjust(-1); return true; }
  if (inRect(x, y, W - 8 - bw, by, bw, bh)) { adjust(+1); return true; }
  if (inRect(x, y, 8, cy, cw, bh)) { _task->closeSettingEdit(); return true; }
  if (inRect(x, y, W - 8 - cw, cy, cw, bh)) { commit(); return true; }
  return true;
}
