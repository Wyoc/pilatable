#include "runner.h"

// ----- Colors -----
// emery has only 64 colors (4 levels/channel), so the design's pastel tokens are
// snapped to the nearest palette entries that stay legible on e-paper. Exact
// brand matching is a final tuning pass on the real PT2 (+ the real Fredoka font).
#define C_PURPLE   GColorFromRGB(85, 0, 170)     // primary (design #7A3DA8)
#define C_BG       GColorFromRGB(255, 255, 255)  // design cream #EFE9E1 -> white
#define C_INK      GColorFromRGB(0, 0, 0)         // design #2A251F
#define C_RING_TRK GColorFromRGB(170, 170, 170)  // visible track (design #DCD4C8)
#define C_DISC     GColorFromRGB(255, 170, 255)  // legible light purple (design #ECDDF6)
#define C_BAR_TRK  GColorFromRGB(170, 170, 170)
#define C_RAIL     GColorFromRGB(170, 170, 170)
#define C_MUTED    GColorFromRGB(85, 85, 85)

// ----- Layout (200 x 228 emery) -----
#define SCREEN_W 200
#define SCREEN_H 228
#define TITLE_H 24
#define RAIL_X 176                 // rail divider; main column is 0..176
#define RAIL_CX 188                // rail glyph center x
#define RING_D 108
#define RING_CX 88                 // center of main column
#define RING_CY 96
#define RING_INSET 9
#define DISC_MAX 42
#define DISC_MIN 24                // ~0.58 of inner radius

#define TICK_MS 200                // ~5 Hz: "redraw a few times per second"
#define LEAD_IN_MS 3000

typedef enum {
  PHASE_LEAD_IN,
  PHASE_INHALE,
  PHASE_EXHALE,
  PHASE_REST,
  PHASE_DONE,
} Phase;

static Session s_session;
static Settings s_settings;

static Window *s_window;
static Layer *s_canvas;
static AppTimer *s_timer;

static uint8_t s_item;        // current exercise index
static uint8_t s_rep;         // current cycle, 1-based
static Phase s_phase;
static uint32_t s_elapsed_ms; // within the current phase
static int s_last_second;     // for per-second lead-in pulses
static bool s_paused;

// ---------------------------------------------------------------------------
// Helpers

static const SessionItem *cur_item(void) { return &s_session.items[s_item]; }

static uint8_t cur_reps(void) {
  uint8_t r = cur_item()->reps;
  return r == 0 ? 1 : r;
}

static uint32_t phase_duration_ms(void) {
  switch (s_phase) {
    case PHASE_LEAD_IN: return LEAD_IN_MS;
    case PHASE_INHALE:
    case PHASE_EXHALE: {
      uint32_t ms = (uint32_t)cur_item()->movement_length_ds * 100;
      return ms < 1000 ? 4000 : ms;  // sane fallback if unset
    }
    case PHASE_REST: return (uint32_t)cur_item()->rest_after_sec * 1000;
    case PHASE_DONE: return 0;
  }
  return 0;
}

static void buzz(int pulses) {
  if (!s_settings.haptics_enabled) return;
  uint32_t on = s_settings.intensity == INTENSITY_GENTLE ? 80
              : s_settings.intensity == INTENSITY_STRONG ? 220 : 140;
  uint32_t seg[5];
  int n = 0;
  for (int i = 0; i < pulses && n < 5; i++) {
    seg[n++] = on;
    if (i < pulses - 1 && n < 5) seg[n++] = 120;  // gap between pulses
  }
  VibePattern p = { .durations = seg, .num_segments = n };
  vibes_enqueue_custom_pattern(p);
}

static void to_uppercase(const char *src, char *dst, size_t n) {
  size_t i = 0;
  for (; src[i] && i < n - 1; i++) {
    char c = src[i];
    dst[i] = (c >= 'a' && c <= 'z') ? (c - 32) : c;
  }
  dst[i] = '\0';
}

// ---------------------------------------------------------------------------
// State machine

static void enter_phase(Phase p) {
  s_phase = p;
  s_elapsed_ms = 0;
  s_last_second = -1;
  switch (p) {
    case PHASE_INHALE: buzz(1); break;  // 1 buzz = inhale
    case PHASE_EXHALE: buzz(2); break;  // 2 buzz = exhale
    case PHASE_DONE:   vibes_long_pulse(); break;
    default: break;
  }
}

static void start_item(uint8_t index) {
  s_item = index;
  s_rep = 1;
  enter_phase(s_settings.lead_in_enabled ? PHASE_LEAD_IN : PHASE_INHALE);
}

