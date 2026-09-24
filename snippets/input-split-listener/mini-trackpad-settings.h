#pragma once

// Right mini trackpad only. Ratio = MULTIPLIER / DIVISOR.
// Examples: 1/2 = 1,2; 1/3 = 1,3; 2/3 = 2,3.
#define MINI_TRACKPAD_SCROLL_MULTIPLIER 1
#define MINI_TRACKPAD_SCROLL_DIVISOR 2
#define MINI_TRACKPAD_CURSOR_MULTIPLIER 1
#define MINI_TRACKPAD_CURSOR_DIVISOR 2

// Existing Mouse Layer in config/keymap.keymap; also used by the left ball's AML.
#define MINI_TRACKPAD_MOUSE_LAYER 4

#if MINI_TRACKPAD_SCROLL_MULTIPLIER <= 0 || MINI_TRACKPAD_SCROLL_DIVISOR <= 0 || \
    MINI_TRACKPAD_CURSOR_MULTIPLIER <= 0 || MINI_TRACKPAD_CURSOR_DIVISOR <= 0
#error "Mini trackpad scale multipliers and divisors must be positive"
#endif
