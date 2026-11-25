/* Extracted policy from recomp-ui launcher_input/model and Genesis input_map/
 * gamepad. Backend SDL access is replaced by caller-owned standard snapshots. */
#include "recomp_input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
struct ri_context {
 unsigned players,actions;ri_persist_fn persist;void *user;
 ri_player selected[RI_MAX_PLAYERS];
 ri_device devices[RI_MAX_DEVICES];size_t device_count;
 uint32_t live_id[RI_MAX_PLAYERS];unsigned connected[RI_MAX_PLAYERS];
 uint8_t keys[RI_MAX_KEYS],capture[RI_MAX_PLAYERS];int blocked;
};
static void copy_text(char *out,size_t cap,const char *in){
 if(!out||!cap)return;size_t n=in?strlen(in):0;if(n>=cap)n=cap-1;
 if(n)memcpy(out,in,n);out[n]=0;
}
static int terminated(const char *s,size_t n){return memchr(s,0,n)!=NULL;}
static const ri_device *find_id(const ri_context *c,uint32_t id){
 for(size_t i=0;i<c->device_count;i++)if(c->devices[i].id==id)return &c->devices[i];return NULL;
}
static int claimed(const ri_context *c,unsigned player,uint32_t id){
 for(unsigned i=0;i<c->players;i++)if(i!=player&&c->connected[i]&&c->live_id[i]==id)return 1;return 0;
}
static int identity_matches(const ri_player *p,const ri_device *d){
 if(p->physical_key[0])return !strcmp(p->physical_key,d->physical_key);
 return !strcmp(p->guid,d->guid);
}
/* Keep live instance selection; reconnect by physical key when supplied, or
 * only a unique unclaimed GUID. A GUID describes a model, not physical identity. */
