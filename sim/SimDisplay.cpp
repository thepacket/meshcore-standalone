#include "SimDisplay.h"

void SimDisplay::startFrame(Color bkg) {
  SDL_SetRenderDrawColor(_ren, 0, 0, 0, 255);  // DARK background
  SDL_RenderClear(_ren);
}

void SimDisplay::setColor(Color c) {
  switch (c) {
    case DARK:   _col = {0, 0, 0, 255}; break;
    case LIGHT:  _col = {235, 235, 235, 255}; break;
    case RED:    _col = {230, 60, 60, 255}; break;
    case GREEN:  _col = {60, 210, 90, 255}; break;
    case BLUE:   _col = {70, 120, 230, 255}; break;
    case YELLOW: _col = {235, 215, 70, 255}; break;
    case ORANGE: _col = {235, 150, 50, 255}; break;
    default:     _col = {235, 235, 235, 255};
  }
}

void SimDisplay::print(const char* str) {
  if (!str || !*str) return;
  SDL_Surface* surf = TTF_RenderUTF8_Blended(_cur, str, _col);
  if (!surf) return;
  SDL_Texture* tex = SDL_CreateTextureFromSurface(_ren, surf);
  SDL_Rect dst{_cx, _cy, surf->w, surf->h};
  SDL_RenderCopy(_ren, tex, nullptr, &dst);
  _cx += surf->w;
  SDL_DestroyTexture(tex);
  SDL_FreeSurface(surf);
}

void SimDisplay::fillRect(int x, int y, int w, int h) {
  SDL_SetRenderDrawColor(_ren, _col.r, _col.g, _col.b, 255);
  SDL_Rect r{x, y, w, h};
  SDL_RenderFillRect(_ren, &r);
}

void SimDisplay::drawRect(int x, int y, int w, int h) {
  SDL_SetRenderDrawColor(_ren, _col.r, _col.g, _col.b, 255);
  SDL_Rect r{x, y, w, h};
  SDL_RenderDrawRect(_ren, &r);
}

uint16_t SimDisplay::getTextWidth(const char* str) {
  if (!str || !*str) return 0;
  int w = 0, h = 0;
  TTF_SizeUTF8(_cur, str, &w, &h);
  return (uint16_t)w;
}

void SimDisplay::endFrame() {
  SDL_RenderPresent(_ren);
}
