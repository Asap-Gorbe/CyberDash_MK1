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
   A turn-by-turn HUD relayed from a phone's Google Maps notifications
   avoids both: the phone does routing/rerouting (the hard part), the
   ESP32 only ever receives the result of one maneuver.

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
      SerialDisplay.h/.cpp    (today's backend - ANSI terminal)
      (TftDisplay.h/.cpp goes here once hardware is wired - M2-b)
      IInput.h                (abstract input contract)
      SerialInput.h/.cpp      (today's backend - keyboard n/s/b)
      IBleNavSource.h         (abstract nav data source contract)
      SimulatedBleNavSource.h/.cpp  (today's backend - fake moving route)
      (RealBleNavSource.h/.cpp goes here once CarPlayBLE is forked - M6 real-hardware step)
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
- [ ] **M6 - NavigationService + turn-by-turn HUD. SKELETON TESTED,
  REAL HARDWARE PENDING.** Built `hal/IBleNavSource` (same shape as
  `IDisplay`/`IInput`) with a `SimulatedBleNavSource` backend, so
  `NavigationService`/`AppState::nav`/`NavScreen` could be built and
  tested before forking the real Android app. `NavigationService`
  polls its source once per `loop()` tick (no background task needed
  - `poll()` is non-blocking by contract) and only publishes to
  `AppState::nav` when data actually changed. `NavScreen` is no
  longer a stub - shows upcoming turn+distance, or street+ETA when
  no turn is pending.
  Verified against the simulator: moving/cycling route data, no
  flicker, correct state on screen switch-away-and-back,
  Music/Nav independence (separate `navMux`/`musicMux` locks),
  Select handling.
    - [ ] **M6 real-hardware step - pending.** Fork `appleshaman/CarPlayBLE`
      (Android app reads Google Maps navigation notifications via
      NotificationListenerService, forwards over BLE; ESP32 acts as
      GATT server, phone as client). Extract the fork's actual GATT
      service/characteristic UUIDs and payload format from its
      `CarPlay-TTGO` source. Write `RealBleNavSource : public
        IBleNavSource` using `NimBLE-Arduino`, matching that protocol
      exactly. Swap it in for `SimulatedBleNavSource` in `main.cpp` -
      by design, nothing above `IBleNavSource` should need to change.
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
| Renderer approach | **Leaning custom thin layer over TFT_eSPI**, not LVGL | Sparse/whitespace-heavy retro look fights LVGL's default density. Big anti-aliased fonts are the tradeoff - revisit if that becomes painful. |
| Display panel/driver | **Not yet chosen - display not wired up** | Need controller chip (ST7789/ILI9341/GC9A01/etc.) + resolution before M2-b. |
| Secrets handling | **Decided (M2)** | `app/Secrets.h` gitignored, `app/Secrets.example.h` committed as template. |
| Concurrency model | **Decided (M1, formalized M5)** | Networking on core 0, render/UI on core 1. Currently uses raw `volatile`s as the hand-off; M5 replaces this with `AppState`. |
| Render invalidation | **Decided (M3)** | `Screen::update()` returns `bool` (changed this tick?); `ScreenManager` only calls `render()` on enter/switch or when `update()` is true. Render-every-tick was tried first and caused a visible glitch/flicker on `SerialDisplay` at ~150-200Hz. This dirty-flag approach is the one carried forward into the TFT backend, not revisited. |
| Input method | **Still not finalized hardware-side** | Touch confirmed available on the LCD; buttons being considered as an addition. `hal/IInput` abstracts this either way - a `TouchInput` and/or `ButtonInput` backend can be added later with zero changes to `ScreenManager` or any screen. `SerialInput` (keyboard n/s/b) is the backend in use today for testing. |
| Secrets storage | **Revised (M4)** | `Secrets.h` can only ever declare (`extern`), never define, credential values - the moment a second `.cpp` file includes a header that defines a global, the linker sees two definitions. Real values now live in gitignored `Secrets.cpp`; `Secrets.h` (declarations only) is safe to commit. |
| State/event model | **Decided (M4)** | Roadmap called for "AppState + EventBus"; built as a version-counter instead of callback-based pub/sub - `AppState::music.version` increments on any real change, screens cache the version they last saw and redraw only when it differs. Same "services publish, screens observe" decoupling, no dynamic allocation or function-pointer tables. Revisit only if a future consumer genuinely needs push notification instead of a poll-and-compare check. |
| Cross-core state safety | **Decided (M5)** | `AppState`'s public struct replaced with accessor functions (`setMusicTrack`/`setMusicLyricLine`/`getMusic`) wrapping a `portMUX_TYPE` spinlock - chosen over a full mutex/semaphore because critical sections here are a handful of field copies (microseconds), not blocking I/O. `getMusic()` returns a copy so callers never hold the lock while using the data. Same pattern applies to any future service (e.g. `NavigationService` in M6) that publishes state read cross-core. |
| Navigation data source | **Decided (M6)** | Rejected both a dedicated GPS module (no native heading, weak antenna, extra hardware/cost) and raw phone-GPS relay (undermines the standalone-device design goal - phone must stay paired/awake, new BLE/OS-permission failure modes). Settled on forking `appleshaman/CarPlayBLE`: a phone app reads Google Maps' navigation notification (ETA, distance, street, next turn) via NotificationListenerService and relays it over BLE. Phone does routing/rerouting; ESP32 only ever receives the result of one maneuver - smaller, more robust payload than continuous GPS. Known fragility: depends on Google Maps' notification format not changing; proof-of-concept grade, not an official integration. |
| Nav feature scope | **Revised (M6)** | Original spec was a GTA V-style live mini-map. Replaced with a turn-by-turn HUD (arrow/distance/street/ETA) since the chosen data source (see above) provides discrete maneuver events, not a continuous position stream - there's no coordinate data to draw a moving map from. Arguably a better in-car display regardless (big clear instruction vs. a tiny moving dot), but a deliberate, real change from the original goal, not an accidental scope-narrowing. |
| NavigationService concurrency | **Decided (M6)** | Unlike `SpotifyService`, `NavigationService` has no background FreeRTOS task - `IBleNavSource::poll()` is contractually non-blocking (even the real NimBLE backend would buffer internally via its own task and just hand back the latest value), so polling once per `loop()` tick on core 1 is sufficient. Simpler than replicating `SpotifyService`'s core-0 task pattern where it isn't needed. |
## Open questions / TODO before next milestones

- [ ] **Security:** the Spotify client secret + refresh token that were in
  the original `main.cpp` were exposed in plaintext - rotate them in
  the Spotify Developer Dashboard before treating `Secrets.h` as the
  real credential store.
- [ ] Confirm display panel + driver chip (blocks M2-b).
- [ ] Finalize input method: touch confirmed on the LCD, buttons possibly
  added alongside it. Not blocking anymore (`IInput` abstracts it),
  but needs a `TouchInput`/`ButtonInput` backend written once decided.
- [ ] Decide GPS module (blocks M6, not urgent yet).
- [x] ~~Security: rotate Spotify credentials~~ - done; current
  `Secrets.cpp` values have not been committed to git (verified via
  `git check-ignore` before every push since the M2/M3 history reset).
- [ ] Fork `appleshaman/CarPlayBLE`, extract real GATT UUIDs/payload
  format from `CarPlay-TTGO` source (blocks `RealBleNavSource`).
- [ ] Test `RealBleNavSource` against an actual phone once forked and
  wired - simulator testing only proves the architecture, not real
  BLE behavior (connection drops, pairing, payload edge cases).
## Conventions

- New app-specific logic goes in `services/`, never directly in `ui/` or
  `main.cpp`.
- New tunable constants go in `app/Config.h`, not inline magic numbers.
- New colors/sizes/spacing go in `gfx/Theme.h`, not inline in a screen.
- If you're about to `#include` a service header from another service, or a
  screen header from `hal/`, stop - that's the dependency rule breaking.

## Changelog
- **M6 (skeleton tested, real hardware pending):** Added
  `hal/IBleNavSource` + `SimulatedBleNavSource`, extended `AppState`
  with a `nav` block (same accessor+spinlock pattern as M5's `music`
  block, separate lock), added `NavigationService`, `NavScreen` no
  longer a stub. Verified against the simulator only. Nav feature
  spec revised from "GTA V mini-map" to "turn-by-turn HUD" (see
  decisions log) - relies on forking `appleshaman/CarPlayBLE` for real
  data, which is the next step before this milestone can be marked
  fully tested.
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