static void reconnect(ri_context *c){
 for(unsigned p=0;p<c->players;p++){
  const ri_device *d=c->connected[p]?find_id(c,c->live_id[p]):NULL;
  if(!(c->selected[p].sources&RI_SOURCE_GAMEPAD)){c->connected[p]=0;continue;}
  if(d&&identity_matches(&c->selected[p],d))continue;
  c->connected[p]=0;c->capture[p]=0;
  if(!c->selected[p].physical_key[0]&&!c->selected[p].guid[0])continue;
  const ri_device *found=NULL;unsigned matches=0;
  for(size_t i=0;i<c->device_count;i++){
   d=&c->devices[i];if(claimed(c,p,d->id))continue;
   int match=identity_matches(&c->selected[p],d);
   if(match){found=d;matches++;}
  }
  if(matches==1){c->live_id[p]=found->id;c->connected[p]=1;
   if(!c->selected[p].name_custom)copy_text(c->selected[p].name,sizeof(c->selected[p].name),found->name[0]?found->name:"Gamepad");}
 }
}
static int valid_player(const ri_context *c,const ri_player *p){
 if(p->sources>3u||p->deadzone_pct>100u||!terminated(p->guid,sizeof p->guid)||
  !terminated(p->physical_key,sizeof p->physical_key)||!terminated(p->name,sizeof p->name))return 0;
 for(unsigned a=0;a<c->actions;a++){
  uint32_t b=p->pad_binding[a];
  if(p->key_binding[a]>=RI_MAX_KEYS)return 0;
  if(b>32u&&b<100u)return 0;
  if(b>=100u&&b<1000u&&(b-100u)/2u>=RI_MAX_AXES)return 0;
 }
 return 1;
}
static int publish(ri_context *c,const ri_player *players){
 if(c->persist&&!c->persist(c->user,players,c->players))return 0;
 memcpy(c->selected,players,c->players*sizeof(*players));return 1;
}
ri_context *ri_create(unsigned players,unsigned actions,ri_persist_fn fn,void *user){
 if(!players||players>RI_MAX_PLAYERS||!actions||actions>RI_MAX_ACTIONS)return NULL;
 ri_context *c=(ri_context *)calloc(1,sizeof(*c));if(!c)return NULL;
 c->players=players;c->actions=actions;c->persist=fn;c->user=user;
 for(unsigned p=0;p<players;p++)c->selected[p].deadzone_pct=25;
 return c;
}
void ri_destroy(ri_context *c){free(c);}
void ri_update(ri_context *c,const ri_device *devices,size_t n,const uint8_t *keys,size_t k){
 if(!c)return;c->device_count=0;
 if(devices)for(size_t i=0;i<n&&c->device_count<RI_MAX_DEVICES;i++){
  const ri_device *d=&devices[i];
  if(!terminated(d->name,sizeof d->name)||!terminated(d->guid,sizeof d->guid)||
   !terminated(d->physical_key,sizeof d->physical_key)||find_id(c,d->id))continue;
  c->devices[c->device_count++]=*d;
 }
 memset(c->keys,0,sizeof c->keys);if(keys){if(k>RI_MAX_KEYS)k=RI_MAX_KEYS;memcpy(c->keys,keys,k);}
 reconnect(c);
}
size_t ri_devices(const ri_context *c,ri_device *out,size_t n){
 if(!c||!out)return 0;if(n>c->device_count)n=c->device_count;memcpy(out,c->devices,n*sizeof(*out));return n;
}
int ri_device_label(const ri_context *c,uint32_t id,char *out,size_t cap){
 if(!c||!out||!cap)return 0;const ri_device *d=find_id(c,id);if(!d){out[0]=0;return 0;}
 const char *name=d->name[0]?d->name:"Gamepad";unsigned copies=0,index=0;
 for(size_t i=0;i<c->device_count;i++)if(!strcmp(c->devices[i].name,d->name)){
  copies++;if(c->devices[i].id==id)index=copies;
 }
 if(copies>1)snprintf(out,cap,"%s (%u)",name,index);else copy_text(out,cap,name);return 1;
}
int ri_assign(ri_context *c,unsigned p,unsigned sources,uint32_t id){
 if(!c||p>=c->players||sources>3u)return 0;
 const ri_device *d=(sources&RI_SOURCE_GAMEPAD)?find_id(c,id):NULL;
 if((sources&RI_SOURCE_GAMEPAD)&&!d)return 0;
 ri_player next[RI_MAX_PLAYERS];memcpy(next,c->selected,sizeof next);next[p].sources=sources;
 if(d){
  for(unsigned o=0;o<c->players;o++)if(o!=p&&c->connected[o]&&c->live_id[o]==id){
   next[o].sources&=~RI_SOURCE_GAMEPAD;next[o].guid[0]=next[o].physical_key[0]=next[o].name[0]=0;
  }
  int keep_name=next[p].name_custom&&identity_matches(&next[p],d);
  copy_text(next[p].guid,sizeof next[p].guid,d->guid);copy_text(next[p].physical_key,sizeof next[p].physical_key,d->physical_key);
  if(!keep_name){copy_text(next[p].name,sizeof next[p].name,d->name[0]?d->name:"Gamepad");next[p].name_custom=0;}
 }else next[p].guid[0]=next[p].physical_key[0]=next[p].name[0]=0;
 if(!publish(c,next))return 0;
 for(unsigned o=0;o<c->players;o++)if(!(next[o].sources&RI_SOURCE_GAMEPAD))c->connected[o]=0;
 c->connected[p]=d!=NULL;if(d)c->live_id[p]=id;c->capture[p]=0;return 1;
}
int ri_configure(ri_context *c,unsigned p,const ri_player *settings){
 if(!c||p>=c->players||!settings||!valid_player(c,settings))return 0;
 int same_identity=!strcmp(c->selected[p].guid,settings->guid)&&
  !strcmp(c->selected[p].physical_key,settings->physical_key);
 ri_player next[RI_MAX_PLAYERS];memcpy(next,c->selected,sizeof next);next[p]=*settings;
 if(!publish(c,next))return 0;
 if(!same_identity||!(settings->sources&RI_SOURCE_GAMEPAD))c->connected[p]=0;
 c->capture[p]=0;reconnect(c);return 1;
}
int ri_get_player(const ri_context *c,unsigned p,ri_player *settings){
 if(!c||p>=c->players||!settings)return 0;*settings=c->selected[p];return 1;
}
int ri_restore(ri_context *c,const ri_player *players,size_t count){
 if(!c||!players||count!=c->players)return 0;
 for(unsigned p=0;p<c->players;p++)if(!valid_player(c,&players[p]))return 0;
 memcpy(c->selected,players,count*sizeof(*players));memset(c->connected,0,sizeof c->connected);
 memset(c->capture,0,sizeof c->capture);reconnect(c);return 1;
}
int ri_connected(const ri_context *c,unsigned p){
 return c&&p<c->players&&((c->selected[p].sources&RI_SOURCE_KEYBOARD)||c->connected[p]);
}
const char *ri_player_label(const ri_context *c,unsigned p){
 if(!c||p>=c->players)return "None";const ri_player *s=&c->selected[p];
 if(s->sources&RI_SOURCE_GAMEPAD){if(s->name[0])return s->name;return c->connected[p]?"Gamepad":"Disconnected gamepad";}
 return s->sources&RI_SOURCE_KEYBOARD?"Keyboard":"None";
}
/* Genesis player_deadzone and bind_held: percentage clamps, strict axis tests. */
int ri_binding_held(const ri_device *d,uint32_t b,int dz){
 if(!d||!b)return 0;
 if(b>=1000u){uint32_t mask=b-1000u;return mask&&(d->buttons&mask)==mask;}
 if(b>=100u){unsigned a=(b-100u)/2u;if(a>=RI_MAX_AXES)return 0;int v=d->axes[a];return (b-100u)&1u?v>dz:v< -dz;}
 return b<=32u&&(d->buttons&(1u<<(b-1u)))!=0;
}
uint32_t ri_sample(const ri_context *c,unsigned p){
 if(!c||p>=c->players||c->blocked)return 0;
 const ri_player *s=&c->selected[p];uint32_t mask=0;
 const ri_device *d=c->connected[p]?find_id(c,c->live_id[p]):NULL;
 const int dz=(int)(s->deadzone_pct*32767u/100u);
 for(unsigned a=0;a<c->actions;a++){
  if((s->sources&RI_SOURCE_KEYBOARD)&&s->key_binding[a]&&c->keys[s->key_binding[a]])mask|=1u<<a;
  if((s->sources&RI_SOURCE_GAMEPAD)&&ri_binding_held(d,s->pad_binding[a],dz))mask|=1u<<a;
 }
 return mask;
}
void ri_gameplay_blocked(ri_context *c,int blocked){if(c)c->blocked=!!blocked;}
/* recomp-ui's exact release band; disconnected devices do not block capture. */
int ri_at_rest(const ri_context *c,unsigned p){
 if(!c||p>=c->players)return 1;const ri_device *d=c->connected[p]?find_id(c,c->live_id[p]):NULL;
 if(!d)return 1;if(d->buttons)return 0;
 for(unsigned a=0;a<RI_MAX_AXES;a++)if(d->axes[a]<=-8000||d->axes[a]>=8000)return 0;return 1;
}
int ri_capture_begin(ri_context *c,unsigned p){if(!c||p>=c->players)return 0;c->capture[p]=1;return 1;}
uint32_t ri_capture_poll(ri_context *c,unsigned p){
 if(!c||p>=c->players||!c->capture[p])return 0;
 if(c->capture[p]==1){if(ri_at_rest(c,p))c->capture[p]=2;return 0;}
 const ri_device *d=c->connected[p]?find_id(c,c->live_id[p]):NULL;if(!d)return 0;
 for(unsigned b=0;b<32;b++)if(d->buttons&(1u<<b)){c->capture[p]=0;return RI_BUTTON(b);}
 for(unsigned a=0;a<RI_MAX_AXES;a++)if(d->axes[a]<-20000||d->axes[a]>20000){c->capture[p]=0;return RI_AXIS(a,d->axes[a]>0);}
 return 0;
}
