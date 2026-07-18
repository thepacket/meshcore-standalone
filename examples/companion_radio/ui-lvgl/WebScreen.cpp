// SPDX-License-Identifier: GPL-3.0-or-later
// The mirror protocol, WebSocket handshake/frame code, and browser page are
// ported and adapted from wadamesh (Kaj Schittecat and contributors),
// GPL-3.0-or-later. See LICENSE and NOTICE at the repo root.
#include "WebScreen.h"
#include "WebMirror.h"
#include <Arduino.h>
#include <WiFi.h>
#include <esp_heap_caps.h>
#include <string.h>
#include "mbedtls/sha1.h"
#include "mbedtls/base64.h"

WebScreen g_web_screen;

// ---- tunables ------------------------------------------------------------
#define WS_MAX_CLIENTS      3
#define WS_HS_MAX           1200          // handshake request buffer
#define WS_BAND_CAP         8192          // max popped mirror band (hdr + body)
#define WS_WRITE_TIMEOUT_MS 400           // per-frame socket write budget before a client is dropped
#define WS_HANDSHAKE_TO_MS  4000          // drop a client that never finishes its HTTP upgrade
static const char WS_MAGIC[] = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11";

// ---- self-contained browser page (served at GET /) -----------------------
// Opens ws://<host>/mirror?pin=NNNN, paints RGB565 (LE) bands the device streams
// onto a <canvas>, and forwards pointer taps + keystrokes back. No external deps
// (LAN, plain HTTP). PIN is taken from the URL #hash or prompted for once.
static const char WS_PAGE[] =
  "HTTP/1.1 200 OK\r\n"
  "Content-Type: text/html; charset=utf-8\r\n"
  "Connection: close\r\n"
  "\r\n"
  "<!DOCTYPE html><html><head><meta charset=utf-8>\n"
  "<meta name=viewport content='width=device-width,initial-scale=1,maximum-scale=1,user-scalable=no'>\n"
  "<title>T-Deck remote</title>\n"
  "<style>\n"
  "html,body{margin:0;height:100%;background:#0b0b0c;color:#e8e8ea;font-family:system-ui,-apple-system,sans-serif;overflow:hidden;-webkit-user-select:none;user-select:none;touch-action:none}\n"
  "#w{display:flex;flex-direction:column;align-items:center;gap:8px;padding-top:8px;box-sizing:border-box}\n"
  "#c{background:#000;image-rendering:pixelated;box-shadow:0 0 26px #000a;border-radius:8px}\n"
  "#s{font-size:13px;opacity:.75} #s.ok{color:#19d6c2} #s.err{color:#ff6b6b}\n"
  "#kb{padding:9px 18px;font-size:14px;background:#1c1c1f;color:#e8e8ea;border:1px solid #333;border-radius:8px}\n"
  "#kb:active{background:#19d6c2;color:#000;border-color:#19d6c2}\n"
  "#k{position:fixed;top:0;left:0;width:1px;height:1px;opacity:0;border:0;padding:0;font-size:16px}\n"
  "</style></head><body>\n"
  "<div id=w>\n"
  "<canvas id=c width=320 height=240></canvas>\n"
  "<div id=s>connecting...</div>\n"
  "<button id=kb>&#9000; Keyboard</button>\n"
  "<textarea id=k autocomplete=off autocorrect=off autocapitalize=off spellcheck=false></textarea>\n"
  "</div>\n"
  "<script>\n"
  "var C=document.getElementById('c'),X=C.getContext('2d'),S=document.getElementById('s');\n"
  "var DW=320,DH=240,down=false,last=0,ws,opened=false;\n"
  "var PIN=location.hash.replace('#','');\n"
  "function st(t,k){S.textContent=t;S.className=k||''}\n"
  "function conn(){\n"
  " if(!PIN){PIN=prompt('Device PIN')||'';location.hash=PIN}\n"
  " opened=false;\n"
  " ws=new WebSocket((location.protocol=='https:'?'wss://':'ws://')+location.host+'/mirror?pin='+encodeURIComponent(PIN));\n"
  " ws.binaryType='arraybuffer';\n"
  " ws.onopen=function(){opened=true;st('connected','ok')};\n"
  " ws.onclose=function(){st(opened?'disconnected - retrying':'wrong PIN?','err');if(!opened){PIN='';location.hash=''}setTimeout(conn,1200)};\n"
  " ws.onerror=function(){};\n"
  " ws.onmessage=function(e){\n"
  "  var a=new Uint8Array(e.data);\n"
  "  if(a[0]==2){DW=a[1]|(a[2]<<8);DH=a[3]|(a[4]<<8);if(C.width!=DW)C.width=DW;if(C.height!=DH)C.height=DH;fit();return}\n"
  "  if(a[0]==1){\n"
  "   var fl=a[1],x=a[2]|(a[3]<<8),y=a[4]|(a[5]<<8),w=a[6]|(a[7]<<8),h=a[8]|(a[9]<<8);\n"
  "   var img=X.createImageData(w,h),d=img.data,L=d.length,p=10,o=0,v,r,g,b,c;\n"
  "   if(fl&1){\n"
  "    while(o<L){c=a[p++];v=a[p]|(a[p+1]<<8);p+=2;r=(v>>8)&0xf8;g=(v>>3)&0xfc;b=(v<<3)&0xf8;\n"
  "     while(c-->0){d[o++]=r;d[o++]=g;d[o++]=b;d[o++]=255}}\n"
  "   }else{\n"
  "    while(o<L){v=a[p]|(a[p+1]<<8);p+=2;d[o++]=(v>>8)&0xf8;d[o++]=(v>>3)&0xfc;d[o++]=(v<<3)&0xf8;d[o++]=255}\n"
  "   }\n"
  "   X.putImageData(img,x,y);\n"
  "  }\n"
  " }\n"
  "}\n"
  "function xy(e){var r=C.getBoundingClientRect();\n"
  " var x=(e.clientX-r.left)*DW/r.width,y=(e.clientY-r.top)*DH/r.height;\n"
  " x=x<0?0:x>=DW?DW-1:x;y=y<0?0:y>=DH?DH-1:y;return[x|0,y|0]}\n"
  "function snd(x,y,pr){if(!ws||ws.readyState!=1)return;ws.send(new Uint8Array([1,x&255,x>>8,y&255,y>>8,pr]))}\n"
  "C.addEventListener('pointerdown',function(e){down=true;try{C.setPointerCapture(e.pointerId)}catch(x){}var p=xy(e);snd(p[0],p[1],1);e.preventDefault()});\n"
  "C.addEventListener('pointermove',function(e){if(!down)return;var t=Date.now();if(t-last<33)return;last=t;var p=xy(e);snd(p[0],p[1],1)});\n"
  "function up(e){if(!down)return;down=false;var p=xy(e);snd(p[0],p[1],0)}\n"
  "C.addEventListener('pointerup',up);C.addEventListener('pointercancel',up);\n"
  "function skey(cp){if(!ws||ws.readyState!=1)return;ws.send(new Uint8Array([2,cp&255,(cp>>8)&255]))}\n"
  "var K=document.getElementById('k');\n"
  "document.getElementById('kb').addEventListener('click',function(){K.value=' ';K.focus();try{K.setSelectionRange(1,1)}catch(x){}});\n"
  "K.addEventListener('beforeinput',function(e){var t=e.inputType;\n"
  " if(t=='insertText'&&e.data){for(var i=0;i<e.data.length;i++)skey(e.data.codePointAt(i));e.preventDefault()}\n"
  " else if(t=='deleteContentBackward'){skey(8);e.preventDefault()}\n"
  " else if(t=='insertLineBreak'||t=='insertParagraph'){skey(13);e.preventDefault()}\n"
  " K.value=' ';try{K.setSelectionRange(1,1)}catch(x){}});\n"
  "window.addEventListener('keydown',function(e){\n"
  " if(document.activeElement===K)return;if(e.ctrlKey||e.metaKey||e.altKey)return;\n"
  " if(e.key=='Backspace'){skey(8);e.preventDefault()}\n"
  " else if(e.key=='Enter'){skey(13);e.preventDefault()}\n"
  " else if(e.key=='Escape'){skey(27);e.preventDefault()}\n"
  " else if(e.key.length==1){skey(e.key.codePointAt(0));e.preventDefault()}});\n"
  "function fit(){var vw=window.innerWidth,vh=window.innerHeight,aw=vw-8,ah=vh-72;\n"
  " var s=Math.min(aw/DW,ah/DH);if(s<0.2)s=0.2;\n"
  " C.style.width=Math.round(DW*s)+'px';C.style.height=Math.round(DH*s)+'px'}\n"
  "window.addEventListener('resize',fit);fit();conn();\n"
  "</script></body></html>";

