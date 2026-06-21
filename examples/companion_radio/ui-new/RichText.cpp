#include "RichText.h"
#include "EmojiGlyphs.h"
#include <string.h>

using namespace uistyle;

namespace richtext {

Result layout(DisplayDriver& d, int x0, int y, int maxw, int lh,
              const char* text, const RGB& base, bool draw) {
  bool emojiOn = (lh >= emoji::EMOJI_H);
  int spacew = d.getTextWidth(" ");

  char seg[256]; int seglen = 0; int segX = x0;
  int cx = x0;
  int lines = 0, widest = 0;
  bool lineHasContent = false;
  bool pendingSpace = false;
  RGB lineColor = base;
  char prevc = 0;  // char before current position, for emoji word-boundary check

  auto isAlnum = [](char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9');
  };
  auto boundary = [&](char before) { return before == 0 || !isAlnum(before); };

  const char* p = text;

  auto computeQuote = [&](const char* q) {
    while (*q == ' ') q++;
    lineColor = (*q == '>') ? C_MUTED : base;
  };
  computeQuote(p);

  auto flushSeg = [&]() {
    if (seglen > 0) {
      seg[seglen] = 0;
      if (draw) { col(d, lineColor); d.setCursor(segX, y); d.print(seg); }
      cx = segX + d.getTextWidth(seg);  // authoritative width for emoji alignment
      seglen = 0;
    }
    segX = cx;
  };
  auto trackWidth = [&]() { if (cx - x0 > widest) widest = cx - x0; };
  auto newline = [&]() {
    flushSeg(); trackWidth();
    y += lh; lines++; cx = x0; segX = x0;
    lineHasContent = false; pendingSpace = false;
    computeQuote(p);
  };
  auto appendChar = [&](char c, int w) {
    if (seglen < (int)sizeof(seg) - 1) { if (seglen == 0) segX = cx; seg[seglen++] = c; }
    cx += w; lineHasContent = true;
  };

  while (*p) {
    if (*p == '\n') { newline(); prevc = 0; p++; continue; }
    if (*p == ' ') { pendingSpace = true; prevc = ' '; p++; continue; }

    int el = 0;
    const uint8_t* g = (emojiOn && boundary(prevc)) ? emoji::match(p, &el) : nullptr;
    if (g) {
      int tw = emoji::EMOJI_W + 1;
      int need = (lineHasContent && pendingSpace ? spacew : 0) + tw;
      if (lineHasContent && cx - x0 + need > maxw) newline();
      if (lineHasContent && pendingSpace) appendChar(' ', spacew);
      pendingSpace = false;
      flushSeg();
      if (draw) {
        col(d, base);
        int gy = y + (lh - emoji::EMOJI_H) / 2; if (gy < y) gy = y;
        d.drawXbm(cx, gy, g, emoji::EMOJI_W, emoji::EMOJI_H);
      }
      cx += tw; segX = cx; lineHasContent = true;
      prevc = 0;  // emoji is a word boundary -> allow a following emoji
      p += el; continue;
    }

    // gather a word (until space/newline/emoji-on-boundary)
    char word[160]; int wl = 0;
    while (*p && *p != ' ' && *p != '\n' && wl < (int)sizeof(word) - 1) {
      char before = wl > 0 ? word[wl - 1] : prevc;
      int dl; if (emojiOn && boundary(before) && emoji::match(p, &dl)) break;
      word[wl++] = *p++;
    }
    word[wl] = 0;
    if (wl > 0) prevc = word[wl - 1];
    int ww = d.getTextWidth(word);
    int sp = (lineHasContent && pendingSpace) ? spacew : 0;

    if (ww > maxw) {  // hard-split long words (e.g. URLs)
      if (lineHasContent && pendingSpace) appendChar(' ', spacew);
      pendingSpace = false;
      for (int i = 0; i < wl; i++) {
        char ch[2] = {word[i], 0};
        int cw = d.getTextWidth(ch);
        if (lineHasContent && cx - x0 + cw > maxw) newline();
        appendChar(word[i], cw);
      }
      continue;
    }

    if (lineHasContent && cx - x0 + sp + ww > maxw) newline();
    if (lineHasContent && pendingSpace) appendChar(' ', spacew);
    pendingSpace = false;
    for (int i = 0; i < wl; i++) {
      char ch[2] = {word[i], 0};
      appendChar(word[i], d.getTextWidth(ch));
    }
  }
  flushSeg(); trackWidth();
  if (lineHasContent) lines++;
  if (lines == 0) lines = 1;

  Result r; r.lines = lines; r.maxw = widest; return r;
}

}  // namespace richtext
