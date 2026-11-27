/* SPDX-License-Identifier: MIT */
#include "recomp_input_effects.h"
#include "recomp_input.h"
#include <stdlib.h>

uint8_t ri_axis_u8(int32_t value, unsigned pct, ri_axis_u8_profile profile) {
    int32_t threshold;
    if (pct > 100) pct = 100;
    threshold = (int32_t)(32767u * pct / 100u);
    if (profile == RI_AXIS_U8_CLIPPED_256) {
        int32_t result;
        if (threshold < 1) threshold = 1;
        if (value > -threshold && value < threshold) value = 0;
        result = 128 + value / 256;
        return (uint8_t)(result < 0 ? 0 : result > 255 ? 255 : result);
    } else {
        uint32_t magnitude = value < 0 ? (uint32_t)(-(int64_t)value) : (uint32_t)value;
        if (magnitude > 32767u) magnitude = 32767u;
        if (magnitude <= (uint32_t)threshold || threshold == 32767) return 128;
        magnitude = (magnitude - (uint32_t)threshold) * 127u / (32767u - (uint32_t)threshold);
        return (uint8_t)(value < 0 ? 127u - magnitude : 128u + magnitude);
    }
}

int32_t ri_axis_half(int32_t magnitude, unsigned pct) {
    int32_t threshold;
    if (pct > 90) pct = 90;
    threshold = (int32_t)(pct * 32768u / 100u);
    if (!threshold) return magnitude;
    if (magnitude <= threshold) return 0;
    return (int32_t)((int64_t)(magnitude - threshold) * 32768 / (32768 - threshold));
}

typedef struct {
    uint16_t low, high;
    ri_haptic_clock clock;
    uint64_t stamp, lifetime;
} source;
typedef struct {
    uint32_t device;
    uint16_t low, high;
    uint64_t refresh;
} output;
struct ri_haptics {
    unsigned players;
    ri_haptic_profile profile;
    source sources[RI_MAX_PLAYERS][2];
    uint32_t routes[RI_MAX_PLAYERS];
    output outputs[RI_MAX_PLAYERS];
};

ri_haptics *ri_haptics_create(unsigned players, ri_haptic_profile profile) {
    ri_haptics *c;
    if (!players || players > RI_MAX_PLAYERS || !profile.effect_ms) return NULL;
    c = (ri_haptics *)calloc(1, sizeof(*c));
    if (c) { c->players = players; c->profile = profile; }
    return c;
}
void ri_haptics_destroy(ri_haptics *c) { free(c); }
void ri_haptics_set(ri_haptics *c, unsigned p, unsigned n, uint16_t low,
                    uint16_t high, ri_haptic_clock clock, uint64_t stamp, uint64_t lifetime) {
    source *s;
    if (!c || p >= c->players || n >= 2) return;
    s = &c->sources[p][n];
    s->low = low; s->high = high; s->clock = clock;
    s->stamp = stamp; s->lifetime = lifetime;
}
void ri_haptics_route(ri_haptics *c, unsigned p, uint32_t device) {
    if (c && p < c->players) c->routes[p] = device;
}
void ri_haptics_values(const ri_haptics *c, unsigned p, uint32_t frame,
                       uint64_t ms, uint16_t *low, uint16_t *high) {
    unsigned n;
    if (!low || !high) return;
    *low = *high = 0;
    if (!c || p >= c->players) return;
    for (n = 0; n < 2; ++n) {
        const source *s = &c->sources[p][n];
        if (s->clock == RI_HAPTIC_FRAME32 &&
            (uint32_t)(frame - (uint32_t)s->stamp) >= s->lifetime) continue;
        if (s->clock == RI_HAPTIC_MILLISECONDS && ms - s->stamp >= s->lifetime) continue;
        if (s->low > *low) *low = s->low;
        if (s->high > *high) *high = s->high;
    }
}
void ri_haptics_release_device(ri_haptics *c, uint32_t device,
                               ri_haptic_emit emit, void *user) {
    unsigned i;
    if (!c || !device) return;
    for (i = 0; i < c->players; ++i) {
        output *o = &c->outputs[i];
        if (o->device == device) {
            if ((o->low || o->high) && emit) emit(user, device, 0, 0, 0);
            o->device = 0; o->low = o->high = 0; o->refresh = 0;
        }
        if (c->routes[i] == device) c->routes[i] = 0;
    }
}
void ri_haptics_tick(ri_haptics *c, uint32_t frame, uint64_t ms,
                     ri_haptic_emit emit, void *user) {
    unsigned i, p;
    if (!c || !emit) return;
    /* Retire old routes before assigning output slots, including full-capacity
     * swaps. Device sharing merges once, so one player's neutral cannot stop
     * another player's active effect on the same physical controller. */
    for (i = 0; i < c->players; ++i) {
        output *o = &c->outputs[i];
        if (!o->device) continue;
        for (p = 0; p < c->players && c->routes[p] != o->device; ++p) {}
        if (p == c->players) {
            if (o->low || o->high) emit(user, o->device, 0, 0, 0);
            o->device = 0; o->low = o->high = 0; o->refresh = 0;
        }
    }
    for (p = 0; p < c->players; ++p) {
        unsigned empty = c->players;
        if (!c->routes[p]) continue;
        for (i = 0; i < c->players; ++i) {
            if (c->outputs[i].device == c->routes[p]) break;
            if (!c->outputs[i].device) empty = i;
        }
        if (i == c->players && empty < c->players) c->outputs[empty].device = c->routes[p];
    }
    for (i = 0; i < c->players; ++i) {
        output *o = &c->outputs[i];
        uint16_t low = 0, high = 0;
        int active;
        if (!o->device) continue;
        for (p = 0; p < c->players; ++p) if (c->routes[p] == o->device) {
            uint16_t l, h;
            ri_haptics_values(c, p, frame, ms, &l, &h);
            if (l > low) low = l;
            if (h > high) high = h;
        }
        active = low || high;
        if ((active && (!c->profile.refresh_ms || ms >= o->refresh || low != o->low || high != o->high)) ||
            (!active && (o->low || o->high))) {
            emit(user, o->device, low, high, active ? c->profile.effect_ms : 0);
            o->refresh = active ? ms + c->profile.refresh_ms : 0;
        }
        o->low = low; o->high = high;
    }
}
