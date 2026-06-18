#include "session.h"

// Persistent storage layout. Each persist value is capped at
// PERSIST_DATA_MAX_LENGTH (256 bytes), so a Session (which can exceed that) is
// split: one meta key + one key per item.
typedef enum {
  PKEY_HAS_SESSION = 0,   // bool: is a session cached?
  PKEY_SETTINGS    = 1,   // Settings
  PKEY_SESSION_META = 2,  // {version, name, item_count}
  PKEY_SCHEMA      = 3,   // on-watch storage layout version
  PKEY_ITEM_BASE   = 100, // PKEY_ITEM_BASE + i holds items[i]
} PersistKey;

// Bump whenever the persisted SessionItem/Settings layout changes, so a cache
// written by an older build is ignored (we fall back to the default) instead of
// being misread field-for-field.
#define STORAGE_SCHEMA 2

typedef struct {
  uint8_t version;
  char name[SESSION_NAME_LEN];
  uint8_t item_count;
} SessionMeta;

static const Settings DEFAULT_SETTINGS = {
  .haptics_enabled = true,
  .intensity = INTENSITY_MEDIUM,
  .lead_in_enabled = true,
};

// Baked-in default program (factual Fundamental Mat names — safe to ship).
// Timings are sensible starting defaults; the phone overrides on first sync.
// Fields: name, reps, movement_ds, rest_s, between_ds, mode, pattern, pattern_len
static const SessionItem DEFAULT_ITEMS[] = {
  { "Pelvic Curl",              5,  50, 30, 10, MODE_NORMAL, "EIE", 3 },
  { "Chest Lift",              10,  40, 30, 10, MODE_NORMAL, "EIE", 3 },
  { "Leg Lift Supine",          5,  40, 30, 10, MODE_NORMAL, "EI",  2 },
  { "Spine Twist Supine",       5,  40, 30, 10, MODE_NORMAL, "EI",  2 },
  { "Chest Lift With Rotation", 5,  40, 30, 10, MODE_NORMAL, "EI",  2 },
  { "Back Extension Prone",     5,  50, 30, 10, MODE_NORMAL, "EI",  2 },
  { "One-Leg Circle",           5,  40, 30, 10, MODE_NORMAL, "EI",  2 },
  { "Rolling Back",            10,  40,  0, 10, MODE_NORMAL, "IE",  2 },
};
#define DEFAULT_ITEM_COUNT (sizeof(DEFAULT_ITEMS) / sizeof(DEFAULT_ITEMS[0]))

void session_load_default(Session *out_session) {
  out_session->version = PILATABLE_PROTOCOL_VERSION;
  strncpy(out_session->name, "Fundamental Mat", SESSION_NAME_LEN - 1);
  out_session->name[SESSION_NAME_LEN - 1] = '\0';
  out_session->item_count = DEFAULT_ITEM_COUNT;
  for (uint8_t i = 0; i < DEFAULT_ITEM_COUNT; i++) {
    out_session->items[i] = DEFAULT_ITEMS[i];
  }
}

void session_load_cached(Session *out_session, Settings *out_settings) {
  if (persist_exists(PKEY_SETTINGS)) {
    persist_read_data(PKEY_SETTINGS, out_settings, sizeof(Settings));
  } else {
    *out_settings = DEFAULT_SETTINGS;
  }

  bool has_cache = persist_exists(PKEY_HAS_SESSION) && persist_read_bool(PKEY_HAS_SESSION);
  bool schema_ok = persist_exists(PKEY_SCHEMA) && persist_read_int(PKEY_SCHEMA) == STORAGE_SCHEMA;
  if (!has_cache || !schema_ok) {
    APP_LOG(APP_LOG_LEVEL_INFO, "No usable cache (schema_ok=%d) — loading default program", schema_ok);
    session_load_default(out_session);
    return;
  }

  SessionMeta meta;
  persist_read_data(PKEY_SESSION_META, &meta, sizeof(meta));
  out_session->version = meta.version;
  strncpy(out_session->name, meta.name, SESSION_NAME_LEN);
  out_session->item_count = meta.item_count;
  if (out_session->item_count > MAX_SESSION_ITEMS) {
    out_session->item_count = MAX_SESSION_ITEMS;
  }
  for (uint8_t i = 0; i < out_session->item_count; i++) {
    persist_read_data(PKEY_ITEM_BASE + i, &out_session->items[i], sizeof(SessionItem));
    // Defensive: never leave an item without a valid breath pattern.
    SessionItem *it = &out_session->items[i];
    if (it->pattern_len == 0 || it->pattern_len > MAX_PATTERN_LEN) {
      strncpy(it->pattern, "IE", MAX_PATTERN_LEN - 1);
      it->pattern_len = 2;
    }
  }
  APP_LOG(APP_LOG_LEVEL_INFO, "Loaded cached session '%s' (%d items)",
          out_session->name, out_session->item_count);
}

void session_save(const Session *session, const Settings *settings) {
  persist_write_data(PKEY_SETTINGS, settings, sizeof(Settings));

  SessionMeta meta;
  meta.version = session->version;
  strncpy(meta.name, session->name, SESSION_NAME_LEN);
  meta.item_count = session->item_count;
  persist_write_data(PKEY_SESSION_META, &meta, sizeof(meta));

  for (uint8_t i = 0; i < session->item_count && i < MAX_SESSION_ITEMS; i++) {
    persist_write_data(PKEY_ITEM_BASE + i, &session->items[i], sizeof(SessionItem));
  }
  persist_write_int(PKEY_SCHEMA, STORAGE_SCHEMA);
  persist_write_bool(PKEY_HAS_SESSION, true);
  APP_LOG(APP_LOG_LEVEL_INFO, "Saved session '%s' (%d items)",
          session->name, session->item_count);
}
