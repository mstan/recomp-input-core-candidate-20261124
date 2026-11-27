/* SPDX-License-Identifier: MIT */
#ifndef RECOMP_INPUT_EFFECTS_H
#define RECOMP_INPUT_EFFECTS_H
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
/* Existing rounding/deadband profiles are explicit, rather than silently
 * normalizing the observations of different native controller adapters. */
typedef enum {
    RI_AXIS_U8_RESCALED_127,
    RI_AXIS_U8_CLIPPED_256
} ri_axis_u8_profile;
uint8_t ri_axis_u8(int32_t value, unsigned deadzone_pct, ri_axis_u8_profile profile);
int32_t ri_axis_half(int32_t magnitude, unsigned deadzone_pct);

typedef struct ri_haptics ri_haptics;
typedef enum { RI_HAPTIC_HELD, RI_HAPTIC_FRAME32, RI_HAPTIC_MILLISECONDS } ri_haptic_clock;
typedef struct {
    uint32_t effect_ms;
    uint32_t refresh_ms; /* zero renews an active effect on every tick */
} ri_haptic_profile;
typedef void (*ri_haptic_emit)(void *user, uint32_t device, uint16_t low,
                               uint16_t high, uint32_t duration_ms);
/* No threads, locks, SDL handles or device discovery. Serialize a context on
 * the existing input owner/lock. Device zero means no route; other IDs are
 * backend-owned stable identities until release_device, before handle close.
 * Two independent sources per player compose by maximum per motor. */
ri_haptics *ri_haptics_create(unsigned players, ri_haptic_profile profile);
void ri_haptics_destroy(ri_haptics *context);
void ri_haptics_set(ri_haptics *context, unsigned player, unsigned source,
                    uint16_t low, uint16_t high, ri_haptic_clock clock,
                    uint64_t stamp, uint64_t lifetime);
void ri_haptics_route(ri_haptics *context, unsigned player, uint32_t device);
void ri_haptics_values(const ri_haptics *context, unsigned player,
                       uint32_t frame, uint64_t milliseconds,
                       uint16_t *low, uint16_t *high);
void ri_haptics_tick(ri_haptics *context, uint32_t frame, uint64_t milliseconds,
                     ri_haptic_emit emit, void *user);
/* Stops a live effect once and forgets all routes to this device. Source
 * values stay with their player. Call before closing/reusing a backend ID. */
void ri_haptics_release_device(ri_haptics *context, uint32_t device,
                               ri_haptic_emit emit, void *user);
#ifdef __cplusplus
}
#endif
#endif
