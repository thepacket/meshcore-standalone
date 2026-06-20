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

static const int SCALE = 3;
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

  ui.render(); save_ppm(ren, "sim/shots/01_root.ppm");
  ui.onKey(KEY_DOWN); ui.onKey(KEY_ENTER);          // open "Radio"
  ui.render(); save_ppm(ren, "sim/shots/02_radio.ppm");
  ui.onKey(KEY_DOWN); ui.onKey(KEY_DOWN); ui.onKey(KEY_ENTER);  // edit "Spread factor"
  ui.render(); save_ppm(ren, "sim/shots/03_edit.ppm");
  ui.onKey(KEY_CANCEL); ui.onKey(KEY_CANCEL);       // back to root
  ui.onKey(KEY_ENTER);                               // open "Public info"
  ui.onKey(KEY_ENTER);                               // edit "Node name" (string -> OSK)
  ui.render(); save_ppm(ren, "sim/shots/04_keyboard.ppm");

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
