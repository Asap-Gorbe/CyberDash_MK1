# Cyberdeck Firmware — Project Memory

This file is the source of truth for the project's goals, architecture, and
status. Paste it into a new conversation (or keep it open alongside the
repo) so nothing has to be re-explained from scratch. Update it whenever a
milestone finishes or a real decision gets made — stale docs are worse than
no docs, so keep this in sync with reality rather than aspirational.

---

## What this is

A retro-inspired cyberdeck built on an ESP32 (WROOM32), used both mounted in
a car and sitting on a desk. Deliberately **not** a smartphone-style
touchscreen device — minimal, distraction-free, big text, one thing on
screen at a time.

**Core features:**
1. Spotify lyrics, synchronized, one line centered on screen at a time.
2. A minimalist turn-by-turn navigation HUD (upcoming turn + distance, or
   street + ETA when no turn is pending) — separate screen/app from
   music. Originally spec'd as a GTA V-style mini-map; revised during M6
   planning once a live map turned out to require either a dedicated GPS
   module (no native heading, weak antenna, extra hardware) or raw
   position relay from a phone (undermines the standalone-device goal).
   A turn-by-turn HUD relayed from a phone via the Chronos app avoids
   both: the phone does routing/rerouting (the hard part), the ESP32
   only ever receives the result of one maneuver.

## Design goals (non-negotiable — check new code against these)

- Minimalist, retro aesthetic, lots of whitespace.
- Large, readable typography — has to be legible glanced at while driving.
- **One purpose per screen.** Music, Navigation, Home, Settings are
  separate screens, not panels crammed onto one view.
- Spotify, Navigation, and UI are modular and don't know about each other.
- Adding album art, playback controls, caching, GPS, or new screens later
  should not require rearchitecting — only adding new files in the
  appropriate layer.

---

## Architecture

Five layers, dependencies point strictly downward. Lower layers never know
about higher layers. Services never know about each other.

```
app (coordination)      AppState, EventBus, Config, Secrets
   ^
services                SpotifyService, NavigationService - own their domain,
                         never draw, only publish state / accept commands
   ^
ui                       Screen base class, ScreenManager, reusable
                         components (Label, ProgressBar, MapView)
   ^
gfx (rendering)          Renderer (draw primitives), Theme (design tokens)
   ^
hal (hardware)           IDisplay + backends, Input, Gps, Storage, Time
```

Flow: **services -> state -> render** (services publish, screens observe).
**UI -> intent -> service** (screens emit commands, never call service
internals directly). This indirection is *why* Music and Navigation can be
built independently and why swapping a display driver never touches app
logic.

**Rule of thumb for "where does this code go":** if it touches a pin or a
driver -> `hal/`. If it's a generic draw primitive or a design token ->
`gfx/`. If it's screen composition or a reusable widget -> `ui/`. If it's an
external data source (Spotify, GPS) -> `services/`. If it's cross-cutting
state or wiring -> `app/`.

---

## Project structure

```
cyberdeck-firmware/
  platformio.ini
  CLAUDE.md                 <- this file
  .gitignore
  src/
    main.cpp
    app/
      Secrets.h              (gitignored - real credentials)
      Secrets.example.h      (committed template)
      Config.h               (tunable constants: poll interval, sync offset, etc.)
      README.md               -> AppState + EventBus land here in M4
   hal/
      IDisplay.h              (abstract display contract)
      SerialDisplay.h/.cpp    (terminal backend - kept for bench debugging)
      TftDisplay.h/.cpp       (real hardware backend - ILI9488 3.5" SPI panel, M2-b)
      IInput.h                (abstract input contract)
      SerialInput.h/.cpp      (today's backend - keyboard n/s/b)
      IBleNavSource.h         (abstract nav data source contract)
      SimulatedBleNavSource.h/.cpp  (today's backend - fake moving route)
      (RealBleNavSource.h/.cpp goes here once wired to ChronosESP32 - M6 real-hardware step)
    gfx/
      Theme.h                 (palette, type scale, spacing tokens)
      Renderer.h               (draw primitives above IDisplay)
    ui/                       -> Screen, ScreenManager, components - M3
      screens/
      components/
    services/
      spotify/                SpotifyService (M4)
      nav/                    NavigationService (M6, built against
                              SimulatedBleNavSource - real BLE pending)                 -> NavigationService - M6
    net/                      -> WifiManager/HttpClient extracted - M5
    core/                     -> FreeRTOS task wrappers, logging - M5+
```

