#!/usr/bin/env python3
"""Generate ui-new/Icons.h: 24x24 monochrome XBM line-art glyphs for the
launcher tiles. Icons are drawn with simple geometric primitives so they stay
clean and recognisable, then emitted as XBM byte arrays (LSB-first, row-padded
to whole bytes) consumable by DisplayDriver::drawXbm()."""
import math, os

S = 24  # icon size (square)

class Bmp:
    def __init__(self, n=S):
        self.n = n
        self.px = [[0]*n for _ in range(n)]
    def set(self, x, y):
        if 0 <= x < self.n and 0 <= y < self.n:
            self.px[y][x] = 1
    def hline(self, x0, x1, y):
        for x in range(min(x0,x1), max(x0,x1)+1): self.set(x, y)
    def vline(self, x, y0, y1):
        for y in range(min(y0,y1), max(y0,y1)+1): self.set(x, y)
    def line(self, x0, y0, x1, y1):
        dx, dy = abs(x1-x0), abs(y1-y0)
        sx = 1 if x0 < x1 else -1
        sy = 1 if y0 < y1 else -1
        err = dx - dy
        while True:
            self.set(x0, y0)
            if x0 == x1 and y0 == y1: break
            e2 = 2*err
            if e2 > -dy: err -= dy; x0 += sx
            if e2 < dx:  err += dx; y0 += sy
    def rect(self, x0, y0, x1, y1):
        self.hline(x0, x1, y0); self.hline(x0, x1, y1)
        self.vline(x0, y0, y1); self.vline(x1, y0, y1)
    def fillrect(self, x0, y0, x1, y1):
        for y in range(y0, y1+1): self.hline(x0, x1, y)
    def circle(self, cx, cy, r):
        # midpoint circle outline
        x, y, d = r, 0, 1 - r
        pts = []
        while x >= y:
            for (px, py) in [(x,y),(y,x),(-x,y),(-y,x),(-x,-y),(-y,-x),(x,-y),(y,-x)]:
                self.set(cx+px, cy+py)
            y += 1
            if d <= 0: d += 2*y + 1
            else: x -= 1; d += 2*(y-x) + 1
    def arc(self, cx, cy, r, a0, a1, step=4):
        a = a0
        while a <= a1:
            rad = math.radians(a)
            self.set(round(cx + r*math.cos(rad)), round(cy + r*math.sin(rad)))
            a += step
    def disc(self, cx, cy, r):
        for y in range(-r, r+1):
            for x in range(-r, r+1):
                if x*x + y*y <= r*r: self.set(cx+x, cy+y)

def chat(b):
    b.rect(2, 3, 21, 16)
    b.line(6, 16, 6, 21); b.line(6, 21, 11, 16)   # tail
    b.hline(6, 17, 7); b.hline(6, 17, 10); b.hline(6, 13, 13)

def contacts(b):
    b.circle(12, 8, 4)               # head
    b.arc(12, 22, 9, 200, 340, 3)    # shoulders
    b.arc(12, 22, 8, 200, 340, 3)

def repeaters(b):
    b.line(8, 22, 12, 6); b.line(16, 22, 12, 6)   # tower
    b.hline(9, 15, 16)
    b.disc(12, 6, 1)
    b.arc(12, 8, 6, 215, 325, 5); b.arc(12, 8, 9, 215, 325, 4)  # waves L/R top

def finder(b):
    b.circle(10, 10, 6)
    b.line(15, 15, 21, 21); b.line(16, 14, 22, 20)

def heard(b):  # concentric sound waves from a point (recently heard)
    b.disc(5, 19, 1)
    b.arc(5, 19, 6, -90, 0, 4)
    b.arc(5, 19, 11, -90, 0, 3)
    b.arc(5, 19, 16, -90, 0, 2)

def mapicon(b):
    b.rect(2, 5, 21, 19)
    b.vline(9, 5, 19); b.vline(15, 5, 19)        # fold lines
    b.disc(14, 11, 1); b.arc(14, 11, 3, 0, 360, 6)  # marker

