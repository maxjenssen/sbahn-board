#include <Arduino.h>
#include <ESP8266WiFi.h>
#include <WiFiManager.h>
#include <time.h>
#include <TZ.h>
#include "Config.h"
#include "BoardLogic.h"
#include "MvgClient.h"
#include "IrisClient.h"
#include "WeatherClient.h"
#include "NewsClient.h"
#include "Display.h"

MvgClient mvg;
IrisClient iris;
WeatherClient weather;
NewsClient news;
Display display;

Departure deps[MAX_DEPARTURES];
int depCount = 0;
String disruptionMsg;
bool haveFetched = false;
bool fetchAttempted = false;
int mvgFailStreak = 0;
bool onIris = false;
unsigned int fetchCycle = 0;
unsigned long lastFetchMs = 0;
unsigned long lastSuccessMs = 0;
unsigned long lastScrollMs = 0;
unsigned long lastAlertMs = 0;
time_t rainStartEpoch = 0;
bool weatherAttempted = false;
unsigned long lastWeatherMs = 0;
unsigned long lastRainScrollMs = 0;
String breakingMsg;
String lastShownBreaking;
bool newsAttempted = false;
unsigned long lastNewsMs = 0;
unsigned long lastNewsScrollMs = 0;

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
  String reason;
  bool ok = false;
  const char *src = "MVG";
  fetchCycle++;

  // MVG is primary; after MVG_FAIL_STREAK consecutive failures switch to
  // IRIS (independent DB infrastructure) and probe MVG every Nth cycle.
  bool tryMvg = !onIris || (fetchCycle % MVG_RETRY_EVERY == 0);
  if (tryMvg) {
    ok = mvg.fetch(fresh, MAX_DEPARTURES, n, reason);
    if (ok) {
      mvgFailStreak = 0;
      if (onIris) {
        onIris = false;
        Serial.println("failback: MVG recovered, leaving IRIS mode");
      }
    } else if (!onIris && ++mvgFailStreak >= MVG_FAIL_STREAK) {
      onIris = true;
      Serial.println("failover: MVG down, switching to IRIS");
    }
  }
  if (!ok && onIris) {
    src = "IRIS";
    ok = iris.fetch(fresh, MAX_DEPARTURES, n, reason);
  }

  if (ok) {
    for (int i = 0; i < n; i++) deps[i] = fresh[i];
    depCount = n;
    disruptionMsg = reason;
    haveFetched = true;
    lastSuccessMs = millis();
    time_t t = time(nullptr);
    struct tm lt;
    localtime_r(&t, &lt);
    Serial.printf("fetch ok (%s): %d departures, heap %u, local %02d:%02d\n",
                  src, n, ESP.getFreeHeap(), lt.tm_hour, lt.tm_min);
    if (disruptionMsg.length()) {
      Serial.println("  disruption: " + disruptionMsg);
    }
  } else {
    Serial.printf("fetch FAILED (%s), heap %u, maxblock %u\n", src,
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

  if (!weatherAttempted || ms - lastWeatherMs >= WEATHER_FETCH_INTERVAL_S * 1000UL) {
    weatherAttempted = true;
    lastWeatherMs = ms;
    if (weather.fetch(rainStartEpoch)) {
      String r = formatRainLine(rainStartEpoch, now);
      Serial.printf("weather ok: %s\n",
                    r.length() ? r.c_str() : "kein Regen (12h)");
    }
    ms = millis();  // weather fetch also blocks; re-sync
    now = time(nullptr);
  }

  if (!newsAttempted || ms - lastNewsMs >= NEWS_FETCH_INTERVAL_S * 1000UL) {
    newsAttempted = true;
    lastNewsMs = ms;
    String b;
    if (news.fetch(b)) {
      breakingMsg = b;
      Serial.printf("news ok: %s\n",
                    b.length() ? b.c_str() : "keine Eilmeldung");
    }
    ms = millis();  // news fetch also blocks; re-sync
    now = time(nullptr);
  }

  bool stale = !haveFetched || (ms - lastSuccessMs) > STALE_S * 1000UL;
  bool disrupted = !stale && disruptionMsg.length() > 0;

  // Disruption alert cycle: "!!!" flashes, then the scrolled reason, then
  // fall through to the countdown ("when's the next train").
  if (disrupted && ms - lastAlertMs >= DISRUPTION_CYCLE_S * 1000UL) {
    lastAlertMs = ms;
    display.alertBlink(ALERT_BLINKS);
    display.scrollLine(formatDisruptionLine(disruptionMsg));
    ms = millis();  // blocking animation; re-sync timers
    now = time(nullptr);
  }

  // Service gap: blank the display with a heartbeat blink instead of
  // showing "--"/"++" all night. Stale data intentionally stays visible
  // as "S1 ?", and an active disruption overrides idle — a broken line
  // must not look like peaceful sleep.
  if (!stale && !disrupted &&
      noUpcomingTrains(deps, depCount, now, NO_TRAIN_OFF_THRESHOLD_MIN)) {
    display.heartbeat((ms % (HEARTBEAT_PERIOD_S * 1000UL)) < HEARTBEAT_ON_MS);
    delay(100);
    return;  // fetch timer and watchdog already ran above
  }

  if (haveFetched && !stale && ms - lastScrollMs >= SCROLL_INTERVAL_S * 1000UL) {
    lastScrollMs = ms;
    display.scrollLine(formatScrollLine(deps, depCount, now));
    ms = millis();
    now = time(nullptr);
  }

  String rainLine = formatRainLine(rainStartEpoch, now);
  if (rainLine.length() && ms - lastRainScrollMs >= RAIN_SCROLL_INTERVAL_S * 1000UL) {
    lastRainScrollMs = ms;
    display.scrollLine(rainLine);
    ms = millis();
    now = time(nullptr);
  }

  // Eilmeldung: a NEW headline alerts immediately; an ongoing one repeats
  // every NEWS_SCROLL_INTERVAL_S. Quiet at night (idle returns above).
  if (breakingMsg.length()) {
    bool fresh = !(breakingMsg == lastShownBreaking);
    if (fresh || ms - lastNewsScrollMs >= NEWS_SCROLL_INTERVAL_S * 1000UL) {
      lastShownBreaking = breakingMsg;
      lastNewsScrollMs = ms;
      display.alertBlink(ALERT_BLINKS);
      display.scrollLine(formatBreakingLine(breakingMsg));
      ms = millis();
      now = time(nullptr);
    }
  }

  display.showResting(formatResting(deps, depCount, now, stale));
  delay(250);
}
