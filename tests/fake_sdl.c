#include "fake_sdl.h"
#include <string.h>
static ri_device pads[RI_MAX_DEVICES];static size_t count;
void oracle_feed(const ri_device *d,size_t n){count=n;if(count>RI_MAX_DEVICES)count=RI_MAX_DEVICES;if(count)memcpy(pads,d,count*sizeof(*d));}
static const ri_device *find(uint32_t id){for(size_t i=0;i<count;i++)if(pads[i].id==id)return &pads[i];return NULL;}
int SDL_NumJoysticks(void){return (int)count;}
int SDL_IsGameController(int i){return i>=0&&(size_t)i<count;}
SDL_JoystickID SDL_JoystickGetDeviceInstanceID(int i){return (SDL_JoystickID)pads[i].id;}
const char *SDL_GameControllerNameForIndex(int i){return pads[i].name;}
SDL_JoystickGUID SDL_JoystickGetDeviceGUID(int i){SDL_JoystickGUID g;memcpy(g.text,pads[i].guid,sizeof g.text);return g;}
void SDL_JoystickGetGUIDString(SDL_JoystickGUID g,char *s,int n){if(n>0){size_t z=strlen(g.text);if(z>=(size_t)n)z=(size_t)n-1;memcpy(s,g.text,z);s[z]=0;}}
SDL_GameController *SDL_GameControllerOpen(int i){SDL_GameController *p=(SDL_GameController *)malloc(sizeof(*p));if(p)p->id=pads[i].id;return p;}
void SDL_GameControllerClose(SDL_GameController *p){free(p);}
char *SDL_GameControllerMapping(SDL_GameController *p){(void)p;return NULL;}
int SDL_GameControllerGetButton(SDL_GameController *p,SDL_GameControllerButton b){const ri_device *d=find(p->id);return d&&b>=0&&b<32&&(d->buttons&(1u<<b));}
int16_t SDL_GameControllerGetAxis(SDL_GameController *p,SDL_GameControllerAxis a){const ri_device *d=find(p->id);return d&&a>=0&&a<6?d->axes[a]:0;}
typedef struct {int kind,code,axis_dir;} GamepadBind;
enum {GP_BIND_NONE,GP_BIND_BUTTON,GP_BIND_AXIS};
static struct {struct {int deadzone_pct;} p[8];} g_input_map;
#include "genesis_bind_oracle.inc"
int oracle_binding(uint32_t id,uint32_t encoded,unsigned dz){
 SDL_GameController pad={id};GamepadBind bind={0,0,0};g_input_map.p[0].deadzone_pct=(int)dz;
 if(encoded>=100u){bind.kind=GP_BIND_AXIS;bind.code=(int)(encoded-100u)/2;bind.axis_dir=(encoded-100u)&1u?1:-1;}
 else if(encoded){bind.kind=GP_BIND_BUTTON;bind.code=(int)encoded-1;}
 return bind_held(&pad,&bind,player_deadzone(0));
}
