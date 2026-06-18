# Pilatable — Build Plan

A two-part Pilates mat trainer for the **Pebble Time 2** (`emery`):
a glanceable watchapp that runs a workout (breath ring + haptic breath cues), and a
phone-side **configuration page** to browse exercises, build a session, and sync it to the watch.

Goal: a **public, shippable** product.

Source design handoff lives in `extracted/design_handoff_pebble_pilates/`
(HTML mockups, design tokens, and `exercises.json` for 46 classic mat exercises).

---

## 1. Architecture

| Layer | Choice |
|---|---|
| **Watchapp** | C + Pebble SDK, target `emery` (200×228, 64-color). Budget ≤128 KB app / ≤256 KB resources. |
| **Phone** | **PebbleKit JS configuration page** — hosted HTML/JS opened from inside the official Core Devices Pebble app. Cross-platform (iOS+Android) for free. |
| **Transport** | AppMessage (versioned, chunk-ready). |

No native Android app, no Play Store. Only the `.pbw` ships (to the Rebble dev portal → `apps.rePebble.com`); the config page is hosted as a static site.

**Rationale:** the handoff mockups are already HTML, so the config page is a near-1:1 port; the watch only needs name + timings per item, so the AppMessage payload is tiny and the watch bundles no dataset.

---

## 2. Data model & sync

### Payload (phone → watch)
```
Session {
  version: int,                 // protocol version (for v2 phase tracks)
  name: string,                 // e.g. "Fundamental Mat"
  items: SessionItem[],         // ordered
}
SessionItem { name, reps, movementLengthSec, restAfterSec }
Settings { hapticsEnabled, intensity, leadInEnabled }
```
Items are **fully resolved** on the phone — the watch bundles **no** exercise dataset, needs no JSON parser, and never carries the copyrighted prose.

### Sync model — pull-on-launch
1. Config page edits the session and persists it to `localStorage`.
2. On watchapp launch, the watch asks the phone JS for the latest session.
3. JS reads from `localStorage` and sends it over AppMessage (chunk if it exceeds one message; query `app_message_*_size_maximum()` at runtime).
4. Watch caches the received session + settings in **persistent storage**.

### Offline behavior
- Phone unreachable → run the **cached** session. The phone is only needed to *change* the program, never to work out.
- First-ever run with empty cache and no phone → run a **baked-in default "Fundamental Mat"** session.

---

## 3. Watchapp

- **Target/screen:** `emery`, 200×228, 64-color, e-paper. Flat fills, high contrast, discrete state changes — no per-frame tweens.
- **Input: buttons only** (PT2 has touch, but we don't use it). Up `«` = prev, Select = pause/resume, Down `»` = skip, Back = exit/confirm-quit. Keep the on-screen hint rail.
- **Screens:**
  - **Active:** title bar (name + `08/20`), breath ring (Variation A: progress arc + scaling inner disc + phase label + center count), CYCLES row + progress bar, button rail.
  - **Rest:** dark title bar, ring shows countdown, NEXT UP + next exercise name, resume/skip rail.
- **Breath engine (v1, generic):** each rep = `inhale` (movementLengthSec) → `exhale` (movementLengthSec). On phase start: fire haptic (**inhale = 1 pulse, exhale = 2 pulses** via `VibePattern`), update label + ring direction. reps = number of cycles. Ring disc animates expand/contract in coarse e-paper-friendly steps (a few redraws/sec).
  - Built **extensible**: engine reads a phase list defaulting to `[inhale, exhale]`; a per-exercise phase track can be sent in **v2** without rework.
  - **Swimming** = `continuous` flag → uninterrupted gentle alternation.
- **Settings honored:** `hapticsEnabled` (gates all vibes), `intensity` (vibe strength), `leadInEnabled` (3·2·1 count-in before each set).
- **Fonts:** bundle a subsetted **Fredoka** `.ttf` (characterRegex limited to needed glyphs) at ~34 / 25 / 15 px for the hero numerals/phase/next-up; use system **Gothic** for 9–11 px labels.
- **Persistence/runtime state:** `currentItemIndex`, `currentRep`, `phase ∈ {inhale, exhale, rest}`, `phaseRemainingSec`, `isPaused`; last session + settings cached.

---

## 4. Phone configuration page

Port the four mockup screens to the exact design tokens; Fredoka + Nunito as webfonts.

1. **Library** — 46 exercises grouped by chapter; search; level filter chips; add to session.
2. **Configure** — per-exercise reps stepper, movement-length slider, rest stepper; breathing sequence + cues (from rewritten copy); **placeholder** pose image (category-tinted, v1).
3. **Breathing & Buzz** — master haptics toggle, the inhale=1/exhale=2 diagram, intensity slider, 3·2·1 lead-in toggle.
4. **Session Ready** — ordered program review with rest dividers; "saved — open Pilates on your watch".

Persists session + settings to `localStorage`; serves them to the watch on pull.

---

## 5. Content (parallel, ship-blocking)

- **Rewrite ALL prose** (`breath` actions, `cues`, `topCues`, `note`) in **original, instructor-reviewed** copy — the verbatim *Pilates Anatomy* text cannot ship.
- **Keep the factual layer:** names, chapters, levels, reps, inhale/exhale ordering + buzz counts.
- Fix the mid-sentence PDF-extraction truncations in `breathShort` / `topCues` during the rewrite.
- Produce a clean shippable `exercises.json` (original prose) consumed by the config page.

---

## 6. Assets

- **Pose illustrations:** polished category-tinted placeholder for v1; commission 46 originals (must be original) post-launch.
- **Fonts:** Fredoka + Nunito webfonts on phone; subsetted Fredoka resource on watch.

---

## 7. Build order

1. **Watch runner vs hardcoded session** — validate ring redraw cadence, 1-vs-2 buzz feel, readability, button ergonomics on the **real PT2**.
2. **Sync** — versioned AppMessage schema, pull-on-launch, persistent-storage cache, offline + default session.
3. **Config-page UI** — port the four screens, wire `localStorage`.
4. **Content rewrite** (parallel workstream).
5. **Fidelity polish** — tokens, Fredoka subset, placeholder art, settings.
6. **Publish** — `.pbw` to Rebble dev portal; host the config page.

---

## 8. Open risks

- **Content licensing** — rewrite is the long pole; gates public launch.
- **Appstore governance** — Nov 2025 Rebble↔Core Devices dispute; verify the publishing endpoint before ship.
- **Config-page hosting** — needs a static host; page needs connectivity to open (watchapp runs fully offline once cached).
- **AppMessage max buffer** — firmware-dependent; query at runtime, chunk if needed.

---

## Design reference

- Mockups: `extracted/design_handoff_pebble_pilates/Pebble Pilates.dc.html` + `screens/*.png`
- Data: `extracted/design_handoff_pebble_pilates/exercises.json`
- Full spec: `extracted/design_handoff_pebble_pilates/README.md`
- Design tokens (colors/typography/spacing/radius) — see the handoff README "Design Tokens".
