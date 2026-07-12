#include "key.h"

/*
 * GPIOB interrupt is no longer used.
 * Encoders now use QEI hardware mode (TIMG5 + TIMG6),
 * which has built-in digital glitch filtering to reject motor PWM noise.
 * Encoder counting is handled by the QEI timer hardware directly.
 */
