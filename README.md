# sbahn-board

A desk-sized live departure board: an ESP8266 and a 32×8 LED matrix showing a
countdown to the next S-Bahn from your station — delays already folded in,
cancelled trains skipped, dimmed at night, self-healing on WiFi and API
hiccups. Built for the Munich S1 (default station: Eching) on the MVG/MVV
network; adapting it to any MVG station is a two-line change.

<!-- photo: add a picture of the running board here -->

The resting view is just `S1 7` — minutes until the next city-bound train,
ticking down every second. Every 30 s it scrolls the detail line:
`S1 Muenchen: 7min +2 | 27min | 47min`.

## Features

- Real-time departures from the (unofficial) MVG API — no API key, no account
- Countdown includes live delays; a `+N` marker shows when a train is late
- Night dimming on a local-time schedule (DST handled via NTP + POSIX TZ)
- WiFi onboarding via captive portal (WiFiManager) — no credentials in code
- Degrades honestly: stale data shows `S1 ?`, never silently wrong
- Watchdog restart if the data path is dead for 15 minutes
- Pure display/filter logic is host-testable on your computer, no hardware needed

## Hardware

| Part | Notes |
|---|---|
| Wemos D1 mini (ESP8266) | any clone works |
| MAX7219 dot-matrix module, 4-in-1 (FC16, 32×8) | |
| USB power supply | the board runs anywhere with WiFi |

### Wiring

| Matrix | Wemos D1 mini |
|---|---|
| CLK | D5 (SCK) |
| DIN | D7 (MOSI) |
| CS  | D6 |
| VCC | 5V |
| GND | GND |

## Data source (check here first when the board stops working)

```bash
curl 'https://www.mvg.de/api/bgw-pt/v3/departures?globalId=de:09178:2650&limit=10&transportTypes=SBAHN'
```

Expected: a JSON array; each entry has at least

```json
{
  "plannedDepartureTime": 1784106300000,
  "realtime": true,
  "delayInMinutes": 6,
  "realtimeDepartureTime": 1784106660000,
  "transportType": "SBAHN",
  "label": "S1",
  "destination": "Flughafen München",
  "cancelled": false
}
```

The firmware keeps SBAHN entries whose `destination` contains neither
"Flughafen" nor "Freising" (on the S1, city-bound trains terminate at
München Ost / Leuchtenbergring / Ostbahnhof). If this curl stops
returning that shape, the API changed — an earlier incarnation of this
project died of exactly that (bahnhof.de API, now a 404), which is why this
contract lives at the top of the README.

## Adapting to your station

1. **Find your station's `globalId`:**

   ```bash
   curl 'https://www.mvg.de/api/bgw-pt/v3/locations?query=YOUR_STATION_NAME'
   ```

   Take the `globalId` of the STATION entry (e.g. `de:09178:2650`) and set
   `STATION_ID` in `Config.h` — or, to keep your home station out of your
   public fork, create a gitignored `ConfigLocal.h`:

   ```cpp
   #pragma once
   #undef STATION_ID
   #define STATION_ID "de:xxxxx:xxxx"
   ```

   `Config.h` includes it automatically when present; any `#define` from
   `Config.h` can be overridden the same way.