static void advance_phase(void) {
  switch (s_phase) {
    case PHASE_LEAD_IN:
      enter_phase(PHASE_INHALE);
      break;
    case PHASE_INHALE:
      enter_phase(PHASE_EXHALE);
      break;
    case PHASE_EXHALE:
      if (s_rep < cur_reps()) {
        s_rep++;
        enter_phase(PHASE_INHALE);
      } else if (s_item + 1 < s_session.item_count) {
        if (cur_item()->rest_after_sec > 0) {
          enter_phase(PHASE_REST);
        } else {
          start_item(s_item + 1);
        }
      } else {
        enter_phase(PHASE_DONE);
      }
      break;
    case PHASE_REST:
      start_item(s_item + 1);
      break;
    case PHASE_DONE:
      break;
  }
}

// ---------------------------------------------------------------------------
// Drawing

static void draw_chevron(GContext *ctx, int cx, int cy, bool right) {
  graphics_context_set_stroke_color(ctx, C_INK);
  graphics_context_set_stroke_width(ctx, 3);
  int dx = right ? 4 : -4;
  graphics_draw_line(ctx, GPoint(cx - dx, cy - 6), GPoint(cx + dx, cy));
  graphics_draw_line(ctx, GPoint(cx + dx, cy), GPoint(cx - dx, cy + 6));
}

static void draw_pause(GContext *ctx, int cx, int cy) {
  graphics_context_set_fill_color(ctx, C_PURPLE);
  graphics_fill_rect(ctx, GRect(cx - 5, cy - 6, 4, 12), 1, GCornersAll);
  graphics_fill_rect(ctx, GRect(cx + 1, cy - 6, 4, 12), 1, GCornersAll);
}

static void draw_play(GContext *ctx, int cx, int cy) {
  graphics_context_set_fill_color(ctx, C_PURPLE);
  GPathInfo info = {
    .num_points = 3,
    .points = (GPoint[]) {{cx - 4, cy - 6}, {cx + 6, cy}, {cx - 4, cy + 6}},
  };
  GPath *tri = gpath_create(&info);
  gpath_draw_filled(ctx, tri);
  gpath_destroy(tri);
}

static void draw_rail(GContext *ctx, bool resting) {
  graphics_context_set_stroke_color(ctx, C_RAIL);
  graphics_context_set_stroke_width(ctx, 1);
  graphics_draw_line(ctx, GPoint(RAIL_X, TITLE_H), GPoint(RAIL_X, SCREEN_H));
  draw_chevron(ctx, RAIL_CX, 54, false);                 // up = prev
  if (resting) draw_play(ctx, RAIL_CX, 126);             // resume/skip rest
  else if (s_paused) draw_play(ctx, RAIL_CX, 126);       // paused -> resume
  else draw_pause(ctx, RAIL_CX, 126);                    // running -> pause
  draw_chevron(ctx, RAIL_CX, 196, true);                 // down = skip
}

static void draw_title_bar(GContext *ctx, GColor bg, const char *title) {
  graphics_context_set_fill_color(ctx, bg);
  graphics_fill_rect(ctx, GRect(0, 0, SCREEN_W, TITLE_H), 0, GCornerNone);

  char up[SESSION_NAME_LEN];
  to_uppercase(title, up, sizeof(up));
  graphics_context_set_text_color(ctx, GColorWhite);
  graphics_draw_text(ctx, up, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(6, 1, 130, TITLE_H), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentLeft, NULL);

  char pos[8];
  snprintf(pos, sizeof(pos), "%02d/%02d", s_item + 1, s_session.item_count);
  graphics_draw_text(ctx, pos, fonts_get_system_font(FONT_KEY_GOTHIC_18),
                     GRect(110, 1, RAIL_X - 112, TITLE_H), GTextOverflowModeFill,
                     GTextAlignmentRight, NULL);
}

// Draws the ring band + progress arc. arc_fraction in [0,1] from 12 o'clock CW.
static void draw_ring(GContext *ctx, int arc_num, int arc_den) {
  GRect r = GRect(RING_CX - RING_D / 2, RING_CY - RING_D / 2, RING_D, RING_D);
  graphics_context_set_fill_color(ctx, C_RING_TRK);
  graphics_fill_radial(ctx, r, GOvalScaleModeFitCircle, RING_INSET, 0, TRIG_MAX_ANGLE);
  if (arc_den > 0 && arc_num > 0) {
    int32_t end = (int32_t)(TRIG_MAX_ANGLE * arc_num / arc_den);
    graphics_context_set_fill_color(ctx, C_PURPLE);
    graphics_fill_radial(ctx, r, GOvalScaleModeFitCircle, RING_INSET, 0, end);
  }
}

