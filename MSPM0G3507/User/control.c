#include "control.h"
#include "trace.h"
#include "motor.h"

/* ═══════════════════════════════════════════════════════════════════════════
 *  Fixed-point conventions (Cortex-M0+ — no hardware FPU)
 *
 *  Speed:      Q8.8  (mm/s × 256)
 *  PID gains:  Q8.8  (dimensionless × 256)
 *  Duty:       raw PWM counts 0..PWM_DUTY_MAX, accumulated in Q8.8 internally
 *
 *  All arithmetic uses int32_t multiply + shift; zero soft-float calls.
 * ═══════════════════════════════════════════════════════════════════════════ */

/* ── PID period ── */
#define PID_PERIOD_MS   20

/* ── Speed conversion ──
   speed = counter / MOTOR_BIANMAQI * π * MOTOR_WHEEL_D * 1000 / PID_PERIOD_MS
         = counter * (π * 44 * 50) / 260
         ≈ counter * 26.5827
   Q8.8:  speed_Q8_8 = counter * 26.5827 * 256 ≈ counter * 6805                */
#define SPEED_FACTOR_Q8_8   6805

/* ── Open‑loop constants (raw PWM units) ── */
#define DUTY_MAX         4000
#define DUTY_STRAIGHT    1300
#define DUTY_GAIN         300

/* ── Closed‑loop constants (Q8.8) ── */
#define BASE_SPEED_Q8_8     25600    /* 100.0 mm/s × 256                       */
#define SPEED_GAIN_Q8_8      7680    /*  30.0 × 256                            */

/* ── PID gains (Q8.8 — tune these to match original kp=0.5, ki=0.4) ── */
#define KP_Q8_8   128               /* 0.5 × 256                              */
#define KI_Q8_8   102               /* 0.4 × 256                              */

#define PWM_DUTY_MAX_Q8_8  ((int32_t)PWM_DUTY_MAX << 8)   /* 4000 × 256 = 1 024 000 */

/* ═══════════════════════════════════════════════════════════════════════════
 *  PID (incremental, fixed‑point)
 * ═══════════════════════════════════════════════════════════════════════════ */
typedef struct {
    int32_t kp;          /* × 256                                            */
    int32_t ki;          /* × 256                                            */
    int32_t last_error;  /* Q8.8                                             */
} pid_t;

static pid_t pid_L;
static pid_t pid_R;

/* Accumulated duty (Q8.8) — the "integral" of the incremental PID.
 * Clamped to [0, PWM_DUTY_MAX_Q8_8] after each update.                     */
static int32_t duty_L_Q8_8;
static int32_t duty_R_Q8_8;

static void pid_reset(pid_t *pid)
{
    pid->last_error = 0;
}

static void pid_init(pid_t *pid, int32_t kp, int32_t ki)
{
    pid->kp = kp;
    pid->ki = ki;
    pid_reset(pid);
}

/* ── Incremental PID step ──────────────────────────────────────────────────
 * Returns Δduty in Q8.8.  Caller accumulates into duty_L_Q8_8 / duty_R_Q8_8
 * and clamps to [0, PWM_DUTY_MAX_Q8_8].
 *
 *   Δout = kp * (e[k] - e[k-1])  +  ki * e[k]
 *
 * All terms Q8.8; products are Q16.16 → arithmetic right-shift 8 → Q8.8.   */
