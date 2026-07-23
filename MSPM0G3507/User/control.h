#ifndef CONTROL_H
#define CONTROL_H

#include <stdint.h>
#include <stdbool.h>

/* ── Control mode ── */
typedef enum {
    CONTROL_OPEN_LOOP   = 0,
    CONTROL_CLOSED_LOOP = 1
} control_mode_t;

/* ── Public API ── */
void        control_init(void);
void        control_update(void);            /* called from PID_INST_IRQHandler      */
void        control_set_mode(control_mode_t m);
control_mode_t control_get_mode(void);

#endif
