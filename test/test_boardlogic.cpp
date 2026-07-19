#include <cassert>
#include <cstdio>
#include "../BoardLogic.h"

static Departure dep(time_t epoch, int delayMin) {
  Departure d;
  d.label = "S1";
  d.destination = "Muenchen Ost";
  d.realtimeEpoch = epoch;
  d.delayMin = delayMin;
  return d;
}

int main() {
  const time_t now = 1784106000; // any plausible epoch

  // transliterate
  assert(transliterate("M\xC3\xBCnchen Ost") == "Muenchen Ost");
  assert(transliterate("Stra\xC3\x9F""e") == "Strasse");
  assert(transliterate("\xC3\x84\xC3\x96\xC3\x9C") == "AeOeUe");
  assert(transliterate("Leuchtenbergring") == "Leuchtenbergring");

  // minutesUntil: floor, clamped at 0
  assert(minutesUntil(now + 419, now) == 6);
  assert(minutesUntil(now + 60, now) == 1);
  assert(minutesUntil(now + 59, now) == 0);
  assert(minutesUntil(now - 100, now) == 0);

  // isTimeSynced
  assert(!isTimeSynced(0));
  assert(!isTimeSynced(999999999));
  assert(isTimeSynced(1784106000));

  // keepDeparture (raw UTF-8 destinations, as the API sends them)
  assert(!keepDeparture("BUS", "Dachau (S)", false));
  assert(!keepDeparture("SBAHN", "Flughafen M\xC3\xBCnchen", false));
  assert(!keepDeparture("SBAHN", "Freising", false));
  assert(!keepDeparture("SBAHN", "Leuchtenbergring", true)); // cancelled
  assert(keepDeparture("SBAHN", "Leuchtenbergring", false));
  assert(keepDeparture("SBAHN", "M\xC3\xBCnchen Ost", false));

  // formatResting
  Departure d1[3] = { dep(now + 7 * 60 + 30, 2), dep(now + 27 * 60, 0), dep(now + 47 * 60, 0) };
  assert(formatResting(d1, 3, now, true) == "S1 ?");    // stale wins
  assert(formatResting(d1, 0, now, false) == "S1 --");  // none
  assert(formatResting(d1, 3, now, false) == "S1.7"); // d1[0] has delayMin 2; even second
  Departure d2[1] = { dep(now + 47 * 60, 0) };
  assert(formatResting(d2, 1, now, false) == "S1 47");
  Departure d3[1] = { dep(now + 99 * 60 + 30, 0) };
  assert(formatResting(d3, 1, now, false) == "S1 99");  // boundary
  Departure d4[1] = { dep(now + 100 * 60, 0) };
  assert(formatResting(d4, 1, now, false) == "S1 ++");  // > 99

  // stale wins even when there is no data at all
  assert(formatResting(d1, 0, now, true) == "S1 ?");

  // formatScrollLine
  assert(formatScrollLine(d1, 3, now) == "S1 Muenchen: 7min +2 | 27min | 47min");
  assert(formatScrollLine(d1, 0, now) == "Keine S1 Abfahrten");
  Departure d5[1] = { dep(now + 12 * 60, 0) };
  assert(formatScrollLine(d5, 1, now) == "S1 Muenchen: 12min");

  // delayMin == 1 boundary: marker must appear at exactly 1
  Departure d6[1] = { dep(now + 5 * 60, 1) };
  assert(formatScrollLine(d6, 1, now) == "S1 Muenchen: 5min +1");

  // inNightWindow: 22:00-05:00 wraps midnight, [start, end)
  assert(!inNightWindow(21, 22, 5));
  assert(inNightWindow(22, 22, 5));
  assert(inNightWindow(23, 22, 5));
  assert(inNightWindow(0, 22, 5));
  assert(inNightWindow(4, 22, 5));
  assert(!inNightWindow(5, 22, 5));
  assert(!inNightWindow(12, 22, 5));
  // non-wrapping window
  assert(inNightWindow(10, 9, 17));
  assert(!inNightWindow(8, 9, 17));
  assert(!inNightWindow(17, 9, 17));
  // equal start/end disables the window entirely
  assert(!inNightWindow(22, 22, 22));

  // noUpcomingTrains: idle when nothing departs within the threshold
  assert(noUpcomingTrains(d1, 0, now, 90));   // empty list
  Departure d7[1] = { dep(now + 91 * 60, 0) };
  assert(noUpcomingTrains(d7, 1, now, 90));   // next train beyond the window
  Departure d8[1] = { dep(now + 90 * 60, 0) };
  assert(!noUpcomingTrains(d8, 1, now, 90));  // exactly at the window edge counts
  assert(!noUpcomingTrains(d1, 3, now, 90));  // trains soon
  assert(noUpcomingTrains(d8, 1, now, 0));    // threshold 0: everything is "too far"

  // formatDisruptionLine: prefixed reason, empty when no disruption
  assert(formatDisruptionLine("Reparatur an einer Weiche") ==
         "Stoerung: Reparatur an einer Weiche");
  assert(formatDisruptionLine("") == "");

  // effectiveEpoch: trust the later of prognosis vs planned+delay
  // (MVG sometimes reports delayInMinutes while realtimeDepartureTime
  // still equals the planned time — observed in the field)
  assert(effectiveEpoch(now + 120, now + 120, 5) == now + 120 + 5 * 60); // stale prognosis -> planned+delay
  assert(effectiveEpoch(now + 420, now + 120, 5) == now + 420);          // consistent -> prognosis
  assert(effectiveEpoch(now + 120, now + 120, 0) == now + 120);          // on time
  assert(effectiveEpoch(now + 60, now + 120, -1) == now + 60);           // early train, prognosis wins

  // formatResting: a 1 Hz blinking dot replaces the space when the next
  // train is delayed — phase derived from the epoch second (now is even)
  Departure dl1[1] = { dep(now + 7 * 60, 5) };
  assert(formatResting(dl1, 1, now, false) == "S1.7");      // even second: dot on
  assert(formatResting(dl1, 1, now - 1, false) == "S1 7");  // odd second: dot off
  Departure dl2[1] = { dep(now + 47 * 60, 1) };
  assert(formatResting(dl2, 1, now, false) == "S1.47");
  Departure dl3[1] = { dep(now + 47 * 60, 0) };
  assert(formatResting(dl3, 1, now, false) == "S1 47");     // on time: no dot,
  assert(formatResting(dl3, 1, now - 1, false) == "S1 47"); // either phase

  // firstRainEpoch: first forecast slot at/above the mm threshold
  {
    time_t ts[5] = { now - 120, now + 780, now + 1680, now + 2580, now + 3480 };
    float dry[5] = { 0.0f, 0.0f, 0.05f, 0.0f, 0.0f };
    float wet[5] = { 0.0f, 0.0f, 0.05f, 0.2f, 0.5f };
    float wetNow[5] = { 0.3f, 0.0f, 0.0f, 0.0f, 0.0f };
    assert(firstRainEpoch(ts, dry, 5, 0.1f) == 0);            // nothing reaches threshold
    assert(firstRainEpoch(ts, wet, 5, 0.1f) == now + 2580);   // 0.05 skipped, 0.2 hits
    assert(firstRainEpoch(ts, wetNow, 5, 0.1f) == now - 120); // already raining slot
    assert(firstRainEpoch(ts, wet, 5, 0.5f) == now + 3480);   // threshold boundary inclusive
  }

  // formatRainLine: minutes under 90, rounded hours from 90, "Regen" while
  // raining (start passed, within the last hour), silent otherwise
  assert(formatRainLine(0, now) == "");
  assert(formatRainLine(now + 45 * 60, now) == "Regen in ~45 min");
  assert(formatRainLine(now + 89 * 60, now) == "Regen in ~89 min");
  assert(formatRainLine(now + 90 * 60, now) == "Regen in ~2 Std");
  assert(formatRainLine(now + 3 * 3600, now) == "Regen in ~3 Std");
  assert(formatRainLine(now - 10 * 60, now) == "Regen");      // raining now
  assert(formatRainLine(now - 61 * 60, now) == "");           // stale start, drop

  // extractBreakingTitle: scan a raw Tagesschau feed prefix (pretty-printed,
  // spaces around colons; title precedes the flag within each item)
  {
    const char *none =
        "{\"news\":[{\"title\" : \"Ruhige Lage\", \"breakingNews\" : false},"
        "{\"title\" : \"Auch ruhig\", \"breakingNews\" : false},";
    const char *breaking =
        "{\"news\":[{\"title\" : \"Alte Meldung\", \"breakingNews\" : false},"
        "{\"sophoraId\":\"x\", \"title\" : \"Grosses Ereignis in M\xC3\xBCnchen\","
        " \"date\" : \"2026-07-19\", \"breakingNews\" : true},";
    const char *flagNoTitle = "{\"news\":[{\"breakingNews\" : true},";
    assert(extractBreakingTitle(none) == "");
    assert(extractBreakingTitle(breaking) == "Grosses Ereignis in M\xC3\xBCnchen");
    assert(extractBreakingTitle(flagNoTitle) == "");
    assert(extractBreakingTitle("partial window without any fla") == "");
  }

  // formatBreakingLine
  assert(formatBreakingLine("Grosses Ereignis") == "EIL: Grosses Ereignis");
  assert(formatBreakingLine("") == "");

  printf("ALL TESTS PASSED\n");
  return 0;
}
