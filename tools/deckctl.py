#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-or-later
# Part of meshcore-standalone (GPL-3.0-or-later). See LICENSE and NOTICE.
"""
deckctl — headless client for the on-device "remote screen" (WebScreen) server.

The T-Deck LVGL firmware can mirror its live screen to a browser over a
PIN-gated WebSocket server (Settings > Remote screen). This is a dependency-free
command-line client for the same server: it decodes the RGB565 framebuffer into
PNG screenshots and sends taps / keys / swipes back — useful for scripting,
screenshots, and CI without a browser. Uses only the Python stdlib.

Usage:
  export DECK_IP=192.168.1.50 DECK_PIN=1234      # or pass ip:/pin: as first args
  python3 deckctl.py shot:home.png                # connect, save a screenshot
  python3 deckctl.py tap:250,150 wait:800 shot:after.png
  python3 deckctl.py swipe:160,215,160,45 shot:scrolled.png
  python3 deckctl.py text:"hello" enter shot:typed.png
  python3 deckctl.py ip:192.168.1.50 pin:1234 shot:s.png   # inline connection

Actions (executed in order):
  ip:A.B.C.D        set device IP (else $DECK_IP)
  pin:NNNN          set access PIN (else $DECK_PIN)
  shot:FILE.png     save the current framebuffer as a PNG
  tap:X,Y           tap at device coords (0..319, 0..239)
  swipe:X1,Y1,X2,Y2 slow drag (scrolls lists)
  key:CP            send one key codepoint (13=enter, 8=backspace, 27=esc)
  text:STRING       type a string
  enter | back      shorthand for key:13 / key:8
  wait:MS           sleep milliseconds
"""
import socket, sys, os, struct, base64, threading, time, zlib

IP   = os.environ.get("DECK_IP", "")
PIN  = os.environ.get("DECK_PIN", "")
PORT = int(os.environ.get("DECK_PORT", "8080"))
W, H = 320, 240

fb = bytearray(W * H * 3)          # RGB888 framebuffer
fb_lock = threading.Lock()
frames_seen = [0]
stop = [False]


def ws_connect():
    s = socket.create_connection((IP, PORT), timeout=8)
    s.settimeout(8)
    key = base64.b64encode(os.urandom(16)).decode()
    req = (f"GET /mirror?pin={PIN} HTTP/1.1\r\nHost: {IP}\r\n"
           "Upgrade: websocket\r\nConnection: Upgrade\r\n"
           f"Sec-WebSocket-Key: {key}\r\nSec-WebSocket-Version: 13\r\n\r\n")
    s.sendall(req.encode())
    buf = b""
    while b"\r\n\r\n" not in buf:
        d = s.recv(1)
        if not d:
            raise RuntimeError("handshake closed: " + buf.decode(errors="replace"))
        buf += d
    if b"101" not in buf.split(b"\r\n")[0]:
        raise RuntimeError("no upgrade (bad PIN?): " + buf.split(b"\r\n")[0].decode(errors="replace"))
    return s


def recv_exact(s, n):
    b = b""
    while len(b) < n:
        d = s.recv(n - len(b))
        if not d:
            raise RuntimeError("closed")
        b += d
    return b


def recv_frame(s):
    h = recv_exact(s, 2)
    op = h[0] & 0x0F
    ln = h[1] & 0x7F
    if ln == 126:
        ln = struct.unpack(">H", recv_exact(s, 2))[0]
    elif ln == 127:
        ln = struct.unpack(">Q", recv_exact(s, 8))[0]
    mask = recv_exact(s, 4) if (h[1] & 0x80) else b""
    pay = recv_exact(s, ln)
    if mask:
        pay = bytes(b ^ mask[i % 4] for i, b in enumerate(pay))
    return op, pay


def apply_band(p):
    if not p or p[0] != 1:
        return
    fl = p[1]
    x = p[2] | (p[3] << 8); y = p[4] | (p[5] << 8)
    w = p[6] | (p[7] << 8); h = p[8] | (p[9] << 8)
    o = 10
    px = bytearray(w * h * 3)
    j = 0
    if fl & 1:  # RLE runs [count, lo, hi]
        end = len(p)
        while o + 2 < end and j < len(px):
            c = p[o]; lo = p[o + 1]; hi = p[o + 2]; o += 3
            v = lo | (hi << 8)
            r = (v >> 8) & 0xF8; g = (v >> 3) & 0xFC; b = (v << 3) & 0xF8
            for _ in range(c):
                if j + 3 > len(px):
                    break
                px[j] = r; px[j + 1] = g; px[j + 2] = b; j += 3
    else:       # raw RGB565 LE
        for _ in range(w * h):
            if o + 1 >= len(p):
                break
            v = p[o] | (p[o + 1] << 8); o += 2
            px[j] = (v >> 8) & 0xF8; px[j + 1] = (v >> 3) & 0xFC; px[j + 2] = (v << 3) & 0xF8; j += 3
    with fb_lock:
        for row in range(h):
            dy = y + row
            if dy < 0 or dy >= H:
                continue
            di = (dy * W + x) * 3
            si = row * w * 3
            cw = min(w, W - x)
            fb[di:di + cw * 3] = px[si:si + cw * 3]
    frames_seen[0] += 1


