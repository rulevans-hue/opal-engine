#ifdef OPAL_GDEXTENSION
// Audio stubs for GDExtension build — Godot handles audio natively
#include "audio.h"

int      audio_init(void)                              { return 1; }
void     audio_shutdown(void)                          {}
void     audio_play_bgm(BgmTrack t)                   { (void)t; }
void     audio_stop_bgm(int ms)                        { (void)ms; }
void     audio_pause_bgm(void)                         {}
void     audio_resume_bgm(void)                        {}
BgmTrack audio_current_bgm(void)                       { return BGM_NONE; }
void     audio_play_sfx(SfxId s)                       { (void)s; }
void     audio_set_bgm_volume(int v)                   { (void)v; }
void     audio_set_sfx_volume(int v)                   { (void)v; }
int      audio_get_bgm_volume(void)                    { return 70; }
int      audio_get_sfx_volume(void)                    { return 80; }
void     audio_update(void)                            {}
void     audio_update_dungeon_bgm(int a, int b)        { (void)a;(void)b; }
void     audio_start_combat_bgm(int a, int b)          { (void)a;(void)b; }
void     audio_end_combat_bgm(void)                    {}
#endif