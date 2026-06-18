# Pilatable

A Pilates mat trainer for the **Pebble Time 2** smartwatch.

- **Watch (`pebble-pilates/`):** a C watchapp (target `emery`, 200×228) that runs a
  workout — a breathing ring, rep/cycle progress, and a rest countdown. Haptics
  carry the breath: **1 buzz = inhale, 2 buzz = exhale**.
- **Phone:** a PebbleKit JS **configuration page** (opened from inside the Pebble
  mobile app) to browse exercises, build a session, set haptic preferences, and
  sync it to the watch. No separate native app required.

See **[PLAN.md](PLAN.md)** for the full architecture and build plan, and
**[pebble-pilates/README.md](pebble-pilates/README.md)** for build/run instructions.

## Releases

Tagging a commit `vMAJOR.MINOR.PATCH` (e.g. `v1.0.0`) triggers CI to build the
watchapp **with the version pinned to the tag** and publish the `.pbw` as a
GitHub Release. See `.github/workflows/release.yml`.

## Note on content licensing

The exercise dataset in the design handoff contains text from a copyrighted book
and is **not** included in this repository. The shipping app uses original copy
plus the factual exercise data only. See PLAN.md → "Content".
