// Scroll/center rendering adapted from marquee-scroller
// (https://github.com/Qrome/marquee-scroller), MIT, (c) 2018 David Payne.
#include "Display.h"
#include "Config.h"
#include <SPI.h>
#include <Adafruit_GFX.h>
#include <Max72xxPanel.h>

static Max72xxPanel matrix(PIN_CS, NUM_PANELS, 1);

static const int SPACER = 1;
static const int CHAR_W = 5 + SPACER;

void Display::begin() {
  matrix.setIntensity(BRIGHTNESS);
  lastBrightness = BRIGHTNESS;
  for (int i = 0; i < NUM_PANELS; i++) {
    matrix.setRotation(i, LED_ROTATION);
    matrix.setPosition(i, PANEL_REVERSED ? (NUM_PANELS - 1 - i) : i, 0);
  }
  matrix.fillScreen(LOW);
  matrix.write();
}

void Display::setBrightness(int level) {
  if (level == lastBrightness) return;
  lastBrightness = level;
  matrix.setIntensity(level);
}

void Display::heartbeat(bool pixelOn) {
  if (hbInit && pixelOn == hbOn) return;  // redraw only on blink transitions
  hbInit = true;
  hbOn = pixelOn;
  lastResting = "";  // force countdown redraw when idle ends
  matrix.fillScreen(LOW);
  if (pixelOn) {
    matrix.drawPixel(random(matrix.width()), random(matrix.height()), HIGH);
  }
  matrix.write();
}

void Display::showResting(const String &text) {
  hbInit = false;  // normal rendering resumed; next idle entry redraws from scratch
  if (text == lastResting) return;
  lastResting = text;
  matrix.fillScreen(LOW);
  int x = (matrix.width() - (int)text.length() * CHAR_W) / 2;
  if (x < 0) x = 0;
  matrix.setCursor(x, 0);
  matrix.print(text);
  matrix.write();
}

void Display::scrollLine(const String &text) {
  hbInit = false;
  for (int i = 0; i < CHAR_W * (int)text.length() + matrix.width() - 1 - SPACER; i++) {
    matrix.fillScreen(LOW);
    int letter = i / CHAR_W;
    int x = (matrix.width() - 1) - i % CHAR_W;
    int y = (matrix.height() - 8) / 2;
    while (x + CHAR_W - SPACER >= 0 && letter >= 0) {
      if (letter < (int)text.length()) {
        matrix.drawChar(x, y, text[letter], HIGH, LOW, 1);
      }
      letter--;
      x -= CHAR_W;
    }
    matrix.write();
    delay(SCROLL_SPEED_MS);
  }
  matrix.fillScreen(LOW);
  matrix.write();
  lastResting = "";  // force resting redraw after scroll
}