def reader(s):
    try:
        while not stop[0]:
            op, p = recv_frame(s)
            if op == 0x8:
                break
            if op == 0x2:
                apply_band(p)
    except Exception as e:
        if not stop[0]:
            sys.stderr.write(f"[reader] {e}\n")


def send_bin(s, data):
    b = bytes(data)
    mask = os.urandom(4)
    masked = bytes(c ^ mask[i % 4] for i, c in enumerate(b))
    s.sendall(bytes([0x82, 0x80 | len(b)]) + mask + masked)


def tap(s, x, y):
    send_bin(s, [1, x & 255, x >> 8, y & 255, y >> 8, 0])   # clean 0->1 edge
    time.sleep(0.04)
    send_bin(s, [1, x & 255, x >> 8, y & 255, y >> 8, 1])
    time.sleep(0.25)   # hold long enough for the indev read to catch it under streaming load
    send_bin(s, [1, x & 255, x >> 8, y & 255, y >> 8, 0])


def swipe(s, x1, y1, x2, y2, steps=20):
    # slow + deliberate: LVGL ignores a too-fast drag as a scroll gesture
    send_bin(s, [1, x1 & 255, x1 >> 8, y1 & 255, y1 >> 8, 0])
    time.sleep(0.03)
    send_bin(s, [1, x1 & 255, x1 >> 8, y1 & 255, y1 >> 8, 1])
    time.sleep(0.05)
    for i in range(1, steps + 1):
        x = x1 + (x2 - x1) * i // steps
        y = y1 + (y2 - y1) * i // steps
        send_bin(s, [1, x & 255, x >> 8, y & 255, y >> 8, 1])
        time.sleep(0.025)
    time.sleep(0.03)
    send_bin(s, [1, x2 & 255, x2 >> 8, y2 & 255, y2 >> 8, 0])
    time.sleep(0.15)


def key(s, cp):
    send_bin(s, [2, cp & 255, (cp >> 8) & 255])


def save_png(path):
    with fb_lock:
        data = bytes(fb)
    raw = bytearray()
    for y in range(H):
        raw.append(0)
        raw += data[y * W * 3:(y + 1) * W * 3]
    comp = zlib.compress(bytes(raw), 6)

    def chunk(typ, d):
        return struct.pack(">I", len(d)) + typ + d + struct.pack(">I", zlib.crc32(typ + d) & 0xffffffff)

    png = (b'\x89PNG\r\n\x1a\n'
           + chunk(b'IHDR', struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0))
           + chunk(b'IDAT', comp) + chunk(b'IEND', b''))
    with open(path, 'wb') as f:
        f.write(png)
    print(f"saved {path} (frames={frames_seen[0]})")


def main():
    global IP, PIN
    args = sys.argv[1:]
    # pull connection settings first so they can appear anywhere
    rest = []
    for a in args:
        k, _, v = a.partition(":")
        if k == "ip":
            IP = v
        elif k == "pin":
            PIN = v
        else:
            rest.append(a)
    if not IP or not PIN:
        sys.exit("set device IP + PIN via $DECK_IP/$DECK_PIN or ip:/pin: args "
                 "(see Settings > Remote screen on the device)")

    s = ws_connect()
    print(f"connected {IP}:{PORT}")
    threading.Thread(target=reader, args=(s,), daemon=True).start()
    time.sleep(0.4)   # let the first frame(s) arrive
    for a in rest:
        k, _, v = a.partition(":")
        if k == "wait":
            time.sleep(float(v) / 1000.0)
        elif k == "shot":
            save_png(v)
        elif k == "tap":
            x, y = map(int, v.split(",")); tap(s, x, y)
        elif k == "swipe":
            x1, y1, x2, y2 = map(int, v.split(",")); swipe(s, x1, y1, x2, y2)
        elif k == "key":
            key(s, int(v))
        elif k == "text":
            for ch in v:
                key(s, ord(ch)); time.sleep(0.05)
        elif k == "enter":
            key(s, 13)
        elif k == "back":
            key(s, 8)
        else:
            print("? unknown action:", a)
    stop[0] = True
    try:
        s.close()
    except Exception:
        pass


if __name__ == "__main__":
    main()