// ---- per-client state ----------------------------------------------------
enum { HS0, HS1, LEXT, MSK, PAY };   // WS incoming-frame parse states
struct Cli {
  WiFiClient sock;
  bool     inUse = false;
  bool     hsDone = false;
  bool     isMirror = false;
  bool     metaSent = false;
  uint32_t acceptMs = 0;
  char     hs[WS_HS_MAX];
  int      hsLen = 0;
  uint8_t  st = HS0, opcode = 0, mask[4];
  uint64_t plen = 0, pread = 0;
  uint8_t  rx[16];
  int      rxLen = 0;
};

// ---- small socket helpers ------------------------------------------------
static bool writeAll(WiFiClient& c, const uint8_t* p, size_t len, uint32_t to_ms) {
  size_t sent = 0; uint32_t t0 = millis();
  while (sent < len) {
    if (!c.connected()) return false;
    size_t n = c.write(p + sent, len - sent);
    if (n > 0) { sent += n; t0 = millis(); }
    else { if (millis() - t0 > to_ms) return false; delay(1); }
  }
  return true;
}

// One FIN+binary WS frame (server->client frames are unmasked).
static bool sendWsBinary(WiFiClient& c, const uint8_t* data, size_t len) {
  uint8_t h[4]; size_t hl;
  h[0] = 0x82;                                    // FIN | binary
  if (len < 126) { h[1] = (uint8_t)len; hl = 2; }
  else { h[1] = 126; h[2] = (len >> 8) & 0xFF; h[3] = len & 0xFF; hl = 4; }
  if (!writeAll(c, h, hl, WS_WRITE_TIMEOUT_MS)) return false;
  return writeAll(c, data, len, WS_WRITE_TIMEOUT_MS);
}

