#pragma once

// --- Hardware (spec: CLK->D5, DIN->D7 are fixed hardware SPI; CS is configurable) ---
#define PIN_CS D6
#define NUM_PANELS 4
#define LED_ROTATION 1       // per-module rotation 0..3 — tuned at hardware checkpoint 2
#define PANEL_REVERSED false // module order flip — tuned at hardware checkpoint 2
#define BRIGHTNESS 2        // 0..15 (day)
#define NIGHT_BRIGHTNESS 0  // 0..15 — dimmed level inside the night window
#define NIGHT_START_HOUR 22 // local (DST-aware) hour the dim window opens
#define NIGHT_END_HOUR 5    // local hour it closes; set equal to start to disable
#define SCROLL_SPEED_MS 25

// --- Data source (station de:09178:2650 = Eching, S1) ---
#define STATION_ID "de:09178:2650"
#define MVG_URL "https://www.mvg.de/api/bgw-pt/v3/departures?globalId=" STATION_ID "&limit=10&transportTypes=SBAHN"
#define MAX_DEPARTURES 3

// --- Timing (seconds) ---
#define FETCH_INTERVAL_S 60
#define SCROLL_INTERVAL_S 30
#define STALE_S 180
#define WATCHDOG_S 900

// --- IRIS fallback (DB's keyless open timetable API; survives MVG outages) ---
#define STATION_EVA "8001647"           // DB EVA number of the station (Eching)
#define IRIS_HOST "https://iris.noncd.db.de/iris-tts/timetable"
#define IRIS_KEEP_PPTH "M\xC3\xBCnchen" // keep trains whose departure path contains this
#define MVG_FAIL_STREAK 3               // switch to IRIS after this many MVG failures
#define MVG_RETRY_EVERY 10              // while on IRIS, retry MVG every Nth cycle

// --- Rain warning (Open-Meteo, keyless) ---
#define WEATHER_LAT "48.303149"       // Eching station; override in ConfigLocal.h
#define WEATHER_LON "11.617184"
#define RAIN_MM_THRESHOLD 0.1f        // 15-min precipitation that counts as rain
#define WEATHER_FETCH_INTERVAL_S 900  // forecast refresh (12 h horizon, 15-min slots)
#define RAIN_SCROLL_INTERVAL_S 300    // rain announcement cadence

// --- Breaking news (Tagesschau Eilmeldungen, keyless) ---
#define NEWS_URL "https://www.tagesschau.de/api2u/news"
#define NEWS_FETCH_INTERVAL_S 600  // poll cadence
#define NEWS_SCROLL_INTERVAL_S 600 // repeat cadence while an Eilmeldung is active
#define NEWS_MAX_LEN 100
#define NEWS_READ_CAP 24576        // scan only the feed head; Eilmeldungen sort first

// --- Disruption alert ---
#define DISRUPTION_CYCLE_S 15 // alert + reason replay interval while disrupted
#define ALERT_BLINKS 3        // "!!!" flashes at the start of each cycle
#define DISRUPTION_MAX_LEN 100 // cap for the scrolled reason text

// --- Night idle ---
#define NO_TRAIN_OFF_THRESHOLD_MIN 90 // blank the display when nothing departs within this window
#define HEARTBEAT_PERIOD_S 5          // one blink cycle while idle
#define HEARTBEAT_ON_MS 1000          // random pixel lit this long per cycle

// --- WiFi ---
#define AP_NAME "SBahnBoard"

// --- Bring-up ---
// 1 = skip WiFi entirely and show static "S1 12" for tuning
// LED_ROTATION / PANEL_REVERSED. 0 = normal operation.
#define DISPLAY_TEST 0

// --- Private overrides ---
// Create a gitignored ConfigLocal.h with #undef/#define pairs to keep
// personal values (e.g. your real STATION_ID) out of the repo.
#if __has_include("ConfigLocal.h")
#include "ConfigLocal.h"
#endif
