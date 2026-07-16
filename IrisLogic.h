#pragma once
// Pure, host-testable logic for DB's keyless IRIS timetable API
// (iris.noncd.db.de) — the fallback data source when the MVG API is down.
// XML is scanned with targeted string ops, never fully parsed: IRIS blocks
// are flat and regular, and the ESP8266 streams them through a small
// rolling buffer.
#include "BoardLogic.h"
#include <time.h>

struct IrisTrain {
  String id;             // join key between plan and rchg/fchg
  String label;          // "S1"
  String destination;    // transliterated last stop of the departure path
  time_t plannedEpoch;
  time_t realtimeEpoch;  // == planned until a change says otherwise
  bool cancelled;
};

// Value of attribute `name` inside one tag string, or "" if absent.
// Matches " name=\"" with a leading space so `pt` never matches inside `ppth`.
inline String xmlAttr(const String &tag, const char *name) {
  String needle = String(" ") + name + "=\"";
  int i = tag.indexOf(needle.c_str());
  if (i < 0) return String("");
  unsigned int start = i + needle.length();
  int end = tag.indexOf("\"", start);
  if (end < 0) return String("");
  return tag.substring(start, end);
}

// IRIS timestamps are local (Europe/Berlin) "YYMMDDHHmm". Requires the TZ
// to be configured (configTime on device, setenv in host tests).
// Returns 0 on malformed input.
inline time_t parseIrisTime(const String &s) {
  if (s.length() != 10) return 0;
  int v[5];
  for (int f = 0; f < 5; f++) {
    char a = s[f * 2], b = s[f * 2 + 1];
    if (a < '0' || a > '9' || b < '0' || b > '9') return 0;
    v[f] = (a - '0') * 10 + (b - '0');
  }
  struct tm tmv = {};
  tmv.tm_year = 100 + v[0];  // "26" -> 2026
  tmv.tm_mon = v[1] - 1;
  tmv.tm_mday = v[2];
  tmv.tm_hour = v[3];
  tmv.tm_min = v[4];
  tmv.tm_isdst = -1;  // let mktime resolve CET/CEST
  return mktime(&tmv);
}

inline String lastPathStop(const String &ppth) {
  int i = ppth.indexOf("|");
  if (i < 0) return ppth;
  int last = i;
  while ((i = ppth.indexOf("|", last + 1)) >= 0) last = i;
  return ppth.substring(last + 1);
}

// Rolling-buffer extractor for complete <s ...>...</s> blocks. feed() takes
// arbitrarily split chunks (TLS stream reads); nextBlock() yields blocks
// FIFO. Memory stays bounded to roughly one block.
class IrisScanner {
 public:
  void feed(const String &chunk) { buf = buf + chunk; }

  bool nextBlock(String &block) {
    int start = buf.indexOf("<s ");
    if (start < 0) {
      // keep a small tail in case "<s " itself was split across chunks
      if (buf.length() > 3) buf = buf.substring(buf.length() - 3);
      return false;
    }
    if (start > 0) buf = buf.substring(start);
    int end = buf.indexOf("</s>");
    if (end < 0) return false;
    block = buf.substring(0, end + 4);
    buf = buf.substring(end + 4);
    return true;
  }

 private:
  String buf;
};

// Parse one plan block into an IrisTrain. Returns false when the block has
// no departure element, a malformed time, or its departure path does not
// contain keepPpthContains (wrong direction). Only <dp> is consulted —
// <ar> carries the arrival path, which points the opposite way.
inline bool parsePlanBlock(const String &block, IrisTrain &out,
                           const char *keepPpthContains) {
  int dp = block.indexOf("<dp ");
  if (dp < 0) return false;
  int dpEnd = block.indexOf("/>", dp);
  if (dpEnd < 0) return false;
  String tag = block.substring(dp, dpEnd + 2);

  String ppth = xmlAttr(tag, "ppth");
  if (ppth.indexOf(keepPpthContains) < 0) return false;

  time_t pt = parseIrisTime(xmlAttr(tag, "pt"));
  if (pt == 0) return false;

  int sEnd = block.indexOf(">");
  String sTag = block.substring(0, sEnd + 1);
  out.id = xmlAttr(sTag, "id");
  String l = xmlAttr(tag, "l");
  out.label = l.length() ? l : String("S");
  out.destination = transliterate(lastPathStop(ppth));
  out.plannedEpoch = pt;
  out.realtimeEpoch = pt;
  out.cancelled = false;
  return true;
}

// Apply one rchg/fchg block: changed time (ct) and cancellation (cs="c")
// for a matching train, and detect any Stoerung message whose validity
// window contains `now` — checked on every block, matched or not, since
// line-level incidents are attached across stops.
inline void applyChangeBlock(const String &block, IrisTrain *trains, int count,
                             time_t now, bool &stoerungActive) {
  int pos = 0;
  while ((pos = block.indexOf("<m ", pos)) >= 0) {
    int end = block.indexOf("/>", pos);
    if (end < 0) break;
    String m = block.substring(pos, end + 2);
    pos = end + 2;
    if (!(xmlAttr(m, "cat") == "St\xC3\xB6rung")) continue;
    time_t from = parseIrisTime(xmlAttr(m, "from"));
    time_t to = parseIrisTime(xmlAttr(m, "to"));
    if ((from == 0 || from <= now) && (to == 0 || now <= to)) {
      stoerungActive = true;
    }
  }

  int sEnd = block.indexOf(">");
  String id = xmlAttr(block.substring(0, sEnd + 1), "id");
  for (int i = 0; i < count; i++) {
    if (!(trains[i].id == id)) continue;
    int dp = block.indexOf("<dp ");
    if (dp < 0) return;
    int dpEnd = block.indexOf("/>", dp);
    if (dpEnd < 0) return;
    String tag = block.substring(dp, dpEnd + 2);
    time_t ct = parseIrisTime(xmlAttr(tag, "ct"));
    if (ct != 0) trains[i].realtimeEpoch = ct;
    if (xmlAttr(tag, "cs") == "c") trains[i].cancelled = true;
    return;
  }
}

// Drop cancelled and departed trains, sort soonest-first, fill Departure[].
inline int emitDepartures(const IrisTrain *trains, int count, time_t now,
                          Departure *out, int maxOut) {
  int idx[16];
  int n = 0;
  for (int i = 0; i < count && n < 16; i++) {
    if (trains[i].cancelled) continue;
    if (trains[i].realtimeEpoch < now) continue;
    idx[n++] = i;
  }
  for (int i = 1; i < n; i++) {
    for (int j = i;
         j > 0 && trains[idx[j]].realtimeEpoch < trains[idx[j - 1]].realtimeEpoch;
         j--) {
      int tmp = idx[j];
      idx[j] = idx[j - 1];
      idx[j - 1] = tmp;
    }
  }
  int emitted = n < maxOut ? n : maxOut;
  for (int i = 0; i < emitted; i++) {
    const IrisTrain &t = trains[idx[i]];
    out[i].label = t.label;
    out[i].destination = t.destination;
    out[i].realtimeEpoch = t.realtimeEpoch;
    out[i].delayMin = (int)((t.realtimeEpoch - t.plannedEpoch) / 60);
  }
  return emitted;
}