// PIN check against the request-target query (first line of the HTTP request).
static bool pinOk(const char* req, const char* pin) {
  if (!pin || !pin[0]) return false;               // no PIN set -> deny (never happens when enabled)
  const char* nl = strchr(req, '\n');              // limit the search to the request line
  const char* q = strstr(req, "pin=");
  if (!q || (nl && q > nl)) return false;
  q += 4;
  char got[8]; int i = 0;
  while (*q && *q != ' ' && *q != '&' && *q != '\r' && *q != '\n' && i < 7) got[i++] = *q++;
  got[i] = '\0';
  return strcmp(got, pin) == 0;
}

// Complete the HTTP/WS handshake once the request headers have arrived.
// Routes: GET /mirror?pin=OK -> 101 upgrade (mirror socket); GET / -> serve the
// page then close; anything else -> 403/close.
static void finishHandshake(Cli& c, const char* pin) {
  // locate the Sec-WebSocket-Key
  const char* key = nullptr; size_t keyLen = 0;
  for (size_t i = 0; i + 20 < (size_t)c.hsLen; i++) {
    if (strncasecmp(c.hs + i, "Sec-WebSocket-Key:", 18) == 0) {
      const char* ks = c.hs + i + 18;
      while (*ks == ' ' || *ks == '\t') ks++;
      const char* ke = ks;
      while (*ke && *ke != '\r' && *ke != '\n') ke++;
      key = ks; keyLen = ke - ks; break;
    }
  }
  bool wantsMirror = (strncmp(c.hs, "GET /mirror", 11) == 0);

  if (key && keyLen && keyLen <= 128 && wantsMirror) {
    if (!pinOk(c.hs, pin)) {
      const char* deny = "HTTP/1.1 403 Forbidden\r\nConnection: close\r\n\r\n";
      writeAll(c.sock, (const uint8_t*)deny, strlen(deny), WS_WRITE_TIMEOUT_MS);
      c.sock.stop(); c.inUse = false; return;
    }
    char concat[128 + sizeof(WS_MAGIC)];
    memcpy(concat, key, keyLen);
    memcpy(concat + keyLen, WS_MAGIC, sizeof(WS_MAGIC) - 1);
    uint8_t hash[20];
    mbedtls_sha1((const unsigned char*)concat, keyLen + sizeof(WS_MAGIC) - 1, hash);
    unsigned char b64[32]; size_t olen = 0;
    mbedtls_base64_encode(b64, sizeof(b64), &olen, hash, 20);
    const char* resp = "HTTP/1.1 101 Switching Protocols\r\n"
                       "Upgrade: websocket\r\nConnection: Upgrade\r\n"
                       "Sec-WebSocket-Accept: ";
    if (!writeAll(c.sock, (const uint8_t*)resp, strlen(resp), WS_WRITE_TIMEOUT_MS) ||
        !writeAll(c.sock, b64, olen, WS_WRITE_TIMEOUT_MS) ||
        !writeAll(c.sock, (const uint8_t*)"\r\n\r\n", 4, WS_WRITE_TIMEOUT_MS)) {
      c.sock.stop(); c.inUse = false; return;
    }
    c.sock.setNoDelay(true);
    c.isMirror = true; c.hsDone = true; c.st = HS0;
    g_web_mirror.requestFullRepaint();     // new viewer wants a complete frame
    return;
  }

  // Plain HTTP GET (or a non-mirror upgrade): serve the page, then close.
  writeAll(c.sock, (const uint8_t*)WS_PAGE, sizeof(WS_PAGE) - 1, WS_WRITE_TIMEOUT_MS);
  c.sock.stop(); c.inUse = false;
}

