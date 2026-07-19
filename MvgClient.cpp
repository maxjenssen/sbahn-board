#include "MvgClient.h"
#include "Config.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

bool MvgClient::fetch(Departure out[], int maxOut, int &count, String &disruption) {
  WiFiClientSecure client;
  client.setInsecure();              // deliberate: see spec "Network & time"
  // rx must hold one full TLS record: mvg.de's ~4.6 KB cert chain arrives as
  // a single >4 KB record from some edge nodes (observed live: HTTP -5 with
  // a 4 KB buffer). 16 KB is the protocol-safe maximum; heap allows it.
  client.setBufferSizes(16384, 512);
  HTTPClient http;
  http.useHTTP10(true);             // no chunked encoding -> stream-parse safe
  http.setTimeout(10000);
  if (!http.begin(client, MVG_URL)) {
    Serial.println("mvg: begin failed");
    return false;
  }
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("mvg: HTTP %d\n", code);  // negative = TLS/connect layer
    http.end();
    return false;
  }

  JsonDocument filter;
  filter[0]["label"] = true;
  filter[0]["transportType"] = true;
  filter[0]["destination"] = true;
  filter[0]["realtimeDepartureTime"] = true;
  filter[0]["plannedDepartureTime"] = true;
  filter[0]["delayInMinutes"] = true;
  filter[0]["cancelled"] = true;
  filter[0]["infos"][0]["message"] = true;
  filter[0]["infos"][0]["type"] = true;

  JsonDocument doc;
  DeserializationError err =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    Serial.printf("mvg: parse %s\n", err.c_str());
    return false;
  }
  if (!doc.is<JsonArray>()) {  // 200 + error object must not read as "no departures"
    Serial.println("mvg: non-array response");
    return false;
  }

  Departure parsed[MAX_DEPARTURES];
  int n = 0;
  String incidentMsg, otherMsg;  // reason from kept departures; INCIDENT wins
  for (JsonObject d : doc.as<JsonArray>()) {
    if (n >= maxOut || n >= MAX_DEPARTURES) break;  // parsed[] is MAX_DEPARTURES-sized
    String tt = d["transportType"] | "";
    String dest = d["destination"] | "";
    bool cancelled = d["cancelled"] | false;
    if (!keepDeparture(tt, dest, cancelled)) continue;
    parsed[n].label = String(d["label"] | "S1");
    parsed[n].destination = transliterate(dest);
    time_t rt = (time_t)((d["realtimeDepartureTime"] | 0.0) / 1000.0);
    time_t pt = (time_t)((d["plannedDepartureTime"] | 0.0) / 1000.0);
    parsed[n].delayMin = d["delayInMinutes"] | 0;
    parsed[n].realtimeEpoch =
        pt > 0 ? effectiveEpoch(rt, pt, parsed[n].delayMin) : rt;
    n++;
    for (JsonObject info : d["infos"].as<JsonArray>()) {
      String msg = info["message"] | "";
      if (msg.length() == 0) continue;
      String type = info["type"] | "";
      if (type == "INCIDENT") {
        if (incidentMsg.length() == 0) incidentMsg = msg;
      } else if (otherMsg.length() == 0) {
        otherMsg = msg;
      }
    }
  }

  // API order is by planned time; a big delay can reorder reality.
  // Insertion sort by realtime epoch (n <= 3).
  for (int i = 1; i < n; i++) {
    for (int j = i; j > 0 && parsed[j].realtimeEpoch < parsed[j - 1].realtimeEpoch; j--) {
      Departure tmp = parsed[j];
      parsed[j] = parsed[j - 1];
      parsed[j - 1] = tmp;
    }
  }

  for (int i = 0; i < n; i++) out[i] = parsed[i];
  count = n;
  disruption = transliterate(incidentMsg.length() ? incidentMsg : otherMsg);
  if (disruption.length() > DISRUPTION_MAX_LEN) {
    disruption = disruption.substring(0, DISRUPTION_MAX_LEN);
  }
  return true;
}
