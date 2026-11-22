#define DT_DRV_COMPAT torabo_ball_scroll_inertia

#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <drivers/input_processor.h>
#include <zmk/event_manager.h>
#include <zmk/events/layer_state_changed.h>
#include <zmk/keymap.h>
#include "mini_scroll_inertia_core.h"
#include "../boards/shields/torabo_tsuki_lp/ball-scroll-settings.h"

enum ball_processor_stage {
    CHECK_RAW_DIRECTION,
    OBSERVE_SCALED_SCROLL,
    FILTER_COAST_OUTPUT,
};

static const struct mini_inertia_config config = {
    .tick_ms = TORABO_BALL_INERTIA_TICK_MS,
    .release_ms = TORABO_BALL_INERTIA_IDLE_MS,
    .decay = TORABO_BALL_INERTIA_DECAY_PERMILLE,
    .blend = 500,
    .min_samples = 2,
    .start_milli = 750,
    .stop_milli = 250,
    .limit_milli = TORABO_BALL_INERTIA_LIMIT_MILLI,
    .max_ms = TORABO_BALL_INERTIA_MAX_MS,
};

static struct mini_inertia_state coast;
static struct k_spinlock coast_lock;
static int8_t direction[2];

static void emit_coast(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(coast_work, emit_coast);

static bool scroll_mode(void) {
    return zmk_keymap_layer_active(TORABO_BALL_SCROLL_LAYER);
}

static void cancel_coast(void) {
    mini_cancel(&coast);
    direction[0] = direction[1] = 0;
    k_work_cancel_delayable(&coast_work);
}

static void emit_coast(struct k_work *work) {
    ARG_UNUSED(work);
    int16_t delta[2];
    k_spinlock_key_t key = k_spin_lock(&coast_lock);
    bool live = mini_tick(&coast, &config, k_uptime_get(), !scroll_mode(), delta);
    uint16_t generation = coast.generation;
    if (live) {
        k_work_reschedule(&coast_work, K_MSEC(config.tick_ms));
    }
    k_spin_unlock(&coast_lock, key);

    if (!live) {
        return;
    }
    for (int axis = 0; axis < 2; axis++) {
        if (delta[axis] == 0) {
            continue;
        }
        int rc = input_report_rel(DEVICE_DT_INST_GET(0),
                                  axis == 0 ? INPUT_REL_HWHEEL : INPUT_REL_WHEEL,
                                  mini_pack(generation, delta[axis]), true, K_NO_WAIT);
        if (rc < 0) {
            key = k_spin_lock(&coast_lock);
            if (coast.generation == generation) {
                cancel_coast();
            }
            k_spin_unlock(&coast_lock, key);
            return;
        }
    }
}

static int process_event(const struct device *dev, struct input_event *evt, uint32_t stage,
                         uint32_t unused, struct zmk_input_processor_state *state) {
    ARG_UNUSED(dev);
    ARG_UNUSED(unused);
    ARG_UNUSED(state);
    if (!DT_INST_PROP(0, enabled)) {
        return stage == FILTER_COAST_OUTPUT ? ZMK_INPUT_PROC_STOP : ZMK_INPUT_PROC_CONTINUE;
    }

    k_spinlock_key_t key = k_spin_lock(&coast_lock);
    if (stage == FILTER_COAST_OUTPUT) {
        bool valid = mini_unpack(&coast, !scroll_mode(), evt->value, &evt->value);
        k_spin_unlock(&coast_lock, key);
        return valid ? ZMK_INPUT_PROC_CONTINUE : ZMK_INPUT_PROC_STOP;
    }

    if (scroll_mode() && evt->type == INPUT_EV_REL) {
        if (stage == CHECK_RAW_DIRECTION &&
            (evt->code == INPUT_REL_X || evt->code == INPUT_REL_Y)) {
            int axis = evt->code == INPUT_REL_X ? 0 : 1;
            if (evt->value != 0) {
                int8_t sign = evt->value < 0 ? -1 : 1;
                /* Check raw motion so a reversal below the 1/40 step clears
                 * old velocity too. The real event continues through unchanged. */
                if (coast.coasting || (direction[axis] != 0 && direction[axis] != sign)) {
                    cancel_coast();
                }
                direction[axis] = sign;
            }
        } else if (stage == OBSERVE_SCALED_SCROLL &&
                   (evt->code == INPUT_REL_HWHEEL || evt->code == INPUT_REL_WHEEL)) {
            int axis = evt->code == INPUT_REL_HWHEEL ? 0 : 1;
            mini_input(&coast, &config, axis, evt->value, k_uptime_get());
            k_work_reschedule(&coast_work, K_MSEC(config.release_ms));
        }
    }
    k_spin_unlock(&coast_lock, key);
    return ZMK_INPUT_PROC_CONTINUE;
}

static int layer_changed(const zmk_event_t *event) {
    const struct zmk_layer_state_changed *layer = as_zmk_layer_state_changed(event);
    if (layer->layer == TORABO_BALL_SCROLL_LAYER) {
        k_spinlock_key_t key = k_spin_lock(&coast_lock);
        cancel_coast();
        k_spin_unlock(&coast_lock, key);
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(ball_scroll_inertia, layer_changed);
ZMK_SUBSCRIPTION(ball_scroll_inertia, zmk_layer_state_changed);

static const struct zmk_input_processor_driver_api inertia_api = {.handle_event = process_event};
DEVICE_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL,
                      CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &inertia_api);
