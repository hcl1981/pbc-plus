#pragma once
#include <stdint.h>
#include <stdlib.h>
class GFXcanvas16 {
 public:
  GFXcanvas16(int w, int h) : _w(w), _h(h) { _b = (uint16_t*)calloc(w*h, 2); }
  uint16_t* getBuffer() { return _b; }
  int width() const { return _w; }
  int height() const { return _h; }
 private:
  int _w, _h; uint16_t* _b;
};
