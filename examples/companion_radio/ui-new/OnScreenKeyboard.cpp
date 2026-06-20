#include "OnScreenKeyboard.h"
#include <string.h>

static const char* KB_ROWS[4] = {
  "1234567890",
  "qwertyuiop",
  "asdfghjkl",
  "zxcvbnm",
};
#define KB_COLS 10  // widest row, used to size cells

static char shiftChar(char c, bool shift) {
  if (shift && c >= 'a' && c <= 'z') return c - 'a' + 'A';
  return c;
}

void OnScreenKeyboard::render(DisplayDriver& d, int rx, int ry, int rw, int rh) {
  int rowH = rh / 5;
  int cellW = rw / KB_COLS;
  d.setTextSize(1);
  for (int r = 0; r < 4; r++) {
    int n = strlen(KB_ROWS[r]);
    int startX = rx + (rw - n * cellW) / 2;
    int y = ry + r * rowH;
    for (int c = 0; c < n; c++) {
      int x = startX + c * cellW;
      d.setColor(DisplayDriver::LIGHT);
      d.drawRect(x, y, cellW - 2, rowH - 2);
      char tmp[2] = { shiftChar(KB_ROWS[r][c], _shift), 0 };
      d.drawTextCentered(x + cellW / 2 - 1, y + rowH / 2 - 4, tmp);
    }
  }
  // bottom special row: Shift | space | Bksp | OK
  int y = ry + 4 * rowH;
  int bw = rw / 4;
  const char* labels[4] = { _shift ? "SHFT" : "shft", "space", "<-", "OK" };
  for (int b = 0; b < 4; b++) {
    int x = rx + b * bw;
    d.setColor(DisplayDriver::LIGHT);
    d.drawRect(x, y, bw - 2, rowH - 2);
    d.drawTextCentered(x + bw / 2 - 1, y + rowH / 2 - 4, labels[b]);
  }
}

char OnScreenKeyboard::handleTap(int tx, int ty, int rx, int ry, int rw, int rh) {
  if (tx < rx || ty < ry || tx >= rx + rw || ty >= ry + rh) return KEY_NONE;
  int rowH = rh / 5;
  int cellW = rw / KB_COLS;
  int row = (ty - ry) / rowH;
  if (row < 0) return KEY_NONE;
  if (row < 4) {
    int n = strlen(KB_ROWS[row]);
    int startX = rx + (rw - n * cellW) / 2;
    int col = (tx - startX) / cellW;
    if (col >= 0 && col < n) return shiftChar(KB_ROWS[row][col], _shift);
    return KEY_NONE;
  }
  // bottom row
  int bw = rw / 4;
  int b = (tx - rx) / bw;
  switch (b) {
    case 0: _shift = !_shift; return KEY_NONE;  // toggle shift
    case 1: return ' ';
    case 2: return KEY_BKSP;
    case 3: return KEY_OK;
    default: return KEY_NONE;
  }
}
