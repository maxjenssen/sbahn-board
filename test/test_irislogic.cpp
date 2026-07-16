#include <cassert>
#include <cstdio>
#include <cstdlib>
#include "../IrisLogic.h"

// Fixtures distilled from live IRIS responses for Eching (eva 8001647),
// captured 2026-07-16 during an actual Störung. Ids shortened.
static const char *PLAN_XML =
    "<?xml version='1.0' encoding='UTF-8'?><timetable station='Eching'>"
    // city-bound: <dp> path towards Muenchen -> KEEP. <ar> path must be ignored.
    "<s id=\"111-2607161000-4\"><tl f=\"S\" c=\"S\" n=\"6947\"/>"
    "<ar pt=\"2607161007\" pp=\"2\" l=\"S1\" ppth=\"Freising|Pulling(b Freising)|Neufahrn(b Freising)\"/>"
    "<dp pt=\"2607161007\" pp=\"2\" l=\"S1\" ppth=\"Lohhof|Unterschlei\xC3\x9Fheim|M\xC3\xBCnchen-Feldmoching|M\xC3\xBCnchen Hbf\"/></s>"
    // Freising-bound: <dp> path without Muenchen -> DROP
    "<s id=\"222-2607161011-8\"><tl f=\"S\" c=\"S\" n=\"6942\"/>"
    "<dp pt=\"2607161022\" pp=\"1\" l=\"S1\" ppth=\"Neufahrn(b Freising)|Pulling(b Freising)|Freising\"/></s>"
    // city-bound, gets delayed via rchg below
    "<s id=\"333-2607161020-4\"><tl f=\"S\" c=\"S\" n=\"6949\"/>"
    "<dp pt=\"2607161027\" pp=\"2\" l=\"S1\" ppth=\"Lohhof|M\xC3\xBCnchen-Feldmoching|M\xC3\xBCnchen Ost\"/></s>"
    // terminating train: no <dp> at all -> DROP
    "<s id=\"444-2607161030-9\"><ar pt=\"2607161031\" l=\"S1\" ppth=\"M\xC3\xBCnchen Ost\"/></s>"
    "</timetable>";

static const char *RCHG_XML =
    "<timetable station=\"Eching\" eva=\"8001647\">"
    // delay for 333: 1027 -> 1035 (+8), plus an ACTIVE Stoerung message
    "<s id=\"333-2607161020-4\" eva=\"8001647\">"
    "<m id=\"r1\" t=\"h\" from=\"2607160900\" to=\"2607161130\" cat=\"St\xC3\xB6rung\" pr=\"2\"/>"
    "<dp ct=\"2607161035\" l=\"S1\"/></s>"
    // cancellation for 111 (synthetic; cs=\"c\" per IRIS schema)
    "<s id=\"111-2607161000-4\" eva=\"8001647\"><dp cs=\"c\" l=\"S1\"/></s>"
    // unrelated train -> must be ignored by the merge
    "<s id=\"999-2607161040-2\" eva=\"8001647\"><dp ct=\"2607161045\"/></s>"
    "</timetable>";

static const char *EXPIRED_STOERUNG_BLOCK =
    "<s id=\"999-2607161040-2\" eva=\"8001647\">"
    "<m id=\"r2\" t=\"h\" from=\"2607160800\" to=\"2607160900\" cat=\"St\xC3\xB6rung\" pr=\"2\"/>"
    "<dp ct=\"2607161045\"/></s>";