Every currently-empty folder has a `README.md` one-liner saying which
milestone fills it in - check those before assuming something doesn't exist
yet.

---

## Milestone roadmap
- [ ] **Home screen: clock + now-playing. RE-VERIFIED ON REAL HARDWARE,
  CENTERING STILL IMPERFECT.** `HomeScreen` is no longer a stub - it's
  the device's actual idle screen: NTP-synced time (`hal/Clock`, no
  interface - single real implementation) plus a compact
  now-playing summary read from `AppState::music`.
  Required a real fix to the render pipeline: `SerialDisplay`
  previously cleared the screen on every single `drawText()` call,
  which only worked because no screen had ever needed two lines at
  once before. `Renderer::clear()` was replaced with `beginFrame()`
  (called once per `render()`, not once per line) to support this.
  Found and fixed two real bugs surfaced by this work:
    1. `MusicScreen` fell back to showing artist/track during
       instrumental gaps instead of going blank, because
       `currentLyricLine.length() == 0` was indistinguishable from
       "no synced lyrics exist at all." Added
       `AppState::MusicSnapshot.hasSyncedLyrics` to disambiguate.
    2. `SpotifyService` logged "Nothing playing" on every 2s poll
       (not just once), and worse - `AppState::music` never actually
       got told playback stopped, since that update path only ran
       inside `if (np.valid)`, which a 204 "nothing playing" response
       never sets. Home/Music screens were stuck showing the last
       song indefinitely. Fixed via a `NowPlaying.nothingPlaying`
       flag, acted on only on the actual playing->not-playing
       transition.
       A third issue - both lines requiring a scroll to see - turned out
       to be `SpotifyService`'s background-task log lines (plain
       `Serial.println()`) scrolling the terminal viewport out from
       under `SerialDisplay`'s absolute-positioned rows on every poll.
       Fixed by switching `SerialDisplay` to the ANSI alternate screen
       buffer (`\033[?1049h`), gated behind `AppConfig::UseAltScreenBuffer`
       so it can be flipped off temporarily to read scrollable debug logs
       (e.g. Chronos/lyrics timing output) without losing the tested
       fixed-screen behavior.
       Now tested on the real TFT (see M2-b below) rather than only
       `SerialDisplay` - text draws and wraps correctly, but the vertical
       centering that `TftDisplay::drawText` computes for multi-line text
       isn't quite landing right yet on real glass ("not quite centered
       but it works," per live test). Left as-is for now rather than
       chased further blind - worth a closer pixel-math look next time
       `HomeScreen` specifically is on the table.
- [x] **M2-b - Real display hardware. WIRED AND VERIFIED.** Identified
  the panel as an ILI9488 480x320 3.5" SPI module (exact part
  `HSD035577D3` didn't turn up in any search - identification was by
  matching pin layout against known ILI9488 boards, then confirmed
  for real via a throwaway `TFT_eSPI` color-fill test sketch, not
  guessed and left unverified). Wired point-to-point with jumpers on
  the ESP32 WROOM32's default VSPI pins (CS 15, RESET 4, DC 2, MOSI
  23, SCK 18, MISO 19); backlight wired straight to 3.3V rather than
  a GPIO, since backlight current draw can exceed a GPIO's safe
  rating. `TFT_eSPI` configured entirely via `platformio.ini`
  `build_flags` (not by editing the library's own `User_Setup.h`,
  which would be silently lost since `.pio/` is gitignored). Wrote
  `TftDisplay : public IDisplay`, swapped in for `SerialDisplay` in
  `main.cpp` - nothing above `IDisplay` needed to change, as designed.
  Found and fixed a real bug during this: `TFT_eSPI`'s built-in text
  auto-wrap restarts every overflow line at the left margin, breaking
  centering on any text wider than the screen. Replaced with manual
  two-pass wrapping in `TftDisplay::drawText` - first pass measures
  line count, second pass draws the whole block centered as a unit -
  see decisions log ("TftDisplay text wrapping"). `Theme::Color::Primary`
  changed from green to blue per live preference; `MusicScreen` dropped
  from `Hero` to `Large` text size since full lyric sentences are
  longer than the short labels `Hero` was sized for. `SerialDisplay`
  kept in the tree, unreferenced, for bench debugging without hardware
  attached.

