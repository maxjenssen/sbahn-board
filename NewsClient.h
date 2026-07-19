#pragma once
#include "BoardLogic.h"

// Tagesschau Eilmeldung detector (keyless). The full feed is ~470 KB, but
// breaking items sort first — so only the head of the stream is read
// (NEWS_READ_CAP bytes) through a rolling window, then the connection is
// dropped.
class NewsClient {
 public:
  // Sets breaking to the transliterated, capped Eilmeldung headline
  // ("" when none in the feed head). Returns false on HTTP/TLS failure
  // with breaking untouched.
  bool fetch(String &breaking);
};
