#include "IrisClient.h"
#include "Config.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

String IrisClient::planUrl(const struct tm &lt) {
  char path[24];
  snprintf(path, sizeof(path), "%02d%02d%02d/%02d", lt.tm_year % 100,
           lt.tm_mon + 1, lt.tm_mday, lt.tm_hour);
  return String(IRIS_HOST "/plan/" STATION_EVA "/") + path;
}

// Stream one IRIS XML document through the rolling-buffer scanner.
// isPlan: collect city-bound trains into trains[]; otherwise apply
// changes (delays/cancellations/Stoerung) to the cached trains.
bool IrisClient::fetchXml(const String &url, bool isPlan, time_t now,
                          bool &stoerung) {
  WiFiClientSecure client;
  client.setInsecure();
  client.setBufferSizes(16384, 512);
  HTTPClient http;
  http.useHTTP10(true);
  http.setTimeout(10000);
  if (!http.begin(client, url)) {
    Serial.println("iris: begin failed");
    return false;
  }
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("iris: HTTP %d\n", code);
    http.end();
    return false;
  }

  IrisScanner sc;
  String block;
  WiFiClient *stream = http.getStreamPtr();
  uint8_t chunk[257];
  unsigned long lastData = millis();
  while (http.connected() || stream->available()) {
    size_t avail = stream->available();
    if (avail > 0) {
      size_t want = avail < 256 ? avail : 256;
      int got = stream->readBytes(chunk, want);
      if (got <= 0) break;
      chunk[got] = 0;
      sc.feed(String((char *)chunk));
      while (sc.nextBlock(block)) {
        if (isPlan) {
          IrisTrain tr;
          if (trainCount < 12 && parsePlanBlock(block, tr, IRIS_KEEP_PPTH)) {
            trains[trainCount++] = tr;
          }
        } else {
          applyChangeBlock(block, trains, trainCount, now, stoerung);
        }
      }
      lastData = millis();
    } else {
      if (millis() - lastData > 8000) break;  // stalled stream
      delay(2);
    }
  }
  http.end();
  return true;
}

bool IrisClient::fetch(Departure out[], int maxOut, int &count,
                       String &disruption) {
  time_t now = time(nullptr);
  struct tm lt;
  localtime_r(&now, &lt);

  if (lt.tm_hour != planHour || lt.tm_mday != planDay || trainCount == 0) {
    trainCount = 0;
    bool dummy = false;
    if (!fetchXml(planUrl(lt), true, now, dummy)) return false;
    time_t nextHour = now + 3600;
    struct tm nx;
    localtime_r(&nextHour, &nx);
    fetchXml(planUrl(nx), true, now, dummy);  // tolerate failure: 1h coverage
    planHour = lt.tm_hour;
    planDay = lt.tm_mday;
    primed = false;  // plan reset realtimes; re-prime standing delays
  }

  bool stoerung = false;
  if (!primed) Serial.println("iris: priming full change set (fchg, ~200KB)");
  String changesUrl = String(IRIS_HOST) + (primed ? "/rchg/" : "/fchg/") +
                      STATION_EVA;
  if (!fetchXml(changesUrl, false, now, stoerung)) return false;
  primed = true;

  count = emitDepartures(trains, trainCount, now, out, maxOut);
  disruption = stoerung ? String("Betriebsstoerung (DB Meldung)") : String("");
  return true;
}
