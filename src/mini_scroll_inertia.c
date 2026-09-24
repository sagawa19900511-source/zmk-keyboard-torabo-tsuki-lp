/* Central-only processor for the right mini trackpad. The sensor and split are unchanged. */
#define DT_DRV_COMPAT torabo_mini_scroll_inertia
#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <drivers/input_processor.h>
#include <zmk/keymap.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include "mini_scroll_inertia_core.h"
#include "../snippets/input-split-listener/mini-trackpad-settings.h"

#if DT_HAS_COMPAT_STATUS_OKAY(DT_DRV_COMPAT)
BUILD_ASSERT(DT_NUM_INST_STATUS_OKAY(DT_DRV_COMPAT) == 1, "Right pad has one inertia instance");
BUILD_ASSERT(MINI_TRACKPAD_MOUSE_LAYER < ZMK_KEYMAP_LAYERS_LEN,
             "The dedicated mini trackpad layer must exist in the Studio keymap");
BUILD_ASSERT(MINI_TRACKPAD_INERTIA_TICK_MS > 0 &&
             MINI_TRACKPAD_INERTIA_RELEASE_MS > MINI_TRACKPAD_INERTIA_TICK_MS);
BUILD_ASSERT(MINI_TRACKPAD_INERTIA_DECAY_PERMILLE > 0 && MINI_TRACKPAD_INERTIA_DECAY_PERMILLE < 1000);
BUILD_ASSERT(MINI_TRACKPAD_INERTIA_BLEND_PERMILLE >= 0 && MINI_TRACKPAD_INERTIA_BLEND_PERMILLE < 1000);
BUILD_ASSERT(MINI_TRACKPAD_INERTIA_START_MILLI >= MINI_TRACKPAD_INERTIA_STOP_MILLI &&
             MINI_TRACKPAD_INERTIA_STOP_MILLI > 0 && MINI_TRACKPAD_INERTIA_LIMIT_MILLI <= 127000 &&
             MINI_TRACKPAD_INERTIA_LIMIT_MILLI >= MINI_TRACKPAD_INERTIA_START_MILLI &&
             MINI_TRACKPAD_INERTIA_MAX_MS > 0 && MINI_TRACKPAD_INERTIA_MIN_SAMPLES > 0);
static const bool enabled = DT_INST_PROP(0, enabled);
static const struct mini_inertia_config cfg = {
    .tick_ms = MINI_TRACKPAD_INERTIA_TICK_MS,
    .release_ms = MINI_TRACKPAD_INERTIA_RELEASE_MS,
    .decay = MINI_TRACKPAD_INERTIA_DECAY_PERMILLE,
    .blend = MINI_TRACKPAD_INERTIA_BLEND_PERMILLE,
    .min_samples = MINI_TRACKPAD_INERTIA_MIN_SAMPLES,
    .start_milli = MINI_TRACKPAD_INERTIA_START_MILLI,
    .stop_milli = MINI_TRACKPAD_INERTIA_STOP_MILLI,
    .limit_milli = MINI_TRACKPAD_INERTIA_LIMIT_MILLI,
    .max_ms = MINI_TRACKPAD_INERTIA_MAX_MS,
};
static struct mini_inertia_state state;
static struct k_spinlock lock;
static struct k_work_delayable tick_work;
static bool cursor_active;

