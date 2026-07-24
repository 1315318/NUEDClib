#ifndef CONTROL_H
#define CONTROL_H

#include <stdint.h>

/* ── Public API ── */
void control_init(void);
void control_update(void);            /* called from PID_INST_IRQHandler      */

/* ── Variant hooks (implemented by control_open.c OR control_closed.c) ── */
void control_variant_init(void);
void control_variant_reset(void);
void control_straight_duty(void);
void control_track_duty(int8_t error);

#endif
