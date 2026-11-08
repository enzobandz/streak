/* Time-based tweening with easing curves. */
#include "kernel.h"

int g_anim_speed_pct = 100;

void anim_start(anim_t *a, float from, float to, uint32_t dur, int ease) {
    a->start = ticks_ms();
    a->dur = g_anim_speed_pct ? dur * 100 / g_anim_speed_pct : 0;
    a->from = from;
    a->to = to;
    a->ease = ease;
    a->active = a->dur > 0;
}

static float ease(int kind, float t) {
    float u = 1 - t;
    switch (kind) {
    case EASE_OUT_QUINT: return 1 - u * u * u * u * u;
    case EASE_OUT_BACK: { float c = 1.4f; return 1 + (c + 1) * (t - 1) * (t - 1) * (t - 1) + c * (t - 1) * (t - 1); }
    case EASE_IN_OUT: return t < 0.5f ? 4 * t * t * t : 1 - 4 * u * u * u;
    case EASE_SPRING: {
        /* damped oscillation 1 - e^(-6t) cos(10.5t); (1-t)^4 stands in for the decay */
        float e = u * u * u * u, x = 10.5f * t, c;
        __asm__("fcos" : "=t"(c) : "0"(x));
        return 1 - e * c;
    }
    default: return 1 - u * u * u;
    }
}

float anim_value(anim_t *a, uint32_t now) {
    if (!a->active) return a->to;
    uint32_t el = now - a->start;
    if (el >= a->dur) { a->active = 0; return a->to; }
    float t = (float)el / a->dur;
    return a->from + (a->to - a->from) * ease(a->ease, t);
}
