// Desktop simulator entry point for the standalone settings UI.
//   interactive:  ./uisim            (mouse = touch, arrows/Enter/Esc/typing = keys)
//   headless:     ./uisim --shot     (renders a few screens to sim/shots/*.ppm and exits)
#include <SDL.h>
#include <SDL_ttf.h>
#include <helpers/ui/UIScreen.h>
#include "SimDisplay.h"
#include "UITask.h"
#include <cstdio>
#include <cstring>
#include <cmath>

static const int SCALE = 1;  // 1:1 with the real 320x240 T-Deck panel (no pixel inflation)
static const char* FONT_PATH = "/System/Library/Fonts/Menlo.ttc";

static char mapKey(SDL_Keycode k) {
  switch (k) {
    case SDLK_UP: return KEY_UP;
    case SDLK_DOWN: return KEY_DOWN;
    case SDLK_LEFT: return KEY_LEFT;
    case SDLK_RIGHT: return KEY_RIGHT;
    case SDLK_RETURN:
    case SDLK_KP_ENTER: return KEY_ENTER;
    case SDLK_ESCAPE: return KEY_CANCEL;
    case SDLK_BACKSPACE: return KEY_BACKSPACE;
    default: return 0;
  }
}

static void save_ppm(SDL_Renderer* ren, const char* path) {
  int w = 320 * SCALE, h = 240 * SCALE;
  unsigned char* buf = (unsigned char*)malloc(w * h * 3);
  if (SDL_RenderReadPixels(ren, nullptr, SDL_PIXELFORMAT_RGB24, buf, w * 3) != 0) {
    fprintf(stderr, "ReadPixels: %s\n", SDL_GetError());
  }
  FILE* f = fopen(path, "wb");
  if (f) {
    fprintf(f, "P6\n%d %d\n255\n", w, h);
    fwrite(buf, 1, w * h * 3, f);
    fclose(f);
    printf("wrote %s\n", path);
  }
  free(buf);
}

