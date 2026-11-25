#include "recomp_input.h"
#include "fake_sdl.h"
#include "oracle/launcher_input.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"%s:%d failed %s\n",__FILE__,__LINE__,#x);exit(1);}}while(0)
typedef struct {unsigned calls;int allow;ri_player saved[RI_MAX_PLAYERS];} Persistence;
static int persist(void *v,const ri_player *p,size_t n){Persistence *s=(Persistence *)v;s->calls++;if(!s->allow)return 0;memcpy(s->saved,p,n*sizeof(*p));return 1;}
static ri_device device(uint32_t id,const char *guid,const char *physical){
 ri_device d;memset(&d,0,sizeof d);d.id=id;snprintf(d.name,sizeof d.name,"Xbox Series Controller");
 snprintf(d.guid,sizeof d.guid,"%s",guid);snprintf(d.physical_key,sizeof d.physical_key,"%s",physical);return d;
}
static void update(ri_context *c,ri_device *d,size_t n){ri_update(c,d,n,NULL,0);oracle_feed(d,n);}
static void donor_boundaries(void){
 Persistence save={0,1,{{0}}};ri_context *c=ri_create(2,4,persist,&save);CHECK(c);
 ri_device d[2]={device(10,"model-guid","left"),device(11,"model-guid","right")};update(c,d,2);
 LauncherPad original[8];CHECK(launcher_input_poll(original,8,0)==2);
 ri_device actual[8];CHECK(ri_devices(c,actual,8)==2);
 CHECK(!strcmp(actual[0].name,original[0].name)&&!strcmp(actual[1].guid,original[1].guid));
 CHECK(ri_assign(c,0,RI_SOURCE_GAMEPAD,10));CHECK(ri_assign(c,1,RI_SOURCE_GAMEPAD,11));
 char label[80];CHECK(ri_device_label(c,11,label,sizeof label));CHECK(!strcmp(label,"Xbox Series Controller (2)"));
 const int values[]={-32768,-20001,-20000,-8001,-8000,-7999,0,7999,8000,8001,20000,20001,32767};
 const unsigned zones[]={0,25,50,100};unsigned observations=0;
 for(unsigned z=0;z<4;z++)for(unsigned axis=0;axis<6;axis++)for(unsigned sign=0;sign<2;sign++){
  ri_player p;CHECK(ri_get_player(c,0,&p));p.deadzone_pct=zones[z];p.pad_binding[0]=RI_AXIS(axis,sign);CHECK(ri_configure(c,0,&p));
  for(unsigned i=0;i<sizeof values/sizeof values[0];i++){
   memset(d[0].axes,0,sizeof d[0].axes);d[0].axes[axis]=(int16_t)values[i];update(c,d,2);
   CHECK((ri_sample(c,0)&1u)==(unsigned)oracle_binding(10,p.pad_binding[0],zones[z]));
   CHECK(ri_sample(c,1)==0);CHECK(ri_at_rest(c,0)==launcher_input_gamepad_at_rest(10));observations++;
  }
 }
 memset(d[0].axes,0,sizeof d[0].axes);
 ri_player p;CHECK(ri_get_player(c,0,&p));p.pad_binding[0]=RI_BUTTON(31);p.pad_binding[1]=RI_BUTTON_COMBO(3);CHECK(ri_configure(c,0,&p));
 d[0].buttons=0x80000001u;update(c,d,2);CHECK((ri_sample(c,0)&1u)==1);CHECK((ri_sample(c,0)&2u)==0);
 d[0].buttons|=2u;update(c,d,2);CHECK((ri_sample(c,0)&2u)==2u);
 CHECK(launcher_input_gamepad_button_mask(10)==d[0].buttons);
 unsigned calls=save.calls;for(unsigned i=0;i<10;i++){update(c,d,2);(void)ri_sample(c,0);}CHECK(save.calls==calls);
 ri_gameplay_blocked(c,1);CHECK(!ri_sample(c,0));ri_gameplay_blocked(c,0);CHECK(ri_sample(c,0));
 launcher_input_shutdown();ri_destroy(c);printf("%u donor boundary comparisons passed\n",observations);
}
static void ownership_and_hotplug(void){
 Persistence save={0,1,{{0}}};ri_context *c=ri_create(2,3,persist,&save);CHECK(c);
 ri_device d[2]={device(0,"same","left"),device(21,"same","right")};d[1].buttons=1;update(c,d,2);
 CHECK(ri_assign(c,0,RI_SOURCE_GAMEPAD,0));CHECK(ri_assign(c,1,RI_SOURCE_GAMEPAD,21));
 for(unsigned i=0;i<2;i++){ri_player p;CHECK(ri_get_player(c,i,&p));p.pad_binding[0]=RI_BUTTON(0);CHECK(ri_configure(c,i,&p));}
 CHECK(!ri_sample(c,0)&&ri_sample(c,1)==1);
 CHECK(ri_assign(c,0,RI_SOURCE_GAMEPAD,21));CHECK(ri_sample(c,0)==1&&!ri_sample(c,1));CHECK(!ri_connected(c,1));
 save.allow=0;CHECK(!ri_assign(c,1,RI_SOURCE_GAMEPAD,21));CHECK(ri_sample(c,0)==1&&!ri_connected(c,1));save.allow=1;
 ri_player p;CHECK(ri_get_player(c,0,&p));snprintf(p.name,sizeof p.name,"Living room controller");p.name_custom=1;CHECK(ri_configure(c,0,&p));
 update(c,d,1);CHECK(!ri_connected(c,0)&&!ri_sample(c,0));CHECK(!strcmp(ri_player_label(c,0),"Living room controller"));
 d[1].id=99;snprintf(d[1].name,sizeof d[1].name,"New driver name");update(c,d,2);CHECK(ri_connected(c,0)&&ri_sample(c,0)==1);
 CHECK(!strcmp(ri_player_label(c,0),"Living room controller"));
 /* A held control cannot immediately bind itself when capture opens. */
 CHECK(ri_capture_begin(c,0));CHECK(!ri_capture_poll(c,0));d[1].buttons=0;update(c,d,2);CHECK(!ri_capture_poll(c,0));
 d[1].axes[0]=20000;update(c,d,2);CHECK(!ri_capture_poll(c,0));d[1].axes[0]=20001;update(c,d,2);CHECK(ri_capture_poll(c,0)==RI_AXIS(0,1));
 /* Keyboard state belongs only to explicitly selected keyboard sources. */
 CHECK(ri_assign(c,1,RI_SOURCE_KEYBOARD,0));CHECK(ri_get_player(c,1,&p));p.key_binding[2]=12;CHECK(ri_configure(c,1,&p));
 uint8_t keys[RI_MAX_KEYS]={0};keys[12]=1;ri_update(c,d,2,keys,sizeof keys);CHECK(ri_sample(c,1)==4);CHECK(!(ri_sample(c,0)&4));
 ri_context *restored=ri_create(2,3,NULL,NULL);CHECK(restored);ri_update(restored,d,2,keys,sizeof keys);
 ri_player saved[2];CHECK(ri_get_player(c,0,&saved[0]));CHECK(ri_get_player(c,1,&saved[1]));CHECK(ri_restore(restored,saved,2));
 CHECK(ri_connected(restored,0)&&ri_sample(restored,1)==4);ri_destroy(restored);
 /* Duplicate enumeration entries represent one physical device. */
 ri_device duplicate[3]={d[0],d[1],d[1]};ri_update(c,duplicate,3,NULL,0);ri_device listed[8];CHECK(ri_devices(c,listed,8)==2);
 ri_destroy(c);
}
static void ambiguous_and_invalid(void){
 ri_context *c=ri_create(1,2,NULL,NULL);CHECK(c);ri_player saved={0};saved.sources=RI_SOURCE_GAMEPAD;saved.deadzone_pct=25;
 snprintf(saved.guid,sizeof saved.guid,"same");snprintf(saved.name,sizeof saved.name,"Saved controller");saved.pad_binding[0]=RI_BUTTON(0);
 ri_device d[2]={device(4,"same",""),device(8,"same","")};d[1].buttons=1;ri_update(c,d,2,NULL,0);CHECK(ri_restore(c,&saved,1));
 CHECK(!ri_connected(c,0)&&!ri_sample(c,0));CHECK(!strcmp(ri_player_label(c,0),"Saved controller"));
 ri_update(c,&d[1],1,NULL,0);CHECK(ri_connected(c,0)&&ri_sample(c,0)==1);
 /* Same live id reused for a different model cannot inherit ownership. */
 snprintf(d[1].guid,sizeof d[1].guid,"different-model");ri_update(c,&d[1],1,NULL,0);CHECK(!ri_connected(c,0)&&!ri_sample(c,0));
 saved.deadzone_pct=101;CHECK(!ri_configure(c,0,&saved));saved.deadzone_pct=25;saved.key_binding[0]=512;CHECK(!ri_restore(c,&saved,1));
 CHECK(!ri_assign(c,8,RI_SOURCE_GAMEPAD,8));CHECK(!ri_create(0,1,NULL,NULL));
 memset(d[0].name,'x',sizeof d[0].name);ri_update(c,d,1,NULL,0);ri_device list[8];CHECK(!ri_devices(c,list,8));ri_destroy(c);
}
int main(void){donor_boundaries();ownership_and_hotplug();ambiguous_and_invalid();puts("input isolation, mappings, persistence, capture, hotplug and ambiguity fixtures passed");return 0;}
