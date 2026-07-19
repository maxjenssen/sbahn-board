#pragma once
#include "BoardLogic.h"

// Open-Meteo 15-minutely precipitation forecast (keyless), 12 h horizon.
class WeatherClient {
 public:
  // Sets rainEpoch to the first forecast slot at/above RAIN_MM_THRESHOLD
  // (0 when dry for the whole horizon). Returns false on HTTP/TLS/parse
  // failure with rainEpoch untouched.
  bool fetch(time_t &rainEpoch);
};
