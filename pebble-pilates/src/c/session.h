#pragma once
#include <pebble.h>

// Pilatable session model + persistence.
// The watch bundles NO exercise dataset — items arrive fully resolved from the
// phone (name + timings). See PLAN.md "Data model & sync".

#define PILATABLE_PROTOCOL_VERSION 1
#define MAX_SESSION_ITEMS 40
#define ITEM_NAME_LEN 32
#define SESSION_NAME_LEN 32

typedef struct {
  char name[ITEM_NAME_LEN];
  uint8_t reps;
  uint16_t movement_length_ds;  // deciseconds (0.1s units), e.g. 50 = 5.0s per phase
  uint16_t rest_after_sec;
} SessionItem;

typedef struct {
  uint8_t version;
  char name[SESSION_NAME_LEN];
  uint8_t item_count;
  SessionItem items[MAX_SESSION_ITEMS];
} Session;

typedef enum {
  INTENSITY_GENTLE = 0,
  INTENSITY_MEDIUM = 1,
  INTENSITY_STRONG = 2,
} Intensity;

typedef struct {
  bool haptics_enabled;
  Intensity intensity;
  bool lead_in_enabled;  // 3-2-1 count-in before each set
} Settings;

// Load the cached session + settings from persistent storage; if none is
// cached, fills `out_session` with the baked-in default "Fundamental Mat".
void session_load_cached(Session *out_session, Settings *out_settings);

// Persist a session + settings (the result of a phone sync).
void session_save(const Session *session, const Settings *settings);

// Fill `out_session` with the baked-in default program (first-ever-run fallback).
void session_load_default(Session *out_session);