- [ ] **M6 - NavigationService + turn-by-turn HUD. SKELETON TESTED,
  REAL HARDWARE PENDING.** Built `hal/IBleNavSource` (same shape as
  `IDisplay`/`IInput`) with a `SimulatedBleNavSource` backend, so
  `NavigationService`/`AppState::nav`/`NavScreen` could be built and
  tested before the real phone-side integration existed.
  `NavigationService` polls its source once per `loop()` tick (no
  background task needed - `poll()` is non-blocking by contract) and
  only publishes to `AppState::nav` when data actually changed.
  `NavScreen` is no longer a stub - shows whatever nav text is
  currently available.
  Verified against the simulator: moving/cycling route data, no
  flicker, correct state on screen switch-away-and-back,
  Music/Nav independence (separate `navMux`/`musicMux` locks),
  Select handling.
  Nav data source and `NavData`/`NavSnapshot` shape were both revised
  mid-milestone - see decisions log ("Navigation data source" and
  "NavData/NavSnapshot shape"). `pio run` confirmed clean after the
  reshape; the simulator and `NavScreen` build against the new string
  fields.
    - [x] **M6 real-hardware step - WIRED, PENDING LIVE DRIVE TEST.** Added
      `fbiego/ChronosESP32` to `platformio.ini` (pulls in `ESP32Time`
      and `NimBLE-Arduino` automatically via its own `library.json`).
      Wrote `RealBleNavSource : public IBleNavSource` wrapping
      `ChronosESP32::getNavigation()` and pumping `_watch.loop()` once
      per `NavigationService::tick()` (no separate task, same reasoning
      as the concurrency decision above). `valid` is driven off
      `isConnected()`, deliberately *not* `isNavigation`, so
      `NavScreen` doesn't get stuck showing stale directions if nav
      genuinely ends while the phone stays connected - same bug class
      SpotifyService's "nothing playing" transition already caught
      once. Swapped in for `SimulatedBleNavSource` in `main.cpp` -
      nothing above `IBleNavSource` needed to change, as designed.
      Found and fixed one build break along the way: `app/Config.h`'s
      `namespace Config` collided with `ChronosESP32.h`'s own `enum
      Config` - renamed to `AppConfig` throughout (see decisions log).
      `pio run` clean. **Still open:** not yet paired with a phone or
      tested against real BLE traffic - `isConnected()` behaving
      correctly and what `directions`/`title` actually contain on a
      live route are both unverified until that happens.
- [ ] **M5 - Concurrency cleanup.** Formalize networking on core 0 /
  rendering+UI on core 1 with a thread-safe state hand-off (already
  half-true today via the volatile lyric buffer - this milestone makes
  it a proper pattern via `AppState`/`EventBus` instead of raw
  volatiles). Extract `WifiManager`/`HttpClient` into `net/`.
- [x] **M4 - Extract SpotifyService + state/event model. TESTED.** Moved
  all Spotify auth, LRCLIB fetching, JSON parsing, and lyric-sync
  timing out of `main.cpp` into `services/spotify/SpotifyService`.
  Added `app/AppState` (a version-counter-based observable, see
  decisions log) as the boundary between the service and the UI.
  `MusicScreen` is no longer a stub - it reads `AppState::music` and
  redraws only when `.version` changes, reusing the exact mechanism
  built (and bug-fixed) in M3.
  Verified on device: full boot -> token refresh -> now-playing poll
  -> lyrics fetch -> synced display chain working with live playback;
  screen switching away from and back to Music shows correct current
  state immediately (not stale).
    - **Bugs found + fixed during this milestone** (see decisions log for
      the two structural ones):
        1. `Secrets.h` ODR violation once a second `.cpp` included it -
           split into `Secrets.h` (extern declarations, committed) +
           `Secrets.cpp` (real values, gitignored).
        2. Typo in `AppState.cpp` (`Music Music;` instead of `Music music;`)
        - case mismatch caused "undefined reference" at link time despite
          both files compiling individually without error.
        3. Missing initial `refreshAccessToken()` call when the token-refresh
           logic moved into `networkTaskLoop()` - without it, the service ran
           unauthenticated for up to 55 minutes after boot, producing a
           `null`/`null` "track", a doomed lyrics search for it, and a task
           watchdog crash. Fixed by calling `refreshAccessToken()` once,
           unconditionally, before the task's `for(;;)` loop begins.

## Decisions log

| Decision | Status | Notes |
|---|---|---|
| Renderer approach | **Decided** | Custom thin layer over `TFT_eSPI`, not LVGL - confirmed in practice once real hardware arrived, not just a plan. Sparse/whitespace-heavy retro look fights LVGL's default density. `TftDisplay` implements `IDisplay` directly against `TFT_eSPI`'s primitives; no intermediate widget system. |
| Display panel/driver | **Decided (M2-b)** | Panel is a 3.5" 480x320 4-wire SPI module, exact part marked `HSD035577D3` (not found documented anywhere by that code - likely an internal glass part number, not a sellable SKU). Identified as **ILI9488** by pin-layout pattern-matching against known modules, then confirmed for real via a `TFT_eSPI` color-fill test on actual hardware before committing `TftDisplay` to the assumption - not left as an unverified guess. |
| Secrets handling | **Decided (M2)** | `app/Secrets.h` gitignored, `app/Secrets.example.h` committed as template. |
| Concurrency model | **Decided (M1, formalized M5)** | Networking on core 0, render/UI on core 1. Currently uses raw `volatile`s as the hand-off; M5 replaces this with `AppState`. |
| Render invalidation | **Decided (M3)** | `Screen::update()` returns `bool` (changed this tick?); `ScreenManager` only calls `render()` on enter/switch or when `update()` is true. Render-every-tick was tried first and caused a visible glitch/flicker on `SerialDisplay` at ~150-200Hz. This dirty-flag approach is the one carried forward into the TFT backend, not revisited. |
| Input method | **Still not finalized hardware-side** | Touch confirmed available on the LCD; buttons being considered as an addition. `hal/IInput` abstracts this either way - a `TouchInput` and/or `ButtonInput` backend can be added later with zero changes to `ScreenManager` or any screen. `SerialInput` (keyboard n/s/b) is the backend in use today for testing. |
| Secrets storage | **Revised (M4)** | `Secrets.h` can only ever declare (`extern`), never define, credential values - the moment a second `.cpp` file includes a header that defines a global, the linker sees two definitions. Real values now live in gitignored `Secrets.cpp`; `Secrets.h` (declarations only) is safe to commit. |
| State/event model | **Decided (M4)** | Roadmap called for "AppState + EventBus"; built as a version-counter instead of callback-based pub/sub - `AppState::music.version` increments on any real change, screens cache the version they last saw and redraw only when it differs. Same "services publish, screens observe" decoupling, no dynamic allocation or function-pointer tables. Revisit only if a future consumer genuinely needs push notification instead of a poll-and-compare check. |
| Cross-core state safety | **Decided (M5)** | `AppState`'s public struct replaced with accessor functions (`setMusicTrack`/`setMusicLyricLine`/`getMusic`) wrapping a `portMUX_TYPE` spinlock - chosen over a full mutex/semaphore because critical sections here are a handful of field copies (microseconds), not blocking I/O. `getMusic()` returns a copy so callers never hold the lock while using the data. Same pattern applies to any future service (e.g. `NavigationService` in M6) that publishes state read cross-core. |
| Navigation data source | **Revised (M6)** | Rejected both a dedicated GPS module (no native heading, weak antenna, extra hardware/cost) and raw phone-GPS relay (undermines the standalone-device design goal - phone must stay paired/awake, new BLE/OS-permission failure modes). Originally planned to fork `appleshaman/CarPlayBLE` (reverse-engineer GATT UUIDs/payload from a single-dev proof-of-concept). Switched to `fbiego/ChronosESP32`: a maintained, MIT-licensed library + official Chronos app implementing the identical ESP32-as-GATT-server/phone-as-client shape, but with the protocol already built and documented rather than extracted by hand. Same tradeoff as before versus GPS (phone does routing, ESP32 gets the result of one maneuver), lower integration risk than the fork plan. |
| NavData/NavSnapshot shape | **Decided (M6)** | ChronosESP32's `Navigation` struct hands back pre-formatted strings (`eta`, `duration`, `distance`, `title`, `directions`, `speed`), not raw numeric distances - the phone-side app formats them before sending. Reshaped `IBleNavSource::NavData` and `AppState::NavSnapshot` to store those strings directly rather than parsing them back into ints (`distanceRemainingM`/`nextTurnDistanceM` dropped). Same "data-source-first" principle as the mini-map -> HUD scope revision. `title`'s exact semantics are documented as dual-purpose ("distance to next point OR title") and app-dependent - `NavScreen`'s interpretation of it is deliberately left minimal until real BLE traffic from a live drive can confirm what it actually contains. |
| Nav feature scope | **Revised (M6)** | Original spec was a GTA V-style live mini-map. Replaced with a turn-by-turn HUD (instruction/distance/ETA) since the chosen data source (see above) provides discrete maneuver events, not a continuous position stream - there's no coordinate data to draw a moving map from. Arguably a better in-car display regardless (big clear instruction vs. a tiny moving dot), but a deliberate, real change from the original goal, not an accidental scope-narrowing. |
| NavigationService concurrency | **Decided (M6)** | Unlike `SpotifyService`, `NavigationService` has no background FreeRTOS task - `IBleNavSource::poll()` is contractually non-blocking (even the real NimBLE backend would buffer internally via its own task and just hand back the latest value), so polling once per `loop()` tick on core 1 is sufficient. Simpler than replicating `SpotifyService`'s core-0 task pattern where it isn't needed. |
| `Config` naming collision | **Fixed (M6)** | `app/Config.h`'s `namespace Config` collided with `ChronosESP32.h`'s own `enum Config` (its message-type enum for `setConfigurationCallback`) - both at global scope, so any translation unit including both failed to compile with "redeclared as different kind of symbol." Renamed to `AppConfig` throughout (`Config.h`, `Clock.cpp`, `SpotifyService.cpp`). Worth remembering as a class of risk when adding future third-party libraries: check for identifier collisions with `app/`'s intentionally short, generic namespace names. |
| Wall clock | **Decided** | NTP via `configTzTime()`, wrapped in `hal/Clock` - deliberately NOT built as a swappable interface like `IDisplay`/`IInput`/`IBleNavSource`. Only one real implementation is worth having (the system clock); an abstraction here would be complexity with no payoff, same reasoning as M5's spinlock-over-mutex choice. Timezone is a manually-set POSIX TZ string in `AppConfig::Timezone`, not auto-detected. |
| Home screen identity | **Decided** | Home IS the clock/idle face (time as hero element + compact now-playing summary), not a separate menu screen with a clock bolted on. Full live lyrics stay `MusicScreen`'s job - Home only ever shows a one-line summary, keeping "one purpose per screen" intact even though both read `AppState::music`. |
| SerialDisplay + background logging | **Fixed** | Plain `Serial.println()` calls from background tasks (`SpotifyService`) share the same serial stream as `SerialDisplay`'s absolute-positioned ANSI output. Normal terminal scrolling from those log lines was pushing fixed-row content out of the visible viewport. Fixed via the ANSI alternate screen buffer, enabled once in `SerialDisplay::begin()`. This whole class of problem is specific to sharing one stream for both logs and "screen" output - it goes away entirely once a real TFT exists in M2-b (logs to serial, UI to the physical panel, no shared channel). |
| Font direction | **Noted, not yet built** | Dot-matrix/LED-style typeface intended (visually in the spirit of Nothing's Ndot font family, NOT using their proprietary asset - the genre predates them and has open alternatives). Purely a HAL-backend concern per `Theme::TextSize` - irrelevant to `SerialDisplay`, real work starts at M2-b once a pixel display with custom bitmap font support exists. |
| Retro animations | **Noted, not yet built** | Mentioned as a future direction for `HomeScreen`/idle-screen polish. No design work done yet - flagged here purely so it isn't forgotten between now and whenever `ui/components` gets built out. |
| TFT_eSPI configuration | **Decided (M2-b)** | Configured entirely via `platformio.ini` `build_flags` (`-DUSER_SETUP_LOADED`, `-DILI9488_DRIVER`, pin defs, etc.), not by editing `TFT_eSPI`'s own `User_Setup.h` inside the library. `.pio/` is gitignored, so editing library files directly would silently vanish on any clean rebuild - build flags are the only form of this config that's actually committed and reproducible. |
| TftDisplay text wrapping | **Decided (M2-b)** | `TFT_eSPI`'s built-in auto-wrap restarts every overflow line at the left margin, which broke centering the moment a lyric line was wider than the screen. `TftDisplay::drawText` disables that (`setTextWrap(false)`) and does its own two-pass wrap instead: first pass splits text into lines and counts them, second pass draws the whole block centered as a unit around the given `y`. Vertical centering still imperfect on real hardware as of the last test - noted in the M2-b roadmap entry, not yet root-caused further. |
| LRCLIB request latency | **Fixed** | `/api/get` requests were taking 5.6-10.4s consistently, regardless of response size (281 bytes and 9KB took nearly identical time) - traced via staged `millis()` instrumentation to the body-read loop waiting for the TCP socket to fully close (`Connection: close`) rather than stopping once the actual content had arrived. Fixed by parsing the response's `Content-Length` header and exiting the read loop once that many bytes are in hand - cut ~2.7s off every request, confirmed via before/after timing, not assumed. A prior attempt to speed this up by trying `/api/get-cached` first made things *worse* (extra sequential round-trip on every cache miss) and was reverted - real instrumentation beat reasoning from API docs alone. Remaining latency (TLS handshake ~2.6-3.0s, LRCLIB's own response time 2.3-6.5s) is outside firmware control - confirmed lrclib.net does not serve plain HTTP (issues a 301 redirect to HTTPS), so the TLS cost can't be avoided either. |

## Open questions / TODO before next milestones

- [ ] **Security:** the Spotify client secret + refresh token that were in
  the original `main.cpp` were exposed in plaintext - rotate them in
  the Spotify Developer Dashboard before treating `Secrets.h` as the
  real credential store.
- [ ] Finalize input method: touch confirmed on the LCD, buttons possibly
  added alongside it. Not blocking anymore (`IInput` abstracts it),
  but needs a `TouchInput`/`ButtonInput` backend written once decided.
  More pressing now that the TFT is wired and `SerialInput` (keyboard
  n/s/b over USB serial) is the only way to switch screens - fine on a
  desk, not viable once this is actually mounted in the car.
- [x] ~~Decide GPS module~~ - moot; superseded by the Chronos/BLE
  relay decision (see decisions log: "Navigation data source"), no
  GPS module is needed.
- [x] ~~Security: rotate Spotify credentials~~ - done; current
  `Secrets.cpp` values have not been committed to git (verified via
  `git check-ignore` before every push since the M2/M3 history reset).
- [x] ~~Confirm display panel + driver chip~~ - done (M2-b): ILI9488,
  see decisions log.
- [ ] Install the Chronos app (Android, v3.7.5+ for nav support) on a
  test phone and confirm it pairs with the ESP32.
- [ ] Watch real BLE payloads during an actual drive to pin down
  `title`'s real meaning (documented as dual-purpose/app-dependent)
  before writing any branching logic in `NavScreen` around it.
- [ ] Test `RealBleNavSource` against an actual phone once paired -
  simulator testing only proves the architecture, not real BLE
  behavior (connection drops, WiFi/BLE radio contention since both
  now run concurrently, payload edge cases).
- [ ] Revisit `TftDisplay::drawText`'s vertical centering math -
  currently off on real hardware ("not quite centered but it works"),
  not yet root-caused.
- [ ] Strip the temporary `[Nav]` field-dump logging in
  `NavigationService::tick()` and the per-stage timing
  `Serial.printf()`s in `SpotifyService::lrclibGet()`/`getLyrics()`
  once both are no longer actively needed - both were added as
  deliberately temporary diagnostics, not permanent logging.
- [ ] Batch of smaller `NavScreen`/logic bugs mentioned but explicitly
  deferred until after the screen was wired - now that it is, worth
  revisiting.

## Conventions

- New app-specific logic goes in `services/`, never directly in `ui/` or
  `main.cpp`.
- New tunable constants go in `app/Config.h`, not inline magic numbers.
- New colors/sizes/spacing go in `gfx/Theme.h`, not inline in a screen.
- If you're about to `#include` a service header from another service, or a
  screen header from `hal/`, stop - that's the dependency rule breaking.