static void cancel_locked(void) {
    mini_cancel(&state);
    k_work_cancel_delayable(&tick_work);
}
static void coast_tick(struct k_work *work) {
    ARG_UNUSED(work);
    int16_t out[2];
    k_spinlock_key_t key = k_spin_lock(&lock);
    bool live = mini_tick(&state, &cfg, k_uptime_get(),
                          !enabled || cursor_active ||
                              zmk_keymap_layer_active(MINI_TRACKPAD_MOUSE_LAYER), out);
    uint16_t generation = state.generation;
    if (live) { k_work_reschedule(&tick_work, K_MSEC(cfg.tick_ms)); }
    k_spin_unlock(&lock, key);
    if (!live) { return; }
    const struct device *dev = DEVICE_DT_GET(DT_DRV_INST(0));
    /* Nonblocking, individually synchronized events: no half-frame can linger after cancel.
     * The guard checks epoch and Layer 6 again on the input thread before normal HID handling. */
    for (int a = 0; a < 2; a++) {
        if (!out[a]) { continue; }
        int rc = input_report_rel(dev, a == 0 ? INPUT_REL_HWHEEL : INPUT_REL_WHEEL,
                                  mini_pack(generation, out[a]), true, K_NO_WAIT);
        if (rc < 0) {
            key = k_spin_lock(&lock);
            if (state.generation == generation) { cancel_locked(); }
            k_spin_unlock(&lock, key);
            return;
        }
    }
}
static int handle_event(const struct device *dev, struct input_event *evt, uint32_t mode,
                        uint32_t unused, struct zmk_input_processor_state *processor_state) {
    ARG_UNUSED(dev); ARG_UNUSED(unused); ARG_UNUSED(processor_state);
    k_spinlock_key_t key = k_spin_lock(&lock);
    /* Mode 1: synthetic-only listener. Decode after cancellation/queue checks. */
    if (mode == 1) {
        bool valid = evt->type == INPUT_EV_REL &&
                     (evt->code == INPUT_REL_WHEEL || evt->code == INPUT_REL_HWHEEL) &&
                     mini_unpack(&state, !enabled || cursor_active ||
                                             zmk_keymap_layer_active(MINI_TRACKPAD_MOUSE_LAYER),
                                 evt->value, &evt->value);
        k_spin_unlock(&lock, key);
        return valid ? ZMK_INPUT_PROC_CONTINUE : ZMK_INPUT_PROC_STOP;
    }
    /* Mode 2: cursor chain; never track/inject inertia here. */
    if (mode == 2 || !enabled || cursor_active) {
        cancel_locked();
        k_spin_unlock(&lock, key);
        return ZMK_INPUT_PROC_CONTINUE;
    }
    int axis = -1;
    if (evt->type == INPUT_EV_REL) {
        if (evt->code == INPUT_REL_HWHEEL) { axis = 0; }
        if (evt->code == INPUT_REL_WHEEL) { axis = 1; }
    }
    mini_input(&state, &cfg, axis, evt->value, k_uptime_get());
    if (axis >= 0) { k_work_reschedule(&tick_work, K_MSEC(cfg.release_ms)); }
    else { k_work_cancel_delayable(&tick_work); }
    k_spin_unlock(&lock, key);
    return ZMK_INPUT_PROC_CONTINUE; /* Direct scroll is byte-for-byte untouched. */
}
static int layer_changed(const zmk_event_t *event) {
    const struct zmk_layer_state_changed *ev = as_zmk_layer_state_changed(event);
    if (!ev || ev->layer != MINI_TRACKPAD_MOUSE_LAYER) { return ZMK_EV_EVENT_BUBBLE; }
    k_spinlock_key_t key = k_spin_lock(&lock);
    cursor_active = ev->state;
    cancel_locked(); /* Both entry and exit; sub-tick toggles cannot revive stale output. */
    k_spin_unlock(&lock, key);
    return ZMK_EV_EVENT_BUBBLE;
}
ZMK_LISTENER(mini_scroll_inertia, layer_changed);
ZMK_SUBSCRIPTION(mini_scroll_inertia, zmk_layer_state_changed);
static int init(const struct device *dev) {
    ARG_UNUSED(dev);
    k_work_init_delayable(&tick_work, coast_tick);
    return 0;
}
static const struct zmk_input_processor_driver_api api = {.handle_event = handle_event};
DEVICE_DT_INST_DEFINE(0, init, NULL, NULL, NULL, POST_KERNEL,
                      CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &api);
#endif