// Feed available handshake bytes; returns once headers are complete (or the
// client is dropped). Non-blocking.
static void pumpHandshake(Cli& c, const char* pin) {
  while (c.sock.available() && c.hsLen < WS_HS_MAX - 1) {
    char ch = (char)c.sock.read();
    c.hs[c.hsLen++] = ch; c.hs[c.hsLen] = '\0';
    if (c.hsLen >= 4 && c.hs[c.hsLen-4]=='\r' && c.hs[c.hsLen-3]=='\n' &&
        c.hs[c.hsLen-2]=='\r' && c.hs[c.hsLen-1]=='\n') {
      finishHandshake(c, pin);
      return;
    }
  }
  if (c.hsLen >= WS_HS_MAX - 1) { c.sock.stop(); c.inUse = false; }
}

// Parse masked WS frames from a mirror client -> pointer/key events.
// Input payloads: pointer [0x01, x,y (LE16), pressed]; key [0x02, cp (LE16)].
static void drainInput(Cli& c) {
  int guard = 0;
  while (c.sock.available() && guard++ < 512) {
    switch (c.st) {
      case HS0: c.opcode = (uint8_t)c.sock.read() & 0x0F; c.st = HS1; break;
      case HS1: {
        uint8_t b = (uint8_t)c.sock.read(); uint8_t l7 = b & 0x7F;
        c.pread = 0; c.rxLen = 0;
        if (l7 == 126 || l7 == 127) { c.plen = l7; c.st = LEXT; }
        else { c.plen = l7; c.st = MSK; }
        break;
      }
      case LEXT: {
        if (c.plen == 126) {
          if (c.sock.available() < 2) return;
          uint8_t hi = (uint8_t)c.sock.read(), lo = (uint8_t)c.sock.read();
          c.plen = ((uint16_t)hi << 8) | lo;
        } else {
          if (c.sock.available() < 8) return;
          c.plen = 0;
          for (int i = 0; i < 8; i++) c.plen = (c.plen << 8) | (uint8_t)c.sock.read();
        }
        c.st = (c.plen == 0) ? HS0 : MSK;
        break;
      }
      case MSK: {
        if (c.sock.available() < 4) return;
        for (int i = 0; i < 4; i++) c.mask[i] = (uint8_t)c.sock.read();
        c.st = (c.plen == 0) ? HS0 : PAY;
        break;
      }
      default: {  // PAY
        uint8_t b = (uint8_t)c.sock.read() ^ c.mask[c.pread % 4];
        if (c.rxLen < (int)sizeof(c.rx)) c.rx[c.rxLen++] = b;
        c.pread++;
        if (c.pread >= c.plen) {
          if (c.opcode == 0x02 && c.rxLen >= 1) {          // binary
            uint8_t ty = c.rx[0];
            if (ty == 0x01 && c.rxLen >= 6) {
              int16_t x = (int16_t)(c.rx[1] | (c.rx[2] << 8));
              int16_t y = (int16_t)(c.rx[3] | (c.rx[4] << 8));
              g_web_mirror.pushPointer(x, y, c.rx[5]);
            } else if (ty == 0x02 && c.rxLen >= 3) {
              g_web_mirror.pushKey((uint16_t)(c.rx[1] | (c.rx[2] << 8)));
            }
          } else if (c.opcode == 0x08) {                   // close
            c.sock.stop(); c.inUse = false; return;
          }
          c.st = HS0;
        }
        break;
      }
    }
  }
}

