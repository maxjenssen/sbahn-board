#pragma once
#ifdef HOST_TEST
  #include "test/StringShim.h"
#else
  #include <Arduino.h>
#endif
#include <time.h>

struct Departure {
  String label;          // "S1"
  String destination;    // display-ready ASCII (transliterated in MvgClient)
  time_t realtimeEpoch;  // realtimeDepartureTime / 1000
  int delayMin;          // delayInMinutes, 0 if absent
};

// City-bound filter: everything except airport/Freising direction.
static const char *const EXCLUDE_DEST[] = {"Flughafen", "Freising"};
static const int EXCLUDE_DEST_COUNT = 2;

inline String transliterate(String s) {
  const char *from[] = {"\xC3\xA4", "\xC3\xB6", "\xC3\xBC", "\xC3\x9F",
                        "\xC3\x84", "\xC3\x96", "\xC3\x9C"};
  const char *to[] = {"ae", "oe", "ue", "ss", "Ae", "Oe", "Ue"};
  for (int i = 0; i < 7; i++) s.replace(from[i], to[i]);
  return s;
}

inline int minutesUntil(time_t dep, time_t now) {
  if (dep <= now) return 0;
  return (int)((dep - now) / 60);
}

inline bool isTimeSynced(time_t now) { return now > 1000000000; }

// Half-open local-hour window [startHour, endHour), wrapping midnight when
// startHour > endHour (e.g. 22..5). Equal start/end means "never".
inline bool inNightWindow(int hour, int startHour, int endHour) {
  if (startHour <= endHour) return hour >= startHour && hour < endHour;
  return hour >= startHour || hour < endHour;
}

// True when nothing departs within thresholdMin minutes — the board goes
// idle (heartbeat) instead of showing "--"/"++" through the service gap.
inline bool noUpcomingTrains(const Departure *deps, int count, time_t now,
                             int thresholdMin) {
  if (count == 0) return true;
  return minutesUntil(deps[0].realtimeEpoch, now) > thresholdMin;
}

inline bool keepDeparture(const String &transportType, const String &destination,
                          bool cancelled) {
  if (cancelled) return false;
  if (!(transportType == "SBAHN")) return false;
  for (int i = 0; i < EXCLUDE_DEST_COUNT; i++) {
    if (destination.indexOf(EXCLUDE_DEST[i]) >= 0) return false;
  }
  return true;
}

// Resting view, 5 chars max (32 px / 6 px per char). Spec table:
// stale -> "S1 ?", none -> "S1 --", >99 min -> "S1 ++", else "S1 N".
inline String formatResting(const Departure *deps, int count, time_t now, bool stale) {
  if (stale) return String("S1 ?");
  if (count == 0) return String("S1 --");
  int m = minutesUntil(deps[0].realtimeEpoch, now);
  if (m > 99) return String("S1 ++");
  return String("S1 ") + String(m);
}

// Standalone disruption marquee pass; empty in, empty out.
inline String formatDisruptionLine(const String &reason) {
  if (reason.length() == 0) return String("");
  return String("Stoerung: ") + reason;
}

// e.g. "S1 Muenchen: 7min +2 | 27min | 47min"; "+N" only when delayed.
inline String formatScrollLine(const Departure *deps, int count, time_t now) {
  if (count == 0) return String("Keine S1 Abfahrten");
  String line = "S1 Muenchen: ";
  for (int i = 0; i < count; i++) {
    if (i > 0) line = line + " | ";
    line = line + String(minutesUntil(deps[i].realtimeEpoch, now)) + "min";
    if (deps[i].delayMin >= 1) line = line + " +" + String(deps[i].delayMin);
  }
  return line;
}
