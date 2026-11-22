#include <zephyr/device.h>
#include <zephyr/input/input.h>
#include <zephyr/kernel.h>
#include <drivers/behavior.h>
#include <drivers/input_processor.h>
#include <zmk/behavior.h>
#include <zmk/event_manager.h>
#include <zmk/events/position_state_changed.h>
#include <zmk/keymap.h>

#define AML_LAYER 4

K_MUTEX_DEFINE(aml_lock);
static unsigned int manual_holds;
static int64_t auto_until;
static bool owns_layer;

static void update_layer(void) {
    bool active = manual_holds > 0 || auto_until > k_uptime_get();
    if (active == owns_layer) {
        return;
    }
    owns_layer = active;
    if (active) {
        zmk_keymap_layer_activate(AML_LAYER);
    } else {
        zmk_keymap_layer_deactivate(AML_LAYER);
    }
}

static void update_from_motion(struct k_work *work);
static K_WORK_DELAYABLE_DEFINE(aml_work, update_from_motion);

static void update_from_motion(struct k_work *work) {
    ARG_UNUSED(work);
    k_mutex_lock(&aml_lock, K_FOREVER);
    update_layer();
    int64_t remaining = auto_until - k_uptime_get();
    if (remaining > 0) {
        k_work_reschedule(&aml_work, K_MSEC(remaining));
    }
    k_mutex_unlock(&aml_lock);
}

static int on_position(const zmk_event_t *event) {
    const struct zmk_position_state_changed *pos = as_zmk_position_state_changed(event);
    if (!pos->state) {
        return ZMK_EV_EVENT_BUBBLE;
    }

    k_mutex_lock(&aml_lock, K_FOREVER);
    if (auto_until != 0) {
        /* ZMK resolves this through the selected physical layout and Studio's
         * current bindings. Keep explicit mouse actions such as clicks and MO(5).
         */
        const struct zmk_behavior_binding *binding =
            zmk_keymap_get_layer_binding_at_idx(AML_LAYER, pos->position);
        if (auto_until <= k_uptime_get() || !binding || !binding->behavior_dev ||
            strcmp(binding->behavior_dev, DEVICE_DT_NAME(DT_NODELABEL(trans))) == 0) {
            auto_until = 0;
            k_work_cancel_delayable(&aml_work);
            update_layer();
        }
    }
    k_mutex_unlock(&aml_lock);
    /* The original event proceeds once. ZMK remembers its layer for release. */
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(torabo_aml, on_position);
ZMK_SUBSCRIPTION(torabo_aml, zmk_position_state_changed);

static int on_motion(const struct device *dev, struct input_event *event, uint32_t layer,
                     uint32_t timeout, struct zmk_input_processor_state *state) {
    ARG_UNUSED(dev);
    ARG_UNUSED(state);
    if (layer != AML_LAYER) {
        return -EINVAL;
    }
    if (event->type == INPUT_EV_REL && event->value &&
        (event->code == INPUT_REL_X || event->code == INPUT_REL_Y)) {
        k_mutex_lock(&aml_lock, K_FOREVER);
        bool needs_activation = auto_until <= k_uptime_get() && !owns_layer;
        auto_until = k_uptime_get() + timeout;
        /* Layer changes and their listeners run on the system work queue. */
        if (needs_activation || !owns_layer) {
            k_work_reschedule(&aml_work, K_NO_WAIT);
        } else {
            k_work_reschedule(&aml_work, K_MSEC(timeout));
        }
        k_mutex_unlock(&aml_lock);
    }
    return ZMK_INPUT_PROC_CONTINUE;
}

static const struct zmk_input_processor_driver_api aml_api = {.handle_event = on_motion};
DEVICE_DT_DEFINE(DT_NODELABEL(ball_aml), NULL, NULL, NULL, NULL,
                 POST_KERNEL, CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &aml_api);

static int manual_pressed(struct zmk_behavior_binding *binding,
                          struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);
    if (binding->param1 != AML_LAYER) {
        return -EINVAL;
    }
    k_mutex_lock(&aml_lock, K_FOREVER);
    manual_holds++;
    update_layer();
    k_mutex_unlock(&aml_lock);
    return ZMK_BEHAVIOR_OPAQUE;
}

static int manual_released(struct zmk_behavior_binding *binding,
                           struct zmk_behavior_binding_event event) {
    ARG_UNUSED(event);
    if (binding->param1 != AML_LAYER) {
        return -EINVAL;
    }
    k_mutex_lock(&aml_lock, K_FOREVER);
    if (manual_holds > 0) {
        manual_holds--;
    }
    update_layer();
    k_mutex_unlock(&aml_lock);
    return ZMK_BEHAVIOR_OPAQUE;
}

#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
static const struct behavior_parameter_value_metadata manual_values[] = {
    {.display_name = "Mouse Layer", .type = BEHAVIOR_PARAMETER_VALUE_TYPE_VALUE, .value = AML_LAYER},
};
static const struct behavior_parameter_metadata_set manual_set = {
    .param1_values = manual_values,
    .param1_values_len = ARRAY_SIZE(manual_values),
};
static const struct behavior_parameter_metadata manual_metadata = {.sets = &manual_set, .sets_len = 1};
#endif

static const struct behavior_driver_api manual_api = {
    .binding_pressed = manual_pressed,
    .binding_released = manual_released,
#if IS_ENABLED(CONFIG_ZMK_BEHAVIOR_METADATA)
    .parameter_metadata = &manual_metadata,
#endif
};

#define DT_DRV_COMPAT torabo_behavior_manual_mouse
BEHAVIOR_DT_INST_DEFINE(0, NULL, NULL, NULL, NULL, POST_KERNEL,
                        CONFIG_KERNEL_INIT_PRIORITY_DEFAULT, &manual_api);