static int32_t pid_update(pid_t *pid, int32_t target, int32_t actual)
{
    int32_t error       = target - actual;                     /* Q8.8       */
    int32_t delta_error = error - pid->last_error;             /* Q8.8       */

    /* kp (×256) × delta_error (Q8.8) → Q16.16
     * ki (×256) × error       (Q8.8) → Q16.16                              */
    int32_t delta = pid->kp * delta_error;                     /* Q16.16     */
    delta        += pid->ki * error;                           /* Q16.16     */

    pid->last_error = error;

    return delta >> 8;                                        /* → Q8.8     */
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  Mode management (volatile — read by ISR, written by main loop)
 * ═══════════════════════════════════════════════════════════════════════════ */
static volatile control_mode_t g_mode = CONTROL_OPEN_LOOP;

void control_set_mode(control_mode_t m)
{
    g_mode = m;                         /* atomic on M0+ for uint32_t-sized enum */
    if (m == CONTROL_CLOSED_LOOP) {
        /* Reset PID state so we start from the current duty, not stale history */
        pid_reset(&pid_L);
        pid_reset(&pid_R);
        duty_L_Q8_8 = (int32_t)DUTY_STRAIGHT << 8;
        duty_R_Q8_8 = (int32_t)DUTY_STRAIGHT << 8;
    }
}

control_mode_t control_get_mode(void)
{
    return (control_mode_t)g_mode;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  control_init
 * ═══════════════════════════════════════════════════════════════════════════ */
void control_init(void)
{
    pid_init(&pid_L, KP_Q8_8, KI_Q8_8);
    pid_init(&pid_R, KP_Q8_8, KI_Q8_8);
    duty_L_Q8_8 = 0;
    duty_R_Q8_8 = 0;
    g_mode = CONTROL_OPEN_LOOP;
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  control_update  — called every PID_PERIOD_MS from timer ISR
 *
 *  This is the ONLY function that writes to motor PWM / direction registers.
 *  All arithmetic is integer; worst-case path ≈ 8 multiplies, zero divides.
 * ═══════════════════════════════════════════════════════════════════════════ */
void control_update(void)
{
    /* ── 1. Read sensors ───────────────────────────────────────────────── */
    trace_read();

    int8_t  error  = trace_get_error();      /* -10 .. +10                   */
    uint8_t active = trace_get_active();     /*  0 ..  4                     */
    bool    inner  = trace_inner_both_line();/* both inner sensors on line   */

    /* ── 2. Special states (both modes) ─────────────────────────────────── */
    if (active == 0) {                       /* cross / end marker → stop    */
        motor_set_direction(MOTOR_L, MOTOR_STOP);
        motor_set_direction(MOTOR_R, MOTOR_STOP);
        motor_set_duty(MOTOR_L, 0);
        motor_set_duty(MOTOR_R, 0);
        /* Reset PID integrator so it doesn't wind up while stopped          */
        pid_reset(&pid_L);
        pid_reset(&pid_R);
        duty_L_Q8_8 = 0;
        duty_R_Q8_8 = 0;
        return;
    }

    motor_set_direction(MOTOR_L, MOTOR_FORWARD);
    motor_set_direction(MOTOR_R, MOTOR_FORWARD);

    if (inner || active == 4) {              /* inner‑on‑line OR lost line   */
        if (g_mode == CONTROL_OPEN_LOOP) {
            motor_set_duty(MOTOR_L, DUTY_STRAIGHT + 20);
            motor_set_duty(MOTOR_R, DUTY_STRAIGHT);
        } else {
            /* Closed‑loop: track base speed; let PID maintain it            */
            int32_t speed_L = motor_read_encoder(MOTOR_L);
            int32_t speed_R = motor_read_encoder(MOTOR_R);

            int32_t delta_L = pid_update(&pid_L, BASE_SPEED_Q8_8, speed_L);
            int32_t delta_R = pid_update(&pid_R, BASE_SPEED_Q8_8, speed_R);

            duty_L_Q8_8 += delta_L;
            duty_R_Q8_8 += delta_R;

            /* Clamp Q8.8 duty accumulator                                  */
            if (duty_L_Q8_8 < 0)               duty_L_Q8_8 = 0;
            if (duty_L_Q8_8 > PWM_DUTY_MAX_Q8_8) duty_L_Q8_8 = PWM_DUTY_MAX_Q8_8;
            if (duty_R_Q8_8 < 0)               duty_R_Q8_8 = 0;
            if (duty_R_Q8_8 > PWM_DUTY_MAX_Q8_8) duty_R_Q8_8 = PWM_DUTY_MAX_Q8_8;

            motor_set_duty(MOTOR_L, (uint32_t)(duty_L_Q8_8 >> 8));
            motor_set_duty(MOTOR_R, (uint32_t)(duty_R_Q8_8 >> 8));
        }
        return;
    }

    /* ── 3. Normal line‑following ──────────────────────────────────────── */
    if (g_mode == CONTROL_OPEN_LOOP) {
        /* duty = straight_base ± error × gain                              */
        int32_t duty_L = (int32_t)DUTY_STRAIGHT - (int32_t)error * DUTY_GAIN;
        int32_t duty_R = (int32_t)DUTY_STRAIGHT + (int32_t)error * DUTY_GAIN;

        motor_set_duty(MOTOR_L, limit_duty(duty_L));
        motor_set_duty(MOTOR_R, limit_duty(duty_R));
    } else {
        /* Closed‑loop: sensor error → target speed → PID → duty            */
        int32_t target_L = BASE_SPEED_Q8_8 - (int32_t)error * SPEED_GAIN_Q8_8;
        int32_t target_R = BASE_SPEED_Q8_8 + (int32_t)error * SPEED_GAIN_Q8_8;

        int32_t speed_L = motor_read_encoder(MOTOR_L);
        int32_t speed_R = motor_read_encoder(MOTOR_R);

        int32_t delta_L = pid_update(&pid_L, target_L, speed_L);
        int32_t delta_R = pid_update(&pid_R, target_R, speed_R);

        duty_L_Q8_8 += delta_L;
        duty_R_Q8_8 += delta_R;

        if (duty_L_Q8_8 < 0)               duty_L_Q8_8 = 0;
        if (duty_L_Q8_8 > PWM_DUTY_MAX_Q8_8) duty_L_Q8_8 = PWM_DUTY_MAX_Q8_8;
        if (duty_R_Q8_8 < 0)               duty_R_Q8_8 = 0;
        if (duty_R_Q8_8 > PWM_DUTY_MAX_Q8_8) duty_R_Q8_8 = PWM_DUTY_MAX_Q8_8;

        motor_set_duty(MOTOR_L, (uint32_t)(duty_L_Q8_8 >> 8));
        motor_set_duty(MOTOR_R, (uint32_t)(duty_R_Q8_8 >> 8));
    }
}

/* ═══════════════════════════════════════════════════════════════════════════
 *  PID_INST_IRQHandler  — timer ISR entry (name must match vector table)
 *
 *  Before:  trace_motor() + 2×calculate_speed() + 2×motor_PID()
 *           ≈ 200–500 μs (soft‑float on M0+)
 *  After:   control_update() only — all integer, ≈ 15–40 μs
 * ═══════════════════════════════════════════════════════════════════════════ */
void PID_INST_IRQHandler(void)
{
    switch (DL_Timer_getPendingInterrupt(PID_INST))
    {
        case DL_TIMER_IIDX_LOAD:
            control_update();
            break;
        default:
            break;
    }
}