def advertise(b):  # megaphone / broadcast
    b.line(4, 11, 15, 6); b.line(4, 13, 15, 18); b.vline(4, 11, 13)
    b.line(15, 6, 15, 18)
    b.arc(15, 12, 4, -60, 60, 5); b.arc(15, 12, 7, -55, 55, 4)

def settings(b):  # gear
    b.circle(12, 12, 7); b.circle(12, 12, 3)
    for a in range(0, 360, 45):
        rad = math.radians(a)
        x0 = round(12 + 7*math.cos(rad)); y0 = round(12 + 7*math.sin(rad))
        x1 = round(12 + 10*math.cos(rad)); y1 = round(12 + 10*math.sin(rad))
        b.line(x0, y0, x1, y1)

def trace(b):  # connected nodes / path
    nodes = [(4,19),(11,12),(18,6),(20,18)]
    b.line(*nodes[0], *nodes[1]); b.line(*nodes[1], *nodes[2]); b.line(*nodes[1], *nodes[3])
    for (x,y) in nodes:
        b.disc(x, y, 2)

def terminal(b):  # monitor with prompt
    b.rect(2, 4, 21, 18); b.hline(8, 15, 21)      # screen + stand base
    b.vline(11, 18, 20); b.vline(12, 18, 20)
    b.line(5, 8, 8, 11); b.line(8, 11, 5, 14)     # '>'
    b.hline(10, 15, 14)                           # '_'

def noise(b):  # waveform
    pts = [(-2,0),(-6,4),(-4,-7),(-1,8),(2,-9),(5,6),(8,-3)]
    prev = None
    for i in range(2, 22):
        y = 12 + round(7*math.sin(i*0.9))
        if prev: b.line(prev[0], prev[1], i, y)
        prev = (i, y)

def signal(b):  # ascending bars
    xs = [3, 9, 15, 21]
    hs = [6, 11, 16, 21]
    for x, h in zip(xs, hs):
        b.fillrect(x-1, 22-h, x+1, 22)

ICONS = [
    ("chat", chat), ("contacts", contacts), ("repeaters", repeaters),
    ("finder", finder), ("heard", heard), ("map", mapicon),
    ("advertise", advertise), ("settings", settings), ("trace", trace),
    ("terminal", terminal), ("noise", noise), ("signal", signal),
]

def to_xbm(b):
    bytes_per_row = (b.n + 7) // 8
    out = []
    for y in range(b.n):
        for byte in range(bytes_per_row):
            v = 0
            for bit in range(8):
                x = byte*8 + bit
                if x < b.n and b.px[y][x]:
                    v |= (1 << bit)   # XBM is LSB-first
            out.append(v)
    return out

def main():
    here = os.path.dirname(os.path.abspath(__file__))
    out_path = os.path.join(here, "..", "examples", "companion_radio", "ui-new", "TileIcons.h")
    lines = []
    lines.append("#pragma once")
    lines.append("")
    lines.append("// Auto-generated by sim/gen_icons.py -- do not edit by hand.")
    lines.append("// (Named TileIcons.h, not Icons.h, to avoid colliding with the existing")
    lines.append("//  icons.h on case-insensitive filesystems.)")
    lines.append("// 24x24 monochrome XBM line-art glyphs for the launcher tiles, drawn via")
    lines.append("// DisplayDriver::drawXbm() and tinted with the current color.")
    lines.append("#include <stdint.h>")
    lines.append("")
    lines.append("namespace icons {")
    lines.append("")
    lines.append(f"constexpr int ICON_W = {S};")
    lines.append(f"constexpr int ICON_H = {S};")
    lines.append("")
    for name, fn in ICONS:
        b = Bmp()
        fn(b)
        data = to_xbm(b)
        rows = []
        bpr = (S + 7)//8
        for i in range(0, len(data), bpr):
            rows.append(", ".join(f"0x{v:02x}" for v in data[i:i+bpr]))
        body = ",\n  ".join(rows)
        lines.append(f"static const uint8_t {name}[] = {{")
        lines.append(f"  {body}")
        lines.append("};")
        lines.append("")
    lines.append("}  // namespace icons")
    lines.append("")
    with open(out_path, "w") as f:
        f.write("\n".join(lines))
    print("wrote", os.path.normpath(out_path))

if __name__ == "__main__":
    main()
