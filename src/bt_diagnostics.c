// SPDX-License-Identifier: GPL-3.0-or-later
// Diagnostic-only profile display. No changes to BLE, HID or stored settings.

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/init.h>
#include <zephyr/kernel.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/atomic.h>
#include <zmk/ble.h>
#include <zmk/event_manager.h>
#include <zmk/events/ble_active_profile_changed.h>

LOG_MODULE_REGISTER(bt_diagnostics, LOG_LEVEL_INF);

static const struct gpio_dt_spec led = GPIO_DT_SPEC_GET(DT_NODELABEL(status_led), gpios);
static atomic_t ready;
static atomic_t restart;
// Only the work handler accesses the pulse state.
static bool led_on;
static uint8_t remaining;
static uint16_t pulse_ms;

static void display_profile(struct k_work *work);
K_WORK_DELAYABLE_DEFINE(profile_work, display_profile);

static void display_profile(struct k_work *work) {
    if (atomic_set(&restart, 0)) {
        led_on = false;
        remaining = 0;
        gpio_pin_set_dt(&led, 0);
    }

    if (led_on) {
        gpio_pin_set_dt(&led, 0);
        led_on = false;
        --remaining;
        k_work_reschedule(&profile_work, K_MSEC(remaining ? 250 : 2500));
        return;
    }

    if (remaining == 0) {
        remaining = zmk_ble_active_profile_index() + 1;
        pulse_ms = zmk_ble_active_profile_is_connected() ? 100 : 400;
    }
    gpio_pin_set_dt(&led, 1);
    led_on = true;
    k_work_reschedule(&profile_work, K_MSEC(pulse_ms));
}

static int profile_changed(const zmk_event_t *eh) {
    const struct zmk_ble_active_profile_changed *event = as_zmk_ble_active_profile_changed(eh);
    if (event && atomic_get(&ready)) {
        LOG_INF("BT_DIAG profile=%u connected=%d open=%d", event->index,
                zmk_ble_active_profile_is_connected(), zmk_ble_active_profile_is_open());
        atomic_set(&restart, 1);
        // Give a partial previous pulse group a visible gap before restarting.
        k_work_reschedule(&profile_work, K_MSEC(600));
    }
    return ZMK_EV_EVENT_BUBBLE;
}

ZMK_LISTENER(bt_diagnostics, profile_changed);
ZMK_SUBSCRIPTION(bt_diagnostics, zmk_ble_active_profile_changed);

static int diagnostics_init(void) {
    if (!device_is_ready(led.port)) {
        return -ENODEV;
    }
    int err = gpio_pin_configure_dt(&led, GPIO_OUTPUT_INACTIVE);
    if (err) {
        return err;
    }
    atomic_set(&ready, 1);
    atomic_set(&restart, 1);
    k_work_reschedule(&profile_work, K_MSEC(1000));
    return 0;
}

SYS_INIT(diagnostics_init, APPLICATION, CONFIG_APPLICATION_INIT_PRIORITY);