static int run_shots(TTF_Font* f1, TTF_Font* f2) {
  SDL_Surface* surf = SDL_CreateRGBSurfaceWithFormat(0, 320 * SCALE, 240 * SCALE, 24,
                                                     SDL_PIXELFORMAT_RGB24);
  SDL_Renderer* ren = SDL_CreateSoftwareRenderer(surf);
  SDL_RenderSetLogicalSize(ren, 320, 240);
  SimDisplay disp(ren, f1, f2);
  UITask ui(&disp);

  ui.render(); save_ppm(ren, "sim/shots/00_home.ppm");  // MeshOS-style launcher grid
  ui.onKey(KEY_ENTER);                               // Settings tile is selected by default
  ui.render(); save_ppm(ren, "sim/shots/01_root.ppm");
  ui.onKey(KEY_DOWN); ui.onKey(KEY_ENTER);          // open "Radio"
  ui.render(); save_ppm(ren, "sim/shots/02_radio.ppm");
  ui.onKey(KEY_ENTER);                               // tap "Preset" -> cycles in place
  ui.render(); save_ppm(ren, "sim/shots/03_edit.ppm");  // Radio now shows the next preset applied
  ui.onKey(KEY_DOWN); ui.onKey(KEY_ENTER);          // "Frequency (MHz)" -> keyboard entry (with '.')
  ui.render(); save_ppm(ren, "sim/shots/04_keyboard.ppm");

  // packet monitor: seed some fake received packets (oldest first so newest is on top)
  const uint32_t NOW = 1000;
  uint8_t adv[]  = {0x11, 0x00, /*payload*/ 1,2,3,4,5,6,7,8,9,10};                 // ADVERT FLOOD, 0 hops
  uint8_t trc[]  = {0x25, 0x04, 9,9,9,9, /*payload*/ 1,2,3,4,5};                   // TRACE FLOOD, 4 hops
  uint8_t ack[]  = {0x0E, 0x03, 0xA1,0xB2,0xC3, /*payload*/ 1,2};                  // ACK DIRECT, 3 hops
  uint8_t grp[]  = {0x14, 0x05,0x00,0x00,0x00, 0x01, 0x77, /*payload*/ 1,2,3,4,5,6,7,8}; // GRP_TXT T-FLOOD, tc, 1 hop
  uint8_t txt[]  = {0x09, 0x02, 0x11,0x22, /*payload*/ 1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16}; // TXT FLOOD, 2 hops
  ui.simAddRaw(NOW - 400, 4.75f, -99, adv, sizeof(adv));
  ui.simAddRaw(NOW - 140, 11.0f, -70, trc, sizeof(trc));
  ui.simAddRaw(NOW - 65,  2.5f, -105, ack, sizeof(ack));
  ui.simAddRaw(NOW - 17,  9.25f, -78, grp, sizeof(grp));
  ui.simAddRaw(NOW - 3,   6.0f,  -92, txt, sizeof(txt));
  ui.simSetPacketNow(NOW);
  ui.gotoPacketMonitor();
  ui.render(); save_ppm(ren, "sim/shots/05_packets.ppm");
  ui.onKey(KEY_ENTER);  // open the selected (newest) packet's detail view
  ui.render(); save_ppm(ren, "sim/shots/06_packet_detail.ppm");

  // ---- RF diagnostics: seed fake data and capture each screen ----
  ui.simSetDiagNow(NOW);

  // Noise scope: a wandering noise floor
  for (int i = 0; i < 200; i++) {
    int base = -103 + (int)(8 * sin(i * 0.15)) + ((i * 37) % 7) - 3;
    ui.simNoiseSample(base);
  }
  ui.gotoNoise();
  ui.render(); save_ppm(ren, "sim/shots/07_noise.ppm");

  // Heard: a few stations, one with GPS-derived distance
  HeardStation h1 = {"Repeater-7", 2, NOW - 12, 9 * 4, -78, 4200};
  HeardStation h2 = {"Andy-Mobile", 0, NOW - 45, 6 * 4, -92, 850};
  HeardStation h3 = {"GW-Hertford", 3, NOW - 600, 2 * 4, -108, -1};
  ui.simAddHeard(h1); ui.simAddHeard(h2); ui.simAddHeard(h3);
  ui.gotoHeard();
  ui.render(); save_ppm(ren, "sim/shots/08_heard.ppm");

  // Signal: repeaters with varying coverage
  RepeaterSignal r1 = {"Repeater-7", NOW - 12, 9 * 4, -72, true};
  RepeaterSignal r2 = {"GW-Hertford", NOW - 600, 2 * 4, -104, true};
  RepeaterSignal r3 = {"Hilltop-Relay", 0, 0, 0, false};
  ui.simAddRepeater(r1); ui.simAddRepeater(r2); ui.simAddRepeater(r3);
  ui.gotoSignal();
  ui.render(); save_ppm(ren, "sim/shots/09_signal.ppm");

  // Trace: target list, then a result
  ui.simAddTraceTarget("Repeater-7", 0);
  ui.simAddTraceTarget("GW-Hertford", 1);
  ui.gotoTrace();
  ui.render(); save_ppm(ren, "sim/shots/10_trace_pick.ppm");
  { uint8_t hashes[] = {0xA3, 0x7F, 0x12};
    uint8_t snrs[]   = {(uint8_t)(int8_t)(8*4), (uint8_t)(int8_t)(5*4), (uint8_t)(int8_t)(3*4)};
    ui.simTraceResult("Repeater-7", hashes, snrs, 3, (int8_t)(7*4)); }
  ui.render(); save_ppm(ren, "sim/shots/11_trace_result.ppm");

  // ---- Chat (M2): seed channels, DMs, and threads ----
  uint8_t peerA[6] = {0xAA, 0x01, 0x02, 0x03, 0x04, 0x05};
  uint8_t peerB[6] = {0xBB, 0x11, 0x12, 0x13, 0x14, 0x15};
  ui.chatHome()->addChannel(0, "Public");
  ui.chatHome()->addChannel(1, "#london");
  ui.chatHome()->addDm(peerA, "Andy-Mobile");
  ui.chatHome()->addDm(peerB, "GW-Hertford");

  chat::Conv* pub = ui.chatStore().getOrCreateChannel(0, "Public");
  ui.chatStore().addIncoming(pub, "Alice", "Anyone around the north side? :)", NOW - 300);
  ui.chatStore().addIncoming(pub, "Bob", "Yep, copy you 5 by 9 <3", NOW - 200);
  ui.chatStore().addOutgoing(pub, "> Alice: Anyone around the north side?\nOn the hill now B)",
                             NOW - 90, 0, 0);
  ui.chatStore().addIncoming(pub, "Carol", "Map here: https://meshcore.io", NOW - 40);

  chat::Conv* dm = ui.chatStore().getOrCreateDm(peerA, "Andy-Mobile");
  chat::Msg* a = ui.chatStore().addOutgoing(dm, "On my way over", NOW - 140, 0x1111, 0);
  a->status = chat::ST_DELIVERED;
  ui.chatStore().addIncoming(dm, "", "Great, see you soon", NOW - 110);
  chat::Msg* b = ui.chatStore().addOutgoing(dm, "ETA about 10 minutes", NOW - 50, 0x2222, 0);
  b->status = chat::ST_SENDING;
  chat::Msg* c2 = ui.chatStore().addOutgoing(dm, "(this one failed)", NOW - 20, 0, 0);
  c2->status = chat::ST_FAILED;

  ui.gotoChat();
  ui.render(); save_ppm(ren, "sim/shots/12_chat_home.ppm");
  ui.onKey(KEY_ENTER);  // open the selected channel (Public)
  ui.render(); save_ppm(ren, "sim/shots/13_chat_channel.ppm");
  ui.openConversation(false, 0, peerA, "Andy-Mobile");  // a DM thread w/ delivery status
  ui.render(); save_ppm(ren, "sim/shots/14_chat_dm.ppm");
  ui.onTouch(160, 232, TouchEvent::press);              // tap the compose bar -> open OSK
  ui.onTouch(160, 232, TouchEvent::release);
  ui.render(); save_ppm(ren, "sim/shots/15_chat_compose.ppm");

  // emoji + reply: open the Public channel (has emoji, a quote, and a URL)
  ui.openConversation(true, 0, nullptr, "Public");
  ui.render(); save_ppm(ren, "sim/shots/16_chat_emoji_reply.ppm");
  // emoji picker overlay
  ui.onTouch(312, 232, TouchEvent::press);              // tap the emoji button (far right of compose)
  ui.onTouch(312, 232, TouchEvent::release);
  ui.render(); save_ppm(ren, "sim/shots/17_emoji_picker.ppm");

  // URL -> QR: reopen Public and tap Carol's URL message (newest, near the bottom)
  ui.openConversation(true, 0, nullptr, "Public");
  ui.onTouch(80, 182, TouchEvent::press);   // tap Carol's URL bubble
  ui.onTouch(80, 182, TouchEvent::release);
  ui.render(); save_ppm(ren, "sim/shots/18_url_qr.ppm");

  SDL_DestroyRenderer(ren);
  SDL_FreeSurface(surf);
  return 0;
}

