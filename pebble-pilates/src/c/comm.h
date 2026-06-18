#pragma once
#include <pebble.h>
#include "session.h"

// Phone<->watch sync over AppMessage. Pull-on-launch model: the watch requests
// the latest session from the phone JS, which streams it back as a header
// message + one message per item + SYNC_DONE. See PLAN.md "Sync model".

// Called once a full session has been received and persisted.
typedef void (*CommSessionReceivedHandler)(const Session *session, const Settings *settings);

void comm_init(CommSessionReceivedHandler handler);
void comm_deinit(void);

// Ask the phone for the latest session (no-op if phone unreachable; the caller
// should already be running the cached/default session in that case).
void comm_request_session(void);
