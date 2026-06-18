# Pilatable — Pebble Time 2 watchapp + config page

Watchapp (C, target `emery`) for the Pilates mat trainer, plus the PebbleKit JS
bridge and a hosted configuration page.

## Layout

```
src/c/
  pilatable.c   entry point + (placeholder) runner window
  session.h/.c  session model + persistent-storage cache + baked-in default program
  comm.h/.c     AppMessage sync (pull-on-launch): request + assemble + persist
src/pkjs/
  index.js      PebbleKit JS: opens the config page, serves the session to the watch
config/
  index.html    configuration page (scaffold stub; full UI is build-order step 3)
resources/fonts/ Fredoka .ttf goes here (see README there)
```

## Build / run

```bash
pebble build
pebble install --emulator emery     # emulator
pebble install --phone <phone-ip>   # real PT2 via the Pebble app dev connection
pebble logs --phone <phone-ip>      # APP_LOG output
```

## Data flow (pull-on-launch)

1. Watch launches → runs the cached session immediately (or the baked-in
   `Fundamental Mat` default on a fresh install) — works fully offline.
2. Watch sends `REQUEST_SESSION`; the phone JS replies with a header message,
   one message per item, then `SYNC_DONE`.
3. Watch persists the received session and refreshes.

The config page (opened from the gear in the Pebble app) edits the session and
returns it to the JS, which persists it to `localStorage` and pushes it to the watch.

## Status

- **Data pipeline** (cache + default fallback + pull-on-launch sync): wired, builds for emery.
- **Active/Rest runner** (`runner.c`): built and verified in the emery emulator —
  breath state machine (lead-in → inhale → exhale → rest → done), expanding ring +
  scaling breath disc, 1-vs-2 buzz `VibePattern`s gated by settings, button nav
  (prev / pause-resume / skip / back), cycles bar, rest countdown with NEXT UP.
- **Plan-review screen**: on launch the watch shows the session (name, move count,
  ~duration, scrollable list); the workout starts only when the user presses Select
  (▶). Up/Down scroll the list, Back exits. No timer runs until the user begins.

- **Fonts:** ✅ Fredoka (Medium, OFL) bundled subsetted at 38px (count/timer) and
  18px (phase label, cycles, next-up name); small UI labels use system Gothic.
- **Colors:** `runner.c` uses a deliberate 64-color-palette mapping (the design's
  pastels collapse under emery's 4-levels-per-channel); validated in the emulator.
- **Haptics:** 1-vs-2 buzz `VibePattern`s tuned per intensity with a tight gap so
  the double reads as two taps.

### Final tuning (needs the real PT2 panel/motor)
- Confirm color choices and the 1-vs-2 buzz *feel* on hardware; values are good
  baselines but the physical panel/motor may warrant nudges.
- Breath-ring redraw cadence (~5 Hz) vs e-paper readability.