static void draw_center_count(GContext *ctx, const char *label, const char *count) {
  if (label) {
    graphics_context_set_text_color(ctx, C_PURPLE);
    graphics_draw_text(ctx, label, fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                       GRect(RING_CX - 54, RING_CY - 28, 108, 16),
                       GTextOverflowModeFill, GTextAlignmentCenter, NULL);
  }
  graphics_context_set_text_color(ctx, C_INK);
  graphics_draw_text(ctx, count, fonts_get_system_font(FONT_KEY_BITHAM_42_BOLD),
                     GRect(RING_CX - 54, RING_CY - 14, 108, 48),
                     GTextOverflowModeFill, GTextAlignmentCenter, NULL);
}

static void draw_cycles_bar(GContext *ctx) {
  graphics_context_set_text_color(ctx, C_MUTED);
  graphics_draw_text(ctx, "CYCLES", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(8, 156, 80, 18), GTextOverflowModeFill, GTextAlignmentLeft, NULL);
  char cyc[12];
  snprintf(cyc, sizeof(cyc), "%d / %d", s_rep, cur_reps());
  graphics_context_set_text_color(ctx, C_INK);
  graphics_draw_text(ctx, cyc, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(88, 153, 80, 20), GTextOverflowModeFill, GTextAlignmentRight, NULL);

  GRect track = GRect(8, 180, RAIL_X - 16, 6);
  graphics_context_set_fill_color(ctx, C_BAR_TRK);
  graphics_fill_rect(ctx, track, 3, GCornersAll);
  int reps = cur_reps();
  if (reps > 0) {
    int w = (track.size.w * s_rep) / reps;
    graphics_context_set_fill_color(ctx, C_PURPLE);
    graphics_fill_rect(ctx, GRect(track.origin.x, track.origin.y, w, 6), 3, GCornersAll);
  }
}

static void render_active(GContext *ctx) {
  draw_title_bar(ctx, C_PURPLE, cur_item()->name);

  uint32_t dur = phase_duration_ms();
  int total_secs = dur / 1000;
  int p_num = (int)s_elapsed_ms;
  int p_den = (int)dur;

  // Ring arc tracks progress through the current phase.
  draw_ring(ctx, p_num, p_den == 0 ? 1 : p_den);

  // Breathing disc: expand on inhale, contract on exhale (coarse steps).
  int radius;
  if (s_phase == PHASE_INHALE) {
    radius = DISC_MIN + (DISC_MAX - DISC_MIN) * p_num / (p_den ? p_den : 1);
  } else if (s_phase == PHASE_EXHALE) {
    radius = DISC_MAX - (DISC_MAX - DISC_MIN) * p_num / (p_den ? p_den : 1);
  } else {
    radius = DISC_MIN;  // lead-in: small, steady
  }
  graphics_context_set_fill_color(ctx, C_DISC);
  graphics_fill_circle(ctx, GPoint(RING_CX, RING_CY), radius);

  // Center label + count.
  const char *label = s_phase == PHASE_INHALE ? "INHALE"
                    : s_phase == PHASE_EXHALE ? "EXHALE" : "READY";
  char count[6];
  if (s_phase == PHASE_LEAD_IN) {
    int remaining = 3 - (int)(s_elapsed_ms / 1000);
    if (remaining < 1) remaining = 1;
    snprintf(count, sizeof(count), "%d", remaining);
  } else {
    int n = (int)(s_elapsed_ms / 1000) + 1;
    if (n > total_secs) n = total_secs;
    snprintf(count, sizeof(count), "%d", n);
  }
  draw_center_count(ctx, label, count);

  draw_cycles_bar(ctx);
  draw_rail(ctx, false);
}

static void render_rest(GContext *ctx) {
  draw_title_bar(ctx, C_INK, "REST");

  uint32_t dur = phase_duration_ms();
  int remaining = (dur - s_elapsed_ms) / 1000;
  if (remaining < 0) remaining = 0;
  // Ring depletes as rest counts down.
  draw_ring(ctx, (int)(dur - s_elapsed_ms), dur == 0 ? 1 : (int)dur);

  char tm[12];
  snprintf(tm, sizeof(tm), "%d:%02d", remaining / 60, remaining % 60);
  draw_center_count(ctx, "REST", tm);

  // NEXT UP block.
  graphics_context_set_text_color(ctx, C_MUTED);
  graphics_draw_text(ctx, "NEXT UP", fonts_get_system_font(FONT_KEY_GOTHIC_14_BOLD),
                     GRect(8, 156, RAIL_X - 16, 16), GTextOverflowModeFill,
                     GTextAlignmentCenter, NULL);
  const char *next = (s_item + 1 < s_session.item_count)
                   ? s_session.items[s_item + 1].name : "Done";
  graphics_context_set_text_color(ctx, C_INK);
  graphics_draw_text(ctx, next, fonts_get_system_font(FONT_KEY_GOTHIC_18_BOLD),
                     GRect(8, 172, RAIL_X - 16, 24), GTextOverflowModeTrailingEllipsis,
                     GTextAlignmentCenter, NULL);
  if (s_item + 1 < s_session.item_count) {
    char reps[24];
    snprintf(reps, sizeof(reps), "%d reps", s_session.items[s_item + 1].reps);
    graphics_context_set_text_color(ctx, C_MUTED);
    graphics_draw_text(ctx, reps, fonts_get_system_font(FONT_KEY_GOTHIC_14),
                       GRect(8, 196, RAIL_X - 16, 18), GTextOverflowModeFill,
                       GTextAlignmentCenter, NULL);
  }

  draw_rail(ctx, true);
}

