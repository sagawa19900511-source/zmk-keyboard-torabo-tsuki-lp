#pragma once

// Right mini trackpad only. Ratio = MULTIPLIER / DIVISOR.
// Examples: 1/2 = 1,2; 1/3 = 1,3; 2/3 = 2,3.
#define MINI_TRACKPAD_SCROLL_MULTIPLIER 1
#define MINI_TRACKPAD_SCROLL_DIVISOR 2
#define MINI_TRACKPAD_CURSOR_MULTIPLIER 1
#define MINI_TRACKPAD_CURSOR_DIVISOR 1

// Dedicated right-pad layer. Left ball AML remains on Layer 4.
#define MINI_TRACKPAD_MOUSE_LAYER 6

#if MINI_TRACKPAD_SCROLL_MULTIPLIER <= 0 || MINI_TRACKPAD_SCROLL_DIVISOR <= 0 || \
    MINI_TRACKPAD_CURSOR_MULTIPLIER <= 0 || MINI_TRACKPAD_CURSOR_DIVISOR <= 0
#error "Mini trackpad scale multipliers and divisors must be positive"
#endif

// Central-only inertia. Direct scroll and inertia share the post-scaler 1/2 units.
// The optional mini-trackpad-inertia-off snippet overrides enabled to 0.
#define MINI_TRACKPAD_INERTIA_ENABLED 1
#define MINI_TRACKPAD_INERTIA_TICK_MS 8
#define MINI_TRACKPAD_INERTIA_RELEASE_MS 40
#define MINI_TRACKPAD_INERTIA_DECAY_PERMILLE 950
#define MINI_TRACKPAD_INERTIA_BLEND_PERMILLE 500
#define MINI_TRACKPAD_INERTIA_MIN_SAMPLES 3
#define MINI_TRACKPAD_INERTIA_START_MILLI 1000
#define MINI_TRACKPAD_INERTIA_STOP_MILLI 250
#define MINI_TRACKPAD_INERTIA_LIMIT_MILLI 64000
#define MINI_TRACKPAD_INERTIA_MAX_MS 1500
