#pragma once
#include <stdint.h>
#include <stddef.h>
typedef unsigned char Uint8;
typedef unsigned int Uint32;
typedef int SDL_Keycode;
typedef struct SDL_Window SDL_Window;
typedef struct SDL_Renderer SDL_Renderer;
typedef struct SDL_Texture SDL_Texture;
typedef struct SDL_GameController SDL_GameController;
typedef struct SDL_Joystick SDL_Joystick;
typedef struct SDL_Surface{int w,h,pitch;void *pixels;} SDL_Surface;
typedef struct SDL_Rect{int x,y,w,h;}SDL_Rect;
typedef struct SDL_Color{Uint8 r,g,b,a;}SDL_Color;
typedef struct SDL_DisplayMode{int format,w,h,refresh_rate;void*driverdata;} SDL_DisplayMode;
typedef struct SDL_Event{Uint32 type;union{struct{Uint32 event;int data1,data2;}window;struct{int repeat;struct{SDL_Keycode sym;}keysym;}key;struct{Uint8 button;}cbutton;struct{int which;}cdevice;struct{char text[32];}text;struct{int button,x,y;}button;};}SDL_Event;
#define SDL_HINT_RENDER_SCALE_QUALITY "linear"
enum{
  SDL_BLENDMODE_BLEND = 1,
  SDL_BUTTON_LEFT = 2,
  SDL_CONTROLLERBUTTONDOWN = 3,
  SDL_CONTROLLERBUTTONUP = 4,
  SDL_CONTROLLERDEVICEADDED = 5,
  SDL_CONTROLLERDEVICEREMOVED = 6,
  SDL_CONTROLLER_BUTTON_A = 7,
  SDL_CONTROLLER_BUTTON_B = 8,
  SDL_CONTROLLER_BUTTON_BACK = 9,
  SDL_CONTROLLER_BUTTON_DPAD_DOWN = 10,
  SDL_CONTROLLER_BUTTON_DPAD_LEFT = 11,
  SDL_CONTROLLER_BUTTON_DPAD_RIGHT = 12,
  SDL_CONTROLLER_BUTTON_DPAD_UP = 13,
  SDL_CONTROLLER_BUTTON_LEFTSHOULDER = 14,
  SDL_CONTROLLER_BUTTON_RIGHTSHOULDER = 15,
  SDL_CONTROLLER_BUTTON_START = 16,
  SDL_CONTROLLER_BUTTON_X = 17,
  SDL_CONTROLLER_BUTTON_Y = 18,
  SDL_INIT_EVENTS = 20,
  SDL_INIT_GAMECONTROLLER = 21,
  SDL_INIT_VIDEO = 22,
  SDL_KEYDOWN = 23,
  SDL_KEYUP = 24,
  SDL_MOUSEBUTTONDOWN = 25,
  SDL_PIXELFORMAT_ARGB8888 = 26,
  SDL_QUIT = 27,
  SDL_RENDERER_ACCELERATED = 28,
  SDL_RENDERER_PRESENTVSYNC = 29,
  SDL_RENDERER_SOFTWARE = 30,
  SDL_TEXTINPUT = 31,
  SDL_WINDOWEVENT = 32,
  SDL_WINDOWEVENT_EXPOSED = 33,
  SDL_WINDOWEVENT_SIZE_CHANGED = 34,
  SDL_WINDOWPOS_CENTERED = 35,
  SDL_WINDOW_FULLSCREEN_DESKTOP = 36,
  SDL_WINDOW_RESIZABLE = 37,
  SDL_WINDOW_SHOWN = 38
};
enum{
  SDLK_BACKSPACE = 10,
  SDLK_DOWN = 11,
  SDLK_ESCAPE = 12,
  SDLK_F1 = 13,
  SDLK_F2 = 14,
  SDLK_F3 = 15,
  SDLK_F4 = 16,
  SDLK_F5 = 17,
  SDLK_F6 = 18,
  SDLK_F7 = 19,
  SDLK_KP_ENTER = 20,
  SDLK_LEFT = 21,
  SDLK_LEFTBRACKET = 22,
  SDLK_PAGEDOWN = 23,
  SDLK_PAGEUP = 24,
  SDLK_RETURN = 25,
  SDLK_RIGHT = 26,
  SDLK_RIGHTBRACKET = 27,
  SDLK_TAB = 28,
  SDLK_UP = 29,
  SDLK_h = 30,
  SDLK_k = 31,
  SDLK_p = 32,
  SDLK_s = 33,
  SDLK_u = 34,
  SDLK_x = 35,
  SDLK_y = 36
};
void SDL_free(void*);
char *SDL_GetPrefPath(const char *,const char *);
void SDL_DestroyTexture(SDL_Texture*);
const char *SDL_GetError(void);
