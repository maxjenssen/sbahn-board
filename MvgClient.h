#pragma once
#include "BoardLogic.h"

class MvgClient {
public:
  // Fills out[] with up to maxOut filtered city-bound departures,
  // soonest first, and sets disruption to the transliterated reason from
  // the kept departures' infos (INCIDENT preferred, empty when none).
  // Returns false on HTTP/TLS/parse failure (outputs untouched).
  bool fetch(Departure out[], int maxOut, int &count, String &disruption);
};
