#ifndef OPAL_SDL_COMPAT_H
#define OPAL_SDL_COMPAT_H

// SDL2 stubs for GDExtension build — no SDL2 needed on build machine
// Render functions compile but are never called (Godot replaces them)

#include <stdint.h>

typedef unsigned char  Uint8;
typedef unsigned short Uint16;
typedef unsigned int   Uint32;
typedef uint64_t       Uint64;

// SDL_Color
typedef struct { Uint8 r, g, b, a; } SDL_Color;

// SDL_Rect
typedef struct { int x, y, w, h; } SDL_Rect;

// SDL_Surface (opaque stub)
typedef struct {
    int    flags;
    void*  format;
    int    w, h;
    int    pitch;
    void*  pixels;
    void*  userdata;
    int    locked;
    void*  lock_data;
    SDL_Rect clip_rect;
    void*  map;
    int    refcount;
} SDL_Surface;

// Opaque types
typedef void SDL_Renderer;
typedef void SDL_Texture;
typedef void SDL_Window;
typedef void TTF_Font;

// Keycode
typedef int SDL_Keycode;
typedef struct { Uint16 scancode; SDL_Keycode sym; Uint16 mod; } SDL_Keysym;

// Event
typedef struct {
    Uint32 type;
    struct { Uint32 timestamp; Uint32 windowID; Uint8 state; Uint8 repeat; Uint8 padding2; Uint8 padding3; SDL_Keysym keysym; } key;
} SDL_Event;

// Blend mode
typedef int SDL_BlendMode;
#define SDL_BLENDMODE_NONE  0
#define SDL_BLENDMODE_BLEND 1
#define SDL_BLENDMODE_ADD   2
#define SDL_BLENDMODE_MOD   4

// Event types
#define SDL_QUIT    0x100
#define SDL_KEYDOWN 0x300
#define SDL_KEYUP   0x301

// Key codes
#define SDLK_RETURN  13
#define SDLK_ESCAPE  27
#define SDLK_SPACE   32
#define SDLK_UP      0x40000052u
#define SDLK_DOWN    0x40000051u
#define SDLK_LEFT    0x40000050u
#define SDLK_RIGHT   0x4000004Fu
#define SDLK_a       97
#define SDLK_b       98
#define SDLK_c       99
#define SDLK_d       100
#define SDLK_e       101
#define SDLK_f       102
#define SDLK_g       103
#define SDLK_h       104
#define SDLK_i       105
#define SDLK_j       106
#define SDLK_k       107
#define SDLK_l       108
#define SDLK_m       109
#define SDLK_n       110
#define SDLK_o       111
#define SDLK_p       112
#define SDLK_q       113
#define SDLK_r       114
#define SDLK_s       115
#define SDLK_t       116
#define SDLK_u       117
#define SDLK_v       118
#define SDLK_w       119
#define SDLK_x       120
#define SDLK_y       121
#define SDLK_z       122
#define SDLK_1       49
#define SDLK_2       50
#define SDLK_3       51
#define SDLK_4       52
#define SDLK_5       53
#define SDLK_F1      0x4000003Au
#define SDLK_F2      0x4000003Bu
#define SDLK_F5      0x4000003Eu

