#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <time.h>
#include <TZ.h>
#include "Config.h"
#include "BoardLogic.h"
#include "MvgClient.h"
#include "Display.h"

MvgClient mvg;
Display display;

Departure deps[MAX_DEPARTURES];
int depCount = 0;
bool haveFetched = false;
bool fetchAttempted = false;
unsigned long lastFetchMs = 0;
unsigned long lastSuccessMs = 0;
unsigned long lastScrollMs = 0;

void setup() {
  Serial.begin(115200);
  delay(200);
  Serial.println("\nsbahn-board");
  display.begin();
#if DISPLAY_TEST
  Serial.println("DISPLAY_TEST: showing 'S1 12'");
  display.showResting("S1 12");
  return;
#endif
  display.showResting("WiFi?");
  WiFiManager wm;
  wm.setConfigPortalTimeout(300);
  wm.setConnectTimeout(20);  // per-attempt cap
  wm.setConnectRetries(3);   // ride out transient AP/router hiccups before portal fallback
  if (!wm.autoConnect(AP_NAME)) ESP.restart();
  Serial.print("WiFi connected, IP: ");
  Serial.println(WiFi.localIP());
  configTime(TZ_Europe_Berlin, "pool.ntp.org");  // UTC epoch unchanged; enables DST-aware localtime()
  display.showResting("sync");
  lastSuccessMs = millis();  // start the watchdog clock at boot
}

void fetchNow() {
  Departure fresh[MAX_DEPARTURES];
  int n = 0;
  if (mvg.fetch(fresh, MAX_DEPARTURES, n)) {
    for (int i = 0; i < n; i++) deps[i] = fresh[i];
    depCount = n;
    haveFetched = true;
    lastSuccessMs = millis();
    time_t t = time(nullptr);
    struct tm lt;
    localtime_r(&t, &lt);
    Serial.printf("fetch ok: %d departures, heap %u, local %02d:%02d\n",
                  n, ESP.getFreeHeap(), lt.tm_hour, lt.tm_min);
  } else {
    Serial.printf("fetch FAILED, heap %u, maxblock %u\n",
                  ESP.getFreeHeap(), ESP.getMaxFreeBlockSize());
  }
}

void loop() {
#if DISPLAY_TEST
  delay(1000);
  return;
#endif
  unsigned long ms = millis();

  if (ms - lastSuccessMs > WATCHDOG_S * 1000UL) {
    Serial.println("watchdog: no successful fetch, restarting");
    ESP.restart();
  }

  time_t now = time(nullptr);
  if (!isTimeSynced(now)) {
    display.showResting("sync");
    delay(250);
    return;
  }

  struct tm lt;
  localtime_r(&now, &lt);
  display.setBrightness(inNightWindow(lt.tm_hour, NIGHT_START_HOUR, NIGHT_END_HOUR)
                            ? NIGHT_BRIGHTNESS
                            : BRIGHTNESS);

  if (!fetchAttempted || ms - lastFetchMs >= FETCH_INTERVAL_S * 1000UL) {
    fetchAttempted = true;
    lastFetchMs = ms;
    fetchNow();
    ms = millis();          // fetch blocks for seconds; re-sync timers
    now = time(nullptr);
  }

  bool stale = !haveFetched || (ms - lastSuccessMs) > STALE_S * 1000UL;

  if (haveFetched && !stale && ms - lastScrollMs >= SCROLL_INTERVAL_S * 1000UL) {
    lastScrollMs = ms;
    display.scrollLine(formatScrollLine(deps, depCount, now));
  }

  display.showResting(formatResting(deps, depCount, now, stale));
  delay(250);
}
