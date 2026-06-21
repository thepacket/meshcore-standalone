#pragma once

#include <helpers/ui/DisplayDriver.h>
#include "UIStyle.h"

// Word-wrapped rich text for chat bubbles: inline emoji glyphs (ASCII
// shortcodes -> bitmaps), '\n' breaks, and dimmed quote lines (starting '>').
namespace richtext {

struct Result { int lines; int maxw; };

// Lay out `text` in a column of width `maxw` with line height `lh`. When draw is
// true it renders at (x,y) using `base` (quote lines use a muted colour); when
// false it only measures. Returns line count + widest line width.
Result layout(DisplayDriver& d, int x, int y, int maxw, int lh,
              const char* text, const uistyle::RGB& base, bool draw);

}  // namespace richtext
