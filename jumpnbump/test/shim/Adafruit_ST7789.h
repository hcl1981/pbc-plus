#pragma once
#include <stdint.h>
class Adafruit_ST7789 {
 public:
  Adafruit_ST7789(int, int, int) {}
  void startWrite() {}
  void endWrite() {}
  void setAddrWindow(int, int, int, int) {}
  void writePixels(uint16_t*, uint32_t) {}
};
