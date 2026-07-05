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
2. A minimalist navigation screen with a GTA V–style mini-map (separate
   screen/app from music — not layered on top of it).

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
    gfx/
      Theme.h                 (palette, type scale, spacing tokens)
      Renderer.h               (draw primitives above IDisplay)
    ui/                       -> Screen, ScreenManager, components - M3
      screens/
      components/
    services/
      spotify/                -> SpotifyService - M4
      nav/                    -> NavigationService - M6
    net/                      -> WifiManager/HttpClient extracted - M5
    core/                     -> FreeRTOS task wrappers, logging - M5+
```

Every currently-empty folder has a `README.md` one-liner saying which
milestone fills it in - check those before assuming something doesn't exist
yet.

---

## Milestone roadmap

- [x] **M1 - Lyrics proof of concept.** ESP32 connects to Spotify, fetches
  synced lyrics from LRCLIB, displays one centered line via raw ANSI
  codes in the serial terminal. Sync is good enough for normal
  listening. *(This was the starting point - the original monolithic
  main.cpp.)*

- [x] **M2 - Display bring-up + theme (software half). TESTED.** Built the
  HAL (`IDisplay` interface), the `SerialDisplay` backend, `Renderer`,
  and `Theme` tokens. Ported the existing lyric display through this
  seam instead of raw ANSI calls. Spotify/lyrics logic itself is
  *unchanged* - still monolithic in `main.cpp`, extraction into
  `SpotifyService` is M4, not M2. Secrets moved out of `main.cpp` into
  a gitignored `app/Secrets.h`; tunables consolidated into
  `app/Config.h`.
  Verified on device: clean build, correct boot sequence, WiFi + token
  refresh unaffected, lyrics fetch/parse/sync/display identical to
  pre-M2 behavior, terminal resize still adapts centering (confirms
  `renderer.poll()` is wired to the throttle, not just firing once at
  boot), no heap drift over an extended run.
    - [ ] **M2-b - Real glass.** Once the display is physically wired: pick
      driver library (TFT_eSPI most likely), confirm panel resolution +
      controller chip, write `TftDisplay : public IDisplay`, swap it in
      for `SerialDisplay` in `main.cpp`, and validate the type scale is
      actually readable at a glance in the car. **Nothing above the HAL
      layer should need to change for this step** - if it does, that's a
      sign the `IDisplay` contract was drawn in the wrong place.

- [x] **M3 - Screen framework + ScreenManager. TESTED.** Built `hal/IInput`
  (abstract input contract, same shape as `IDisplay`) with a
  `SerialInput` backend (keyboard: n/s/b -> Next/Select/Back) so
  switching logic could be built and tested before touch/button
  hardware is finalized. Built `ui/Screen` (abstract base class) and
  `ui/ScreenManager`, which owns the active screen and routes input to
  it. Three screens exist: `HomeScreen` (index 0, always in place after
  Back), and stub `MusicScreen`/`NavScreen` that prove switching works
  but don't yet show real lyrics/map (M4/M6). `main.cpp`'s `loop()` now
  calls `screenManager.tick()` instead of drawing directly; the
  existing lyric-sync tracking math keeps running in the background
  (updating `currentLine`) even though nothing draws it yet — parked
  for M4 to pick up via the state model, not deleted.
  Verified on device: boot enters Home; n/n/n cycles Home->Music->Nav->
  Home; b returns to Home from anywhere; s on Music reaches that
  screen's handleInput() specifically; lyric fetch/parse logs keep
  appearing normally in the background with nothing drawn (expected).
    - **Bug found + fixed during testing:** initial version had
      `ScreenManager::tick()` call `render()` on every loop iteration
      (~150-200Hz), which on `SerialDisplay` meant a full ANSI clear+redraw
      that many times a second - visibly glitchy, and log lines (e.g. the
      Select-pressed message) would render then immediately get stomped by
      the next redraw. Fixed by making `Screen::update()` return `bool`
      (changed since last tick?) and having `ScreenManager` only call
      `render()` on screen-enter/switch or when `update()` returns true.
      This is the correct long-term shape, not a stopgap - it's what will
      make M4's real `MusicScreen` redraw only when the lyric line actually
      advances, not on every tick regardless of change.

- [ ] **M4 - Extract SpotifyService + state/event model.** Pull the
  Spotify/lyrics logic out of `main.cpp` into `services/spotify/`
  behind a clean interface. Introduce `AppState` + `EventBus`. Music
  screen observes state instead of calling Spotify code directly.
  This is where the service/UI boundary gets proven on the one feature
  already understood deeply - Navigation becomes a template-fill after
  this.

- [ ] **M5 - Concurrency cleanup.** Formalize networking on core 0 /
  rendering+UI on core 1 with a thread-safe state hand-off (already
  half-true today via the volatile lyric buffer - this milestone makes
  it a proper pattern via `AppState`/`EventBus` instead of raw
  volatiles). Extract `WifiManager`/`HttpClient` into `net/`.

- [ ] **M6 - NavigationService + mini-map screen.** GPS behind the HAL,
  `NavigationService`, GTA-V-style map rendering. Real test of the
  whole architecture: if M2-M5 were done right, this should not touch
  Music at all.

- [ ] **M7+ - Leaf features.** Album art (image decode + PSRAM + caching),
  playback controls (UI -> intent -> service path already exists by
  this point), lyric/track caching (LittleFS), Settings screen,
  additional screens. Each should land in exactly one place.

---

## Decisions log

| Decision | Status | Notes |
|---|---|---|
| Renderer approach | **Leaning custom thin layer over TFT_eSPI**, not LVGL | Sparse/whitespace-heavy retro look fights LVGL's default density. Big anti-aliased fonts are the tradeoff - revisit if that becomes painful. |
| Display panel/driver | **Not yet chosen - display not wired up** | Need controller chip (ST7789/ILI9341/GC9A01/etc.) + resolution before M2-b. |
| Secrets handling | **Decided (M2)** | `app/Secrets.h` gitignored, `app/Secrets.example.h` committed as template. |
| Concurrency model | **Decided (M1, formalized M5)** | Networking on core 0, render/UI on core 1. Currently uses raw `volatile`s as the hand-off; M5 replaces this with `AppState`. |
| Render invalidation | **Decided (M3)** | `Screen::update()` returns `bool` (changed this tick?); `ScreenManager` only calls `render()` on enter/switch or when `update()` is true. Render-every-tick was tried first and caused a visible glitch/flicker on `SerialDisplay` at ~150-200Hz. This dirty-flag approach is the one carried forward into the TFT backend, not revisited. |
| Input method | **Still not finalized hardware-side** | Touch confirmed available on the LCD; buttons being considered as an addition. `hal/IInput` abstracts this either way - a `TouchInput` and/or `ButtonInput` backend can be added later with zero changes to `ScreenManager` or any screen. `SerialInput` (keyboard n/s/b) is the backend in use today for testing. |

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

## Conventions

- New app-specific logic goes in `services/`, never directly in `ui/` or
  `main.cpp`.
- New tunable constants go in `app/Config.h`, not inline magic numbers.
- New colors/sizes/spacing go in `gfx/Theme.h`, not inline in a screen.
- If you're about to `#include` a service header from another service, or a
  screen header from `hal/`, stop - that's the dependency rule breaking.

## Changelog

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