static int run_interactive(TTF_Font* f1, TTF_Font* f2) {
  SDL_Window* win = SDL_CreateWindow("MeshCore Standalone UI (sim)", SDL_WINDOWPOS_CENTERED,
                                     SDL_WINDOWPOS_CENTERED, 320 * SCALE, 240 * SCALE, 0);
  SDL_Renderer* ren = SDL_CreateRenderer(win, -1, SDL_RENDERER_ACCELERATED);
  SDL_RenderSetLogicalSize(ren, 320, 240);
  SimDisplay disp(ren, f1, f2);
  UITask ui(&disp);

  SDL_StartTextInput();
  bool running = true, mdown = false;
  while (running) {
    SDL_Event e;
    while (SDL_PollEvent(&e)) {
      switch (e.type) {
        case SDL_QUIT: running = false; break;
        case SDL_KEYDOWN: { char c = mapKey(e.key.keysym.sym); if (c) ui.onKey(c); break; }
        case SDL_TEXTINPUT:
          for (char* p = e.text.text; *p; ++p)
            if ((unsigned char)*p >= 32 && (unsigned char)*p < 127) ui.onKey(*p);
          break;
        case SDL_MOUSEBUTTONDOWN:
          if (e.button.button == SDL_BUTTON_LEFT) {
            mdown = true; int lx = e.button.x / SCALE, ly = e.button.y / SCALE;
            disp.setMouse(true, lx, ly); ui.onTouch(lx, ly, TouchEvent::press);
          }
          break;
        case SDL_MOUSEBUTTONUP:
          if (e.button.button == SDL_BUTTON_LEFT) {
            mdown = false; int lx = e.button.x / SCALE, ly = e.button.y / SCALE;
            disp.setMouse(false, lx, ly); ui.onTouch(lx, ly, TouchEvent::release);
          }
          break;
        case SDL_MOUSEMOTION:
          if (mdown) {
            int lx = e.motion.x / SCALE, ly = e.motion.y / SCALE;
            disp.setMouse(true, lx, ly); ui.onTouch(lx, ly, TouchEvent::move);
          }
          break;
      }
    }
    ui.render();
    SDL_Delay(16);
  }
  SDL_DestroyRenderer(ren);
  SDL_DestroyWindow(win);
  return 0;
}

int main(int argc, char** argv) {
  bool shot = (argc > 1 && strcmp(argv[1], "--shot") == 0);
  if (SDL_Init(SDL_INIT_VIDEO) != 0) { fprintf(stderr, "SDL_Init: %s\n", SDL_GetError()); return 1; }
  if (TTF_Init() != 0) { fprintf(stderr, "TTF_Init: %s\n", TTF_GetError()); return 1; }

  TTF_Font* f1 = TTF_OpenFont(FONT_PATH, 11);
  TTF_Font* f2 = TTF_OpenFont(FONT_PATH, 18);
  if (!f1 || !f2) { fprintf(stderr, "font load failed: %s\n", TTF_GetError()); return 1; }

  int rc = shot ? run_shots(f1, f2) : run_interactive(f1, f2);

  TTF_CloseFont(f1); TTF_CloseFont(f2);
  TTF_Quit(); SDL_Quit();
  return rc;
}
