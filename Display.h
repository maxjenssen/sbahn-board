#pragma once
#include <Arduino.h>

class Display {
public:
  void begin();
  void showResting(const String &text);
  void scrollLine(const String &text);
  void setBrightness(int level);  // 0..15; no-op unless changed

private:
  String lastResting;
  int lastBrightness = -1;
};