// ---- FreeRTOS task -------------------------------------------------------
static void webScreenTaskEntry(void* arg) {
  static_cast<WebScreen*>(arg)->taskLoop();
  vTaskDelete(nullptr);
}

void WebScreen::taskLoop() {
  WiFiServer* srv = nullptr;
  static Cli clients[WS_MAX_CLIENTS];
  uint8_t* band = (uint8_t*)heap_caps_malloc(WS_BAND_CAP, MALLOC_CAP_SPIRAM | MALLOC_CAP_8BIT);
  if (!band) band = (uint8_t*)malloc(WS_BAND_CAP);

  for (;;) {
    bool want = _enabled && band && WiFi.status() == WL_CONNECTED;

    if (want && !srv) {
      srv = new WiFiServer(_port);
      srv->begin();
      srv->setNoDelay(true);
    }
    if (!want && srv) {
      for (int i = 0; i < WS_MAX_CLIENTS; i++) if (clients[i].inUse) { clients[i].sock.stop(); clients[i] = Cli(); }
      srv->end(); delete srv; srv = nullptr;
      _clients = 0; g_web_mirror.noteClients(0);
    }
    if (!srv) { vTaskDelay(pdMS_TO_TICKS(200)); continue; }

    // accept
    while (srv->hasClient()) {
      WiFiClient nc = srv->accept();
      if (!nc) break;
      int slot = -1;
      for (int i = 0; i < WS_MAX_CLIENTS; i++) if (!clients[i].inUse) { slot = i; break; }
      if (slot < 0) { nc.stop(); continue; }             // full
      clients[slot] = Cli();
      clients[slot].sock = nc;
      clients[slot].inUse = true;
      clients[slot].acceptMs = millis();
    }

    // service each client
    uint32_t now = millis();
    for (int i = 0; i < WS_MAX_CLIENTS; i++) {
      Cli& c = clients[i];
      if (!c.inUse) continue;
      if (!c.sock.connected()) { c.sock.stop(); c = Cli(); continue; }
      if (!c.hsDone) {
        if (now - c.acceptMs > WS_HANDSHAKE_TO_MS) { c.sock.stop(); c = Cli(); continue; }
        pumpHandshake(c, _pin);
        continue;
      }
      if (c.isMirror) {
        if (!c.metaSent) {
          uint8_t meta[6] = { 0x02,
            (uint8_t)(g_web_mirror.screenW() & 0xFF), (uint8_t)(g_web_mirror.screenW() >> 8),
            (uint8_t)(g_web_mirror.screenH() & 0xFF), (uint8_t)(g_web_mirror.screenH() >> 8), 0 };
          if (!sendWsBinary(c.sock, meta, 6)) { c.sock.stop(); c = Cli(); continue; }
          c.metaSent = true;
        }
        drainInput(c);
      }
    }

    // count mirror viewers + publish to the bridge
    int mcount = 0;
    for (int i = 0; i < WS_MAX_CLIENTS; i++)
      if (clients[i].inUse && clients[i].hsDone && clients[i].isMirror) mcount++;
    _clients = mcount; g_web_mirror.noteClients(mcount);

    // drain the framebuffer ring to every mirror viewer (single consumer here)
    if (mcount > 0) {
      size_t n;
      while ((n = g_web_mirror.popFrame(band, WS_BAND_CAP)) > 0) {
        for (int i = 0; i < WS_MAX_CLIENTS; i++) {
          Cli& c = clients[i];
          if (!c.inUse || !c.isMirror || !c.metaSent) continue;
          if (!sendWsBinary(c.sock, band, n)) { c.sock.stop(); c = Cli(); }
        }
      }
    }

    vTaskDelay(pdMS_TO_TICKS(3));
  }
}

void WebScreen::begin(uint16_t port) {
  _port = port;
  if (_task_started) return;
  _task_started = true;
  // Pin to core 0 (PRO_CPU, where lwIP/Wi-Fi live) so socket I/O never steals
  // time from the LVGL/mesh loop on core 1.
  xTaskCreatePinnedToCore(webScreenTaskEntry, "webscreen", 4096, this, 1, nullptr, 0);
}

void WebScreen::setEnabled(bool en) { _enabled = en; }

void WebScreen::setPin(const char* pin) {
  if (!pin) { _pin[0] = '\0'; return; }
  strncpy(_pin, pin, sizeof(_pin) - 1);
  _pin[sizeof(_pin) - 1] = '\0';
}