int main() {
  // Deterministic Berlin local time for parseIrisTime/mktime
  setenv("TZ", "CET-1CEST,M3.5.0,M10.5.0/3", 1);
  tzset();

  // xmlAttr
  assert(xmlAttr("<dp pt=\"2607161007\" l=\"S1\"/>", "pt") == "2607161007");
  assert(xmlAttr("<dp pt=\"2607161007\" l=\"S1\"/>", "l") == "S1");
  assert(xmlAttr("<dp pt=\"2607161007\"/>", "ct") == "");
  assert(xmlAttr("<dp ppth=\"A|B\" pt=\"1\"/>", "pt") == "1"); // no substring trap via ppth's 'pt'

  // parseIrisTime: round-trip through localtime in Berlin TZ
  time_t t = parseIrisTime("2607161007");
  assert(t > 1000000000);
  struct tm lt;
  localtime_r(&t, &lt);
  assert(lt.tm_year == 126 && lt.tm_mon == 6 && lt.tm_mday == 16);
  assert(lt.tm_hour == 10 && lt.tm_min == 7);
  assert(parseIrisTime("2607161035") - parseIrisTime("2607161027") == 8 * 60);
  assert(parseIrisTime("") == 0);
  assert(parseIrisTime("26071610") == 0); // too short

  // lastPathStop
  assert(lastPathStop("Lohhof|M\xC3\xBCnchen-Feldmoching|M\xC3\xBCnchen Ost") == "M\xC3\xBCnchen Ost");
  assert(lastPathStop("Freising") == "Freising");
  assert(lastPathStop("") == "");

  // Scanner: reassembles <s>...</s> blocks across arbitrary chunk splits
  IrisScanner sc;
  String planXml(PLAN_XML);
  for (unsigned int i = 0; i < planXml.length(); i += 7) {
    unsigned int end = i + 7 < planXml.length() ? i + 7 : planXml.length();
    sc.feed(planXml.substring(i, end));
  }
  String block;
  int nBlocks = 0;
  IrisTrain trains[8];
  int nTrains = 0;
  while (sc.nextBlock(block)) {
    nBlocks++;
    IrisTrain tr;
    if (parsePlanBlock(block, tr, "M\xC3\xBCnchen") && nTrains < 8) {
      trains[nTrains++] = tr;
    }
  }
  assert(nBlocks == 4);
  assert(nTrains == 2); // 111 and 333 kept; 222 wrong direction; 444 no dp
  assert(trains[0].id == "111-2607161000-4");
  assert(trains[0].label == "S1");
  assert(trains[0].destination == "Muenchen Hbf"); // transliterated
  assert(trains[0].realtimeEpoch == trains[0].plannedEpoch);
  assert(!trains[0].cancelled);
  assert(trains[1].id == "333-2607161020-4");

  // Merge rchg: delay on 333, cancel 111, ignore 999, Stoerung active at 10:30
  const time_t now = parseIrisTime("2607161030");
  bool stoerung = false;
  IrisScanner sc2;
  sc2.feed(String(RCHG_XML));
  while (sc2.nextBlock(block)) {
    applyChangeBlock(block, trains, nTrains, now, stoerung);
  }
  assert(stoerung);
  assert(trains[0].cancelled);
  assert(trains[1].realtimeEpoch - trains[1].plannedEpoch == 8 * 60);

  // Expired Stoerung window must NOT flag
  bool stoerung2 = false;
  applyChangeBlock(String(EXPIRED_STOERUNG_BLOCK), trains, nTrains, now, stoerung2);
  assert(!stoerung2);

  // Emit: drops cancelled + past, sorts by realtime, fills Departure
  Departure deps[3];
  int nDeps = emitDepartures(trains, nTrains, now, deps, 3);
  assert(nDeps == 1); // 111 cancelled; only delayed 333 remains
  assert(deps[0].label == "S1");
  assert(deps[0].destination == "Muenchen Ost");
  assert(deps[0].delayMin == 8);
  assert(minutesUntil(deps[0].realtimeEpoch, now) == 5); // 10:35 vs 10:30

  // Past departures are dropped
  IrisTrain past;
  past.id = "555";
  past.label = "S1";
  past.destination = "Muenchen Ost";
  past.plannedEpoch = now - 120;
  past.realtimeEpoch = now - 120;
  past.cancelled = false;
  IrisTrain solo[1] = {past};
  assert(emitDepartures(solo, 1, now, deps, 3) == 0);

  printf("ALL IRIS TESTS PASSED\n");
  return 0;
}