// Stub functions — no-ops in GDExtension build
static inline void     SDL_SetRenderDrawColor(SDL_Renderer* r, Uint8 a, Uint8 b, Uint8 c, Uint8 d) { (void)r;(void)a;(void)b;(void)c;(void)d; }
static inline void     SDL_RenderFillRect(SDL_Renderer* r, const SDL_Rect* rect) { (void)r;(void)rect; }
static inline void     SDL_RenderDrawRect(SDL_Renderer* r, const SDL_Rect* rect) { (void)r;(void)rect; }
static inline void     SDL_RenderDrawLine(SDL_Renderer* r, int x1, int y1, int x2, int y2) { (void)r;(void)x1;(void)y1;(void)x2;(void)y2; }
static inline int      SDL_RenderCopy(SDL_Renderer* r, SDL_Texture* t, const SDL_Rect* s, const SDL_Rect* d) { (void)r;(void)t;(void)s;(void)d; return 0; }
static inline void     SDL_FreeSurface(SDL_Surface* s) { (void)s; }
static inline void     SDL_DestroyTexture(SDL_Texture* t) { (void)t; }
static inline void     SDL_SetRenderDrawBlendMode(SDL_Renderer* r, SDL_BlendMode m) { (void)r;(void)m; }
static inline void     SDL_SetTextureAlphaMod(SDL_Texture* t, Uint8 a) { (void)t;(void)a; }
static inline SDL_Surface* TTF_RenderText_Blended(TTF_Font* f, const char* t, SDL_Color c) { (void)f;(void)t;(void)c; return 0; }
static inline SDL_Texture* SDL_CreateTextureFromSurface(SDL_Renderer* r, SDL_Surface* s) { (void)r;(void)s; return 0; }
static inline void     SDL_SetTextureColorMod(SDL_Texture* t, Uint8 r, Uint8 g, Uint8 b) { (void)t;(void)r;(void)g;(void)b; }
static inline void     SDL_RenderCopyEx(SDL_Renderer* r, SDL_Texture* t, const SDL_Rect* s, const SDL_Rect* d, double a, const void* c, int f) { (void)r;(void)t;(void)s;(void)d;(void)a;(void)c;(void)f; }
static inline void     SDL_QueryTexture(SDL_Texture* t, Uint32* f, int* a, int* w, int* h) { (void)t;(void)f;(void)a; if(w)*w=0; if(h)*h=0; }
static inline int      TTF_SizeText(TTF_Font* f, const char* t, int* w, int* h) { (void)f;(void)t; if(w)*w=0; if(h)*h=0; return 0; }


// SDL Scancodes
typedef int SDL_Scancode;
#define SDL_SCANCODE_A    4
#define SDL_SCANCODE_B    5
#define SDL_SCANCODE_C    6
#define SDL_SCANCODE_D    7
#define SDL_SCANCODE_E    8
#define SDL_SCANCODE_F    9
#define SDL_SCANCODE_G    10
#define SDL_SCANCODE_H    11
#define SDL_SCANCODE_I    12
#define SDL_SCANCODE_J    13
#define SDL_SCANCODE_K    14
#define SDL_SCANCODE_L    15
#define SDL_SCANCODE_M    16
#define SDL_SCANCODE_N    17
#define SDL_SCANCODE_O    18
#define SDL_SCANCODE_P    19
#define SDL_SCANCODE_Q    20
#define SDL_SCANCODE_R    21
#define SDL_SCANCODE_S    22
#define SDL_SCANCODE_T    23
#define SDL_SCANCODE_U    24
#define SDL_SCANCODE_V    25
#define SDL_SCANCODE_W    26
#define SDL_SCANCODE_X    27
#define SDL_SCANCODE_Y    28
#define SDL_SCANCODE_Z    29
#define SDL_SCANCODE_UP   82
#define SDL_SCANCODE_DOWN 81
#define SDL_SCANCODE_LEFT 80
#define SDL_SCANCODE_RIGHT 79
#define SDL_NUM_SCANCODES 512

// SDL_GetKeyboardState stub
static inline const Uint8* SDL_GetKeyboardState(int* n) { static Uint8 s[512] = {0}; if(n)*n=512; return s; }

// Additional SDL stubs
static inline Uint32 SDL_GetTicks(void) { return 0; }
static inline void SDL_RenderClear(SDL_Renderer* r) { (void)r; }
static inline void SDL_RenderPresent(SDL_Renderer* r) { (void)r; }
static inline double SDL_sinf(double x) { return 0.0; }
static inline double SDL_cosf(double x) { return 0.0; }
static inline void SDL_SetRenderTarget(SDL_Renderer* r, SDL_Texture* t) { (void)r;(void)t; }
static inline SDL_Texture* SDL_CreateTexture(SDL_Renderer* r, Uint32 f, int a, int w, int h) { (void)r;(void)f;(void)a;(void)w;(void)h; return 0; }
static inline void SDL_GetWindowSize(SDL_Window* w, int* x, int* y) { (void)w; if(x)*x=1280; if(y)*y=720; }
static inline void SDL_RenderSetLogicalSize(SDL_Renderer* r, int w, int h) { (void)r;(void)w;(void)h; }

#endif