## Changelog
- **M2-b (wired and verified):** Identified the panel as ILI9488
  (480x320, 3.5" SPI, part marked `HSD035577D3`) by pin-layout
  matching, confirmed for real via a `TFT_eSPI` color-fill test before
  committing to it. Wired point-to-point on the WROOM32's default
  VSPI pins; backlight tied straight to 3.3V rather than a GPIO.
  `TFT_eSPI` configured via `platformio.ini` build_flags, not the
  library's own `User_Setup.h` (would be lost - `.pio/` is
  gitignored). Wrote `TftDisplay : public IDisplay`, swapped in for
  `SerialDisplay` in `main.cpp`. Fixed a real bug found on real
  hardware: `TFT_eSPI`'s built-in auto-wrap broke text centering on
  any line wider than the screen - replaced with a manual two-pass
  wrap-then-center-as-a-block approach in `TftDisplay::drawText`.
  `Theme::Color::Primary` changed green -> blue; `MusicScreen` text
  size dropped `Hero` -> `Large` for full lyric sentences. Vertical
  centering still imperfect, tracked as an open TODO rather than
  chased blind. `SerialDisplay` kept in the tree for bench debugging.
- **Lyrics latency fix:** Diagnosed `/api/get` requests consistently
  taking 5.6-10.4s via staged `millis()` instrumentation added
  specifically to find the real bottleneck rather than guess at one.
  Root cause: the body-read loop waited for the TCP socket to fully
  close instead of stopping once the expected content had arrived.
  Fixed by parsing the `Content-Length` header and exiting once that
  many bytes are read - confirmed ~2.7s saved per request via
  before/after timing. A prior attempt (trying `/api/get-cached`
  first) made things worse and was reverted before the real cause was
  found. Confirmed separately that lrclib.net doesn't serve plain
  HTTP (301 redirect to HTTPS), so the remaining TLS-handshake cost
  can't be avoided.
