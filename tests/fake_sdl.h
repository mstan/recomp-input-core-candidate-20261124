#ifndef FAKE_SDL_H
#define FAKE_SDL_H
#include "recomp_input.h"
#include <stdlib.h>
#define SDL_VERSION_ATLEAST(a,b,c) 0
#define SDL_CONTROLLER_BUTTON_MAX 32
#define SDL_CONTROLLER_AXIS_MAX 6
typedef int32_t SDL_JoystickID;
typedef int SDL_GameControllerButton;
typedef int SDL_GameControllerAxis;
typedef struct {uint32_t id;} SDL_GameController;
typedef struct {char text[40];} SDL_JoystickGUID;
int SDL_NumJoysticks(void);
int SDL_IsGameController(int);
SDL_JoystickID SDL_JoystickGetDeviceInstanceID(int);
const char *SDL_GameControllerNameForIndex(int);
SDL_JoystickGUID SDL_JoystickGetDeviceGUID(int);
void SDL_JoystickGetGUIDString(SDL_JoystickGUID,char *,int);
SDL_GameController *SDL_GameControllerOpen(int);
void SDL_GameControllerClose(SDL_GameController *);
char *SDL_GameControllerMapping(SDL_GameController *);
int SDL_GameControllerGetButton(SDL_GameController *,SDL_GameControllerButton);
int16_t SDL_GameControllerGetAxis(SDL_GameController *,SDL_GameControllerAxis);
#define SDL_free free
void oracle_feed(const ri_device *,size_t);
int oracle_binding(uint32_t id,uint32_t binding,unsigned deadzone);
#endif
