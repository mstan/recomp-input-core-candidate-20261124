#ifndef RECOMP_INPUT_H
#define RECOMP_INPUT_H
#include <stddef.h>
#include <stdint.h>
#ifdef __cplusplus
extern "C" {
#endif
#define RI_MAX_DEVICES 8
#define RI_MAX_PLAYERS 8
#define RI_MAX_ACTIONS 24
#define RI_MAX_AXES 6
#define RI_MAX_KEYS 512
#define RI_SOURCE_NONE 0u
#define RI_SOURCE_KEYBOARD 1u
#define RI_SOURCE_GAMEPAD 2u
/* Same encoding as recomp_launcher.h; zero means unbound. */
#define RI_BUTTON(code) (1u+(uint32_t)(code))
#define RI_AXIS(code,positive) (100u+2u*(uint32_t)(code)+((positive)?1u:0u))
#define RI_BUTTON_COMBO(mask) (1000u+(uint32_t)(mask))
typedef struct ri_context ri_context;
typedef struct {
 uint32_t id,buttons;
 char name[64],guid[40],physical_key[128];
 int16_t axes[RI_MAX_AXES];
 int has_gyro;
 float gyro_z;
} ri_device;
typedef struct {
 unsigned sources,deadzone_pct;
 uint32_t pad_binding[RI_MAX_ACTIONS];
 uint16_t key_binding[RI_MAX_ACTIONS];
 char guid[40],physical_key[128],name[64];
 int name_custom;
} ri_player;
/* Called before explicit configuration edits publish. Return zero to reject
 * without changing the live configuration. Snapshot/restore is caller-owned;
 * the service never chooses files or writes to disk. Do not reenter. */
typedef int (*ri_persist_fn)(void *user,const ri_player *players,size_t count);
ri_context *ri_create(unsigned players,unsigned actions,ri_persist_fn persist,void *user);
void ri_destroy(ri_context *context);
/* Backend snapshots use SDL standard button/axis indices. Backend owns event
 * pumping, controller handles, sensor activation and their lifecycle. */
void ri_update(ri_context *context,const ri_device *devices,size_t count,const uint8_t *keys,size_t key_count);
size_t ri_devices(const ri_context *context,ri_device *out,size_t capacity);
int ri_device_label(const ri_context *context,uint32_t id,char *out,size_t capacity);
int ri_assign(ri_context *context,unsigned player,unsigned sources,uint32_t device_id);
int ri_configure(ri_context *context,unsigned player,const ri_player *settings);
int ri_get_player(const ri_context *context,unsigned player,ri_player *settings);
int ri_restore(ri_context *context,const ri_player *players,size_t count);
int ri_connected(const ri_context *context,unsigned player);
const char *ri_player_label(const ri_context *context,unsigned player);
uint32_t ri_sample(const ri_context *context,unsigned player);
void ri_gameplay_blocked(ri_context *context,int blocked);
int ri_at_rest(const ri_context *context,unsigned player);
/* Begin while controls may be held; capture waits for full release. Returns
 * zero until the selected controller produces a new button/axis binding. */
int ri_capture_begin(ri_context *context,unsigned player);
uint32_t ri_capture_poll(ri_context *context,unsigned player);
#ifdef __cplusplus
}
#endif
#endif
