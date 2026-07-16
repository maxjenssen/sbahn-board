#pragma once
#include <Arduino.h>

class Display {
public:
  void begin();
  void showResting(const String &text);
  void scrollLine(const String &text);
  void setBrightness(int level);  // 0..15; no-op unless changed
  void heartbeat(bool pixelOn);   // idle mode: dark screen, one random pixel while on

private:
  String lastResting;
  int lastBrightness = -1;
  bool hbOn = false;
  bool hbInit = false;
};
