#include <pebble.h>
#include "session.h"
#include "comm.h"
#include "runner.h"

// Pilatable — entry point.
//
// Pull-on-launch (see PLAN.md): run the cached/default session immediately, and
// ask the phone for the latest in the background. A fresh sync is persisted and
// applied on the NEXT launch (we don't change the workout mid-session).

static Session s_session;
static Settings s_settings;

static void prv_session_received(const Session *session, const Settings *settings) {
  // Already persisted by comm; applies next launch.
  APP_LOG(APP_LOG_LEVEL_INFO, "Synced '%s' (%d items) — applies next launch",
          session->name, session->item_count);
}

static void prv_init(void) {
  // 1. Run immediately from cache (or baked-in default) — fully offline.
  session_load_cached(&s_session, &s_settings);

  // 2. Bring up comms and pull the latest session in the background.
  comm_init(prv_session_received);
  comm_request_session();

  // 3. Start the workout runner on the loaded session.
  runner_init(&s_session, &s_settings);
  runner_push();
}

static void prv_deinit(void) {
  runner_deinit();
  comm_deinit();
}

int main(void) {
  prv_init();
  APP_LOG(APP_LOG_LEVEL_DEBUG, "Pilatable initialized");
  app_event_loop();
  prv_deinit();
}
