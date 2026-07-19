#include "NewsClient.h"
#include "Config.h"
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <WiFiClientSecure.h>

bool NewsClient::fetch(String &breaking) {
  WiFiClientSecure client;
  client.setInsecure();
  client.setBufferSizes(16384, 512);
  HTTPClient http;
  http.useHTTP10(true);
  http.setTimeout(10000);
  if (!http.begin(client, NEWS_URL)) {
    Serial.println("news: begin failed");
    return false;
  }
  int code = http.GET();
  if (code != HTTP_CODE_OK) {
    Serial.printf("news: HTTP %d\n", code);
    http.end();
    return false;
  }

  WiFiClient *stream = http.getStreamPtr();
  String window;
  String found;
  uint8_t chunk[257];
  unsigned int total = 0;
  unsigned long lastData = millis();
  while ((http.connected() || stream->available()) && total < NEWS_READ_CAP) {
    size_t avail = stream->available();
    if (avail > 0) {
      size_t want = avail < 256 ? avail : 256;
      int got = stream->readBytes(chunk, want);
      if (got <= 0) break;
      chunk[got] = 0;
      window = window + String((char *)chunk);
      total += got;
      found = extractBreakingTitle(window);
      if (found.length()) break;
      // keep the window bounded; a title+flag span is well under 2 KB
      if (window.length() > 8192) {
        window = window.substring(window.length() - 8192);
      }
      lastData = millis();
    } else {
      if (millis() - lastData > 8000) break;
      delay(2);
    }
  }
  http.end();  // abort mid-body by design — only the head matters

  breaking = transliterate(found);
  if (breaking.length() > NEWS_MAX_LEN) {
    breaking = breaking.substring(0, NEWS_MAX_LEN);
  }
  return true;
}