- **Terminal scroll toggle:** Added `AppConfig::UseAltScreenBuffer` so
  the alternate-screen-buffer lock (which intentionally disables
  terminal scrollback for `SerialDisplay`'s fixed-row output) can be
  flipped off temporarily to read scrollable debug logs, without
  permanently changing the already-tested default behavior.
- **M6 real-hardware step (wired, pending live drive test):** Added
  `fbiego/ChronosESP32` to `platformio.ini`. Wrote
  `RealBleNavSource : public IBleNavSource`, swapped in for
  `SimulatedBleNavSource` in `main.cpp`. Fixed a build-breaking
  `Config` namespace/enum collision between `app/Config.h` and
  `ChronosESP32.h` by renaming to `AppConfig`. `pio run` clean.
  Not yet paired with a phone or tested against real BLE traffic.
- **M6 (nav data source revised, pre-real-hardware):** Switched planned
  nav BLE source from a CarPlayBLE fork to `fbiego/ChronosESP32` (see
  decisions log). Reshaped `IBleNavSource::NavData` and
  `AppState::NavSnapshot` from numeric fields to the string fields
  ChronosESP32 actually provides; updated `NavigationService`,
  `SimulatedBleNavSource`, and `NavScreen` accordingly and confirmed
  `pio run` clean. `NavScreen`'s new field-reading logic is
  intentionally minimal, pending real BLE traffic. `RealBleNavSource`
  itself not yet written.
- **Home screen (in progress):** `hal/Clock` added (NTP, manual
  timezone). Multi-line render support added (`beginFrame()` replacing
  per-call clearing). `HomeScreen` no longer a stub. Fixed instrumental
  gaps incorrectly showing artist/track instead of going blank (new
  `hasSyncedLyrics` field), fixed "Nothing playing" log spam and
  `AppState::music` never learning playback stopped, fixed terminal
  scrolling desyncing fixed-row layout (alternate screen buffer). Not
  yet fully re-verified on device; centering revert pinned until real
  LCD hardware.
- **M6 (skeleton tested, real hardware pending):** Added
  `hal/IBleNavSource` + `SimulatedBleNavSource`, extended `AppState`
  with a `nav` block (same accessor+spinlock pattern as M5's `music`
  block, separate lock), added `NavigationService`, `NavScreen` no
  longer a stub. Verified against the simulator only. Nav feature
  spec revised from "GTA V mini-map" to "turn-by-turn HUD" (see
  decisions log).
- **M5 (tested):** Made `AppState::music` thread-safe - public struct
  replaced with `setMusicTrack()`/`setMusicLyricLine()`/`getMusic()`
  accessors protected by a spinlock. `SpotifyService` and `MusicScreen`
  updated accordingly. Verified via functional regression, a 10-15min
  soak test, and deliberate rapid-switching stress during track
  transitions. `net/` extraction deferred to M6.
- **M4 (tested):** Extracted `services/spotify/SpotifyService` from
  `main.cpp` (auth, LRCLIB fetch, sync timing - logic unchanged, just
  relocated and encapsulated). Added `app/AppState` as the
  service-to-UI boundary. `MusicScreen` now shows real synced lyrics.
  Fixed a `Secrets.h` ODR violation (split into `.h`/`.cpp`), a
  case-typo linker error in `AppState.cpp`, and a missing initial token
  refresh that caused ~55 minutes of unauthenticated operation and a
  watchdog crash on first boot after the extraction.
- **M3 (tested):** Added `hal/IInput` + `SerialInput` (keyboard n/s/b ->
  Next/Select/Back), `ui/Screen` (abstract base), `ui/ScreenManager`, and
  three screens (`HomeScreen`, stub `MusicScreen`, stub `NavScreen`).
  `main.cpp`'s `loop()` now delegates to `screenManager.tick()`; the
  lyric-sync tracking math stays running in the background for M4 to pick
  up. Found and fixed a render-every-tick bug (see decisions log: Render
  invalidation) during testing - `Screen::update()` now gates redraws.
- **M2 (tested):** Added `hal/` (`IDisplay`, `SerialDisplay`), `gfx/`
  (`Renderer`, `Theme`), `app/Config.h`, `app/Secrets.h` (+ example).
  `main.cpp` now renders through `Renderer`/`Theme` instead of raw ANSI
  codes; Spotify logic otherwise untouched. Established folder skeleton
  for M3-M6 with placeholder READMEs. Verified on device: builds clean,
  boots correctly, lyrics fetch/sync/display identically to pre-M2,
  terminal resize still adapts, no heap drift over an extended run.
- **M1:** Initial working lyrics-in-serial-terminal proof of concept.