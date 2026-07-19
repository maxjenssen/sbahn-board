#include "WeatherClient.h"
#include "Config.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>
#include <ArduinoJson.h>

#define WEATHER_URL                                                    \
  "https://api.open-meteo.com/v1/forecast?latitude=" WEATHER_LAT       \
  "&longitude=" WEATHER_LON                                            \
  "&minutely_15=precipitation&forecast_minutely_15=48&timeformat=unixtime"

bool WeatherClient::fetch(time_t &rainEpoch) {
  WiFiClientSecure client;
  client.setInsecure();
  client.setBufferSizes(16384, 512);
  HTTPClient http;
  http.useHTTP10(true);
  http.setTimeout(10000);
  if (!http.begin(client, WEATHER_URL)) {
    Serial.println("weather: begin failed");
    return false;
  }
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("weather: HTTP %d\n", code);
    http.end();
    return false;
  }

  JsonDocument filter;
  filter["minutely_15"]["time"] = true;
  filter["minutely_15"]["precipitation"] = true;

  JsonDocument doc;
  DeserializationError err =
      deserializeJson(doc, http.getStream(), DeserializationOption::Filter(filter));
  http.end();
  if (err) {
    Serial.printf("weather: parse %s\n", err.c_str());
    return false;
  }

  JsonArray tArr = doc["minutely_15"]["time"];
  JsonArray pArr = doc["minutely_15"]["precipitation"];
  int n = 0;
  time_t times[48];
  float precip[48];
  for (int i = 0; i < (int)tArr.size() && i < (int)pArr.size() && n < 48; i++) {
    times[n] = (time_t)tArr[i].as<double>();
    precip[n] = pArr[i].as<float>();
    n++;
  }
  if (n == 0) {
    Serial.println("weather: empty forecast");
    return false;
  }

  rainEpoch = firstRainEpoch(times, precip, n, RAIN_MM_THRESHOLD);
  return true;
}
