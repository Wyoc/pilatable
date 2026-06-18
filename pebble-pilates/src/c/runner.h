#pragma once
#include <pebble.h>
#include "session.h"

// The workout runner: Active (breath ring) + Rest screens, the breath engine,
// and haptic breath cues. Owns its own Window. See PLAN.md "Watchapp".

// Takes a copy of the session + settings to run. Call before runner_push().
void runner_init(const Session *session, const Settings *settings);
void runner_push(void);
void runner_deinit(void);

// Apply a freshly-synced session while running. Live-updates the plan-review
// screen if the workout hasn't started; ignored mid-workout (it was persisted
// and will be used on the next launch).
void runner_apply_session(const Session *session, const Settings *settings);