2. **Direction filter:** edit `EXCLUDE_DEST[]` in `BoardLogic.h` to the
   destination substrings you want to hide (the terminus names of the
   direction you *don't* care about). Empty the list to show both directions.

3. **Labels:** the `"S1 "` resting prefix and `"S1 Muenchen: "` scroll prefix
   are hardcoded in `formatResting`/`formatScrollLine` in `BoardLogic.h` —
   this is deliberately a small, readable firmware you adapt by editing, not
   a config framework. The host tests pin the display strings; update them
   together.

4. **Night window / brightness / intervals:** all in `Config.h`
   (`NIGHT_START_HOUR` 22, `NIGHT_END_HOUR` 5, `NIGHT_BRIGHTNESS` 0 — set the
   hours equal to disable). Timezone is `TZ_Europe_Berlin` in the `.ino`;
   swap the TZ constant if you're adapting beyond Germany.

Works for any station on Munich's MVG/MVV network. Other cities need a
different API client — `MvgClient.cpp` is ~70 lines and is the only file
that knows about MVG.

## Flashing

Arduino IDE with the [ESP8266 core](https://arduino.esp8266.com/stable/package_esp8266com_index.json)
(3.x) installed. Board: **LOLIN(WEMOS) D1 R2 & mini**, Flash Size
**4MB (FS:1MB)**. Libraries (Library Manager unless noted):

- WiFiManager (tzapu)
- ArduinoJson (v7)
- Adafruit GFX Library (+ Adafruit BusIO)
- [Max72xxPanel](https://github.com/markruys/arduino-Max72xxPanel) (GitHub)

Open `sbahn-board.ino`, Upload. Serial monitor at 115200. On first boot the
board opens a WiFi network `SBahnBoard` — join it with your phone and enter
your WiFi credentials in the portal.

CLI alternative:

```bash
arduino-cli compile --fqbn esp8266:esp8266:d1_mini --upload -p <your-port> .
```

## Display states

| Shows | Meaning |
|---|---|
| `S1 7` | Next city-bound S1 leaves in 7 min (delay included) |
| `S1 ++` | Next one is > 99 min away |
| `S1 --` | No city-bound S1 in the data (normal late at night) |
| `S1 ?` | Data older than 3 min — fetches failing |
| `sync` | Waiting for NTP time after boot |
| `WiFi?` | No WiFi — join AP `SBahnBoard` and configure |

Every 30 s the board scrolls details: `S1 Muenchen: 7min +2 | 27min | 47min`.
No fetch success for 15 min → automatic restart.

## Tuning

All knobs are in `Config.h`. Display mirrored or garbled? Set
`DISPLAY_TEST 1`, reflash, adjust `LED_ROTATION` (0-3) and `PANEL_REVERSED`
until `S1 12` reads correctly, set back to 0.

Fetch log lines include the board's local time (`local HH:MM`) as a
timezone sanity check.

## Host tests (pure logic)

```bash
g++ -std=c++17 -DHOST_TEST -I. -o test/test_boardlogic.bin test/test_boardlogic.cpp && ./test/test_boardlogic.bin
```

Covers the direction filter, countdown math, umlaut transliteration, all
display-string states and their boundaries, and the night-window logic —
via a 30-line Arduino-String shim, no hardware or emulator required.

## Troubleshooting

- Persistent `fetch FAILED` in serial: the preceding `mvg:` line names the
  failing stage — `mvg: HTTP <negative>` is the TLS/connect layer (first
  suspect the reduced `setBufferSizes(4096, 512)` in `MvgClient.cpp`; raise
  the rx buffer or probe MFLN), `mvg: HTTP 4xx/5xx` is the API itself,
  `mvg: parse …`/`mvg: non-array response` means the response shape changed
  (re-check the curl contract above). A shrinking `maxblock` value on the
  failure line means heap fragmentation. Occasional isolated failures are
  normal; the display keeps counting down from the last good data.
- First `ESP.restart()` after a USB flash can hang (ESP8266 bootloader
  quirk) — press the physical reset button once; subsequent restarts work.
- IRAM is at ~92%: substantial new features (OTA especially) may not fit
  without freeing instruction RAM first.

## Credits

- Scroll/center rendering adapted from
  [marquee-scroller](https://github.com/Qrome/marquee-scroller)
  (MIT, © 2018 David Payne) — this project began life as a fork of it.
- Departure data from the unofficial MVG API. Not affiliated with or
  endorsed by MVG; the firmware polls once per minute — keep it gentle.
- Built with [Claude Code](https://claude.com/claude-code).

## License

[MIT](LICENSE)
