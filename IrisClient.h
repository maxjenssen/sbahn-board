#pragma once
#include "BoardLogic.h"
#include "IrisLogic.h"

// Fallback data source: DB's keyless IRIS timetable API, independent of
// MVG/DEFAS infrastructure. Same contract as MvgClient::fetch.
class IrisClient {
 public:
  bool fetch(Departure out[], int maxOut, int &count, String &disruption);

 private:
  bool fetchXml(const String &url, bool isPlan, time_t now, bool &stoerung);
  String planUrl(const struct tm &lt);

  IrisTrain trains[12];
  int trainCount = 0;
  int planHour = -1;
  int planDay = -1;
  bool primed = false;  // full fchg scanned since last plan refresh
};
