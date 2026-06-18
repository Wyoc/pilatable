#include "comm.h"

// Incoming-transfer assembly state.
static CommSessionReceivedHandler s_handler;
static Session s_incoming;
static Settings s_incoming_settings;
static uint8_t s_received_items;
static bool s_in_transfer;

static void prv_request_latest(void) {
  DictionaryIterator *out;
  if (app_message_outbox_begin(&out) != APP_MSG_OK) {
    APP_LOG(APP_LOG_LEVEL_ERROR, "outbox unavailable for session request");
    return;
  }
  dict_write_uint8(out, MESSAGE_KEY_REQUEST_SESSION, 1);
  app_message_outbox_send();
}

static void prv_inbox_received(DictionaryIterator *iter, void *context) {
  // Header message: starts a new transfer.
  Tuple *version = dict_find(iter, MESSAGE_KEY_SESSION_VERSION);
  if (version) {
    s_in_transfer = true;
    s_received_items = 0;
    memset(&s_incoming, 0, sizeof(s_incoming));
    s_incoming.version = version->value->uint8;

    Tuple *name = dict_find(iter, MESSAGE_KEY_SESSION_NAME);
    if (name) {
      strncpy(s_incoming.name, name->value->cstring, SESSION_NAME_LEN - 1);
    }
    Tuple *count = dict_find(iter, MESSAGE_KEY_ITEM_COUNT);
    if (count) {
      s_incoming.item_count = count->value->uint8;
      if (s_incoming.item_count > MAX_SESSION_ITEMS) {
        s_incoming.item_count = MAX_SESSION_ITEMS;
      }
    }

    Tuple *haptics = dict_find(iter, MESSAGE_KEY_SET_HAPTICS);
    if (haptics) s_incoming_settings.haptics_enabled = haptics->value->uint8 != 0;
    Tuple *intensity = dict_find(iter, MESSAGE_KEY_SET_INTENSITY);
    if (intensity) s_incoming_settings.intensity = (Intensity)intensity->value->uint8;
    Tuple *lead_in = dict_find(iter, MESSAGE_KEY_SET_LEADIN);
    if (lead_in) s_incoming_settings.lead_in_enabled = lead_in->value->uint8 != 0;

    APP_LOG(APP_LOG_LEVEL_INFO, "Sync started: '%s', %d items",
            s_incoming.name, s_incoming.item_count);
    return;
  }

  // Item message: one session item, addressed by CHUNK_INDEX.
  Tuple *chunk = dict_find(iter, MESSAGE_KEY_CHUNK_INDEX);
  if (chunk && s_in_transfer) {
    uint8_t i = chunk->value->uint8;
    if (i < MAX_SESSION_ITEMS) {
      SessionItem *item = &s_incoming.items[i];
      memset(item, 0, sizeof(*item));
      Tuple *name = dict_find(iter, MESSAGE_KEY_ITEM_NAME);
      if (name) strncpy(item->name, name->value->cstring, ITEM_NAME_LEN - 1);
      Tuple *reps = dict_find(iter, MESSAGE_KEY_ITEM_REPS);
      if (reps) item->reps = reps->value->uint8;
      Tuple *len = dict_find(iter, MESSAGE_KEY_ITEM_LENGTH_DS);
      if (len) item->movement_length_ds = len->value->uint16;
      Tuple *rest = dict_find(iter, MESSAGE_KEY_ITEM_REST);
      if (rest) item->rest_after_sec = rest->value->uint16;
      Tuple *btw = dict_find(iter, MESSAGE_KEY_ITEM_BETWEEN_DS);
      if (btw) item->between_reps_ds = btw->value->uint16;
      Tuple *mode = dict_find(iter, MESSAGE_KEY_ITEM_MODE);
      if (mode) item->mode = mode->value->uint8;
      Tuple *pat = dict_find(iter, MESSAGE_KEY_ITEM_PATTERN);
      if (pat) {
        strncpy(item->pattern, pat->value->cstring, MAX_PATTERN_LEN - 1);
        item->pattern_len = strlen(item->pattern);
      }
      if (item->pattern_len == 0) {  // back-compat default
        strncpy(item->pattern, "IE", MAX_PATTERN_LEN - 1);
        item->pattern_len = 2;
      }
      s_received_items++;
    }
    return;
  }

  // Trailer message: transfer complete.
  Tuple *done = dict_find(iter, MESSAGE_KEY_SYNC_DONE);
  if (done && s_in_transfer) {
    s_in_transfer = false;
    APP_LOG(APP_LOG_LEVEL_INFO, "Sync done: %d/%d items received",
            s_received_items, s_incoming.item_count);
    session_save(&s_incoming, &s_incoming_settings);
    if (s_handler) {
      s_handler(&s_incoming, &s_incoming_settings);
    }
  }
}

static void prv_inbox_dropped(AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_ERROR, "inbox dropped: %d", (int)reason);
}

static void prv_outbox_failed(DictionaryIterator *iter, AppMessageResult reason, void *context) {
  APP_LOG(APP_LOG_LEVEL_WARNING, "outbox failed: %d (phone likely unreachable)", (int)reason);
}

void comm_init(CommSessionReceivedHandler handler) {
  s_handler = handler;
  s_in_transfer = false;
  app_message_register_inbox_received(prv_inbox_received);
  app_message_register_inbox_dropped(prv_inbox_dropped);
  app_message_register_outbox_failed(prv_outbox_failed);
  // Size buffers to the firmware maximum (see PLAN.md AppMessage risk).
  app_message_open(app_message_inbox_size_maximum(), app_message_outbox_size_maximum());
}

void comm_deinit(void) {
  app_message_deregister_callbacks();
}

void comm_request_session(void) {
  prv_request_latest();
}
