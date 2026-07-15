#pragma once
#include "BoardLogic.h"

class MvgClient {
public:
  // Fills out[] with up to maxOut filtered city-bound departures,
  // soonest first. Returns false on HTTP/TLS/parse failure (out untouched).
  bool fetch(Departure out[], int maxOut, int &count);
};
