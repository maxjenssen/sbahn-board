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
