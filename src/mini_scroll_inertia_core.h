/* Right-pad-only inertia math/state. No ZMK, Bluetooth, sensor or HID dependencies. */
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include <string.h>

struct mini_inertia_config {
    int32_t tick_ms, release_ms, decay, blend, min_samples;
    int32_t start_milli, stop_milli, limit_milli, max_ms;
};
struct mini_inertia_state {
    int32_t velocity[2], remainder[2]; /* milli scroll units per tick, AFTER scaler */
    int32_t pending[2];
    int64_t axis_time[2], axis_last_event[2], last_input, coast_start;
    bool seen[2], tracking, coasting;
    uint16_t generation;
    uint32_t samples;
};
static inline int32_t mini_abs(int32_t v) { return v < 0 ? -v : v; }
static inline int32_t mini_clamp(int64_t v, int32_t cap) {
    return v > cap ? cap : (v < -cap ? -cap : (int32_t)v);
}
static inline void mini_cancel(struct mini_inertia_state *s) {
    uint16_t next = s->generation + 1;
    memset(s, 0, sizeof(*s));
    s->generation = next;
}
/* Every physical event invalidates already queued coast output, including zero deltas. */
static inline void mini_input(struct mini_inertia_state *s, const struct mini_inertia_config *c,
                              int axis, int32_t delta, int64_t now) {
    if (axis < 0 || s->coasting || (s->tracking && now - s->last_input >= c->release_ms)) {
        mini_cancel(s);
    }
    s->generation++;
    if (axis < 0) { return; }
    s->tracking = true;
    s->last_input = now;
    s->axis_last_event[axis] = now;
    if (!s->seen[axis]) {
        s->seen[axis] = true;
        s->axis_time[axis] = now;
        return; /* No interval is known for the first event. */
    }
    s->pending[axis] = mini_clamp((int64_t)s->pending[axis] + delta, INT32_MAX);
    /* Split packets can arrive in bursts. Accumulate a full tick of input instead
     * of treating a 0/1 ms arrival gap as a sudden, extremely fast physical motion. */
    if (now - s->axis_time[axis] >= c->tick_ms) {
        int64_t sample = (int64_t)s->pending[axis] * 1000 * c->tick_ms /
                         (now - s->axis_time[axis]);
        int32_t bounded = mini_clamp(sample, c->limit_milli);
        s->velocity[axis] = (int64_t)s->velocity[axis] * c->blend / 1000 +
                            (int64_t)bounded * (1000 - c->blend) / 1000;
        if (s->pending[axis] != 0 && s->samples < UINT32_MAX) { s->samples++; }
        s->pending[axis] = 0;
        s->axis_time[axis] = now;
    }
}
/* Return true only for a live coast. The backend uses a real-input silence timer. */
static inline bool mini_tick(struct mini_inertia_state *s, const struct mini_inertia_config *c,
                             int64_t now, bool blocked, int16_t output[2]) {
    output[0] = output[1] = 0;
    if (blocked) { mini_cancel(s); return false; }
    if (!s->tracking) { return false; }
    if (!s->coasting) {
        if (now - s->last_input < c->release_ms) { return false; }
        /* Don't reuse velocity from an axis that stopped long before the other. */
        for (int a = 0; a < 2; a++) {
            if (s->last_input - s->axis_last_event[a] >= c->release_ms) { s->velocity[a] = 0; }
        }
        if (s->samples < (uint32_t)c->min_samples ||
            (mini_abs(s->velocity[0]) < c->start_milli &&
             mini_abs(s->velocity[1]) < c->start_milli)) {
            mini_cancel(s); return false;
        }
        s->coasting = true;
        s->coast_start = now;
    }
    if (now - s->coast_start >= c->max_ms ||
        (mini_abs(s->velocity[0]) < c->stop_milli &&
         mini_abs(s->velocity[1]) < c->stop_milli)) {
        mini_cancel(s); return false;
    }
    for (int a = 0; a < 2; a++) {
        int32_t total = s->velocity[a] + s->remainder[a];
        output[a] = mini_clamp(total / 1000, 127);
        s->remainder[a] = total - (int32_t)output[a] * 1000;
        s->velocity[a] = (int64_t)s->velocity[a] * c->decay / 1000;
    }
    return true;
}
/* Each synthetic event carries its epoch, so old queued output cannot survive mode toggles.
 * At most CONFIG_INPUT_QUEUE_MAX_MSGS events can be pending; epochs wrap only after 65536
 * real events/cancellations. No HID globals are touched by this processor. */
static inline int32_t mini_pack(uint16_t generation, int16_t delta) {
    return (int32_t)(((uint32_t)generation << 16) | (uint16_t)delta);
}
static inline bool mini_unpack(const struct mini_inertia_state *s, bool blocked,
                               int32_t encoded, int32_t *value) {
    if (blocked || !s->coasting || (uint16_t)((uint32_t)encoded >> 16) != s->generation) {
        return false;
    }
    *value = (int16_t)(encoded & 0xffff);
    return true;
}