static void render_done(GContext *ctx) {
  draw_title_bar(ctx, C_PURPLE, s_session.name);
  graphics_context_set_text_color(ctx, C_INK);
  graphics_draw_text(ctx, "Session\ncomplete", fonts_get_system_font(FONT_KEY_BITHAM_30_BLACK),
                     GRect(8, 70, SCREEN_W - 16, 80), GTextOverflowModeFill,
                     GTextAlignmentCenter, NULL);
  graphics_context_set_text_color(ctx, C_MUTED);
  graphics_draw_text(ctx, "Back to exit", fonts_get_system_font(FONT_KEY_GOTHIC_14),
                     GRect(8, 170, SCREEN_W - 16, 20), GTextOverflowModeFill,
                     GTextAlignmentCenter, NULL);
}

static void canvas_update(Layer *layer, GContext *ctx) {
  graphics_context_set_fill_color(ctx, C_BG);
  graphics_fill_rect(ctx, layer_get_bounds(layer), 0, GCornerNone);

  if (s_phase == PHASE_DONE) render_done(ctx);
  else if (s_phase == PHASE_REST) render_rest(ctx);
  else render_active(ctx);
}

// ---------------------------------------------------------------------------
// Tick

static void tick(void *data) {
  s_timer = app_timer_register(TICK_MS, tick, NULL);
  if (s_paused || s_phase == PHASE_DONE) {
    return;
  }
  s_elapsed_ms += TICK_MS;

  // Per-second pulse during the lead-in count.
  if (s_phase == PHASE_LEAD_IN) {
    int sec = (int)(s_elapsed_ms / 1000);
    if (sec != s_last_second) {
      s_last_second = sec;
      if (s_settings.haptics_enabled) vibes_short_pulse();
    }
  }

  if (s_elapsed_ms >= phase_duration_ms()) {
    advance_phase();
  }
  layer_mark_dirty(s_canvas);
}

// ---------------------------------------------------------------------------
// Buttons

static void click_up(ClickRecognizerRef rec, void *ctx) {
  // Previous exercise (or restart current if at the first).
  start_item(s_item > 0 ? s_item - 1 : 0);
  layer_mark_dirty(s_canvas);
}

static void click_select(ClickRecognizerRef rec, void *ctx) {
  if (s_phase == PHASE_DONE) return;
  s_paused = !s_paused;
  layer_mark_dirty(s_canvas);
}

static void click_down(ClickRecognizerRef rec, void *ctx) {
  // Skip to next exercise (or end the rest).
  if (s_phase == PHASE_REST) {
    advance_phase();
  } else if (s_item + 1 < s_session.item_count) {
    start_item(s_item + 1);
  } else {
    enter_phase(PHASE_DONE);
  }
  layer_mark_dirty(s_canvas);
}

static void click_config(void *ctx) {
  window_single_click_subscribe(BUTTON_ID_UP, click_up);
  window_single_click_subscribe(BUTTON_ID_SELECT, click_select);
  window_single_click_subscribe(BUTTON_ID_DOWN, click_down);
}

// ---------------------------------------------------------------------------
// Window lifecycle

static void window_load(Window *window) {
  Layer *root = window_get_root_layer(window);
  s_canvas = layer_create(layer_get_bounds(root));
  layer_set_update_proc(s_canvas, canvas_update);
  layer_add_child(root, s_canvas);
}

static void window_unload(Window *window) {
  layer_destroy(s_canvas);
}

void runner_init(const Session *session, const Settings *settings) {
  s_session = *session;
  s_settings = *settings;
  s_paused = false;
}

void runner_push(void) {
  s_window = window_create();
  window_set_background_color(s_window, C_BG);
  window_set_click_config_provider(s_window, click_config);
  window_set_window_handlers(s_window, (WindowHandlers) {
    .load = window_load,
    .unload = window_unload,
  });
  window_stack_push(s_window, true);

  start_item(0);
  s_timer = app_timer_register(TICK_MS, tick, NULL);
}

void runner_deinit(void) {
  if (s_timer) app_timer_cancel(s_timer);
  vibes_cancel();
  if (s_window) window_destroy(s_window);
}
