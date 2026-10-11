#ifndef AUDIO_H
#define AUDIO_H

// ── Audio System ──────────────────────────────────────────────────────────────
// Renderer-agnostic audio interface. Game code only calls these functions.
// The implementation (audio_sdl2.c) handles all platform-specific details.
// For the 3D port: replace audio_sdl2.c with audio_openal.c or equivalent.
// Never include SDL_mixer.h outside of audio_sdl2.c.

// ── BGM tracks ────────────────────────────────────────────────────────────────

typedef enum {
    BGM_NONE = -1,
    BGM_TITLE,           // title screen
    BGM_OVERWORLD,       // real world exploration
    BGM_DUNGEON,         // dungeon exploration (normal alert)
    BGM_DUNGEON_ALERT,   // dungeon exploration (alert > 50%)
    BGM_SAFE_ROOM,       // safe room inside dungeon
    BGM_COMBAT,          // normal battle
    BGM_BOSS,            // boss battle
    BGM_BOSS_PHASE2,     // boss phase 2
    BGM_VICTORY,         // battle won
    BGM_GAME_OVER,       // party wiped
    BGM_COUNT
} BgmTrack;

// ── SFX ───────────────────────────────────────────────────────────────────────

typedef enum {
    SFX_MENU_MOVE,       // cursor moves in menu
    SFX_MENU_CONFIRM,    // menu option confirmed
    SFX_MENU_CANCEL,     // menu closed / back
    SFX_ATTACK_HIT,      // physical hit lands
    SFX_WEAKNESS_HIT,    // elemental weakness exploited
    SFX_MISS,            // attack misses
    SFX_TEAM_ATTACK,         // team attack trigger
    SFX_EXTRA_TURN,        // extra turn trigger
    SFX_LEVEL_UP,        // character levels up
    SFX_ROOM_TRANSITION, // moving between dungeon rooms
    SFX_CHEST_OPEN,      // treasure chest opened
    SFX_ALERT_RISE,      // alert level increases
    SFX_HEAL,            // HP/SP restored
    SFX_DIALOGUE_ADVANCE,// dialogue text advances
    SFX_COUNT
} SfxId;

// ── Lifecycle ─────────────────────────────────────────────────────────────────

// Init audio system. Call once at startup before any other audio calls.
// Returns 1 on success, 0 on failure (game continues without audio).
int  audio_init(void);

// Shutdown audio system. Call on exit.
void audio_shutdown(void);

// ── BGM control ───────────────────────────────────────────────────────────────

// Play a BGM track. If already playing the same track, does nothing.
// Fades out current track and fades in new one.
void audio_play_bgm(BgmTrack track);

// Stop current BGM with optional fade out (ms).
void audio_stop_bgm(int fade_ms);

// Pause/resume BGM (e.g. when game is paused).
void audio_pause_bgm(void);
void audio_resume_bgm(void);

// Get currently playing track.
BgmTrack audio_current_bgm(void);

// ── SFX control ───────────────────────────────────────────────────────────────

// Play a sound effect. Multiple SFX can play simultaneously.
void audio_play_sfx(SfxId sfx);

// ── Volume control ────────────────────────────────────────────────────────────

// Set volumes 0-100.
void audio_set_bgm_volume(int vol);
void audio_set_sfx_volume(int vol);
int  audio_get_bgm_volume(void);
int  audio_get_sfx_volume(void);

// ── Update ────────────────────────────────────────────────────────────────────

// Call once per frame — handles crossfades, ducking, etc.
void audio_update(void);

// ── Convenience helpers ───────────────────────────────────────────────────────
// Called by game systems — these encapsulate the "what should play now" logic.

// Dungeon: picks BGM_DUNGEON or BGM_DUNGEON_ALERT based on alert level.
void audio_update_dungeon_bgm(int alert_level, int in_safe_room);

// Combat: picks BGM_COMBAT or BGM_BOSS based on is_boss flag.
void audio_start_combat_bgm(int is_boss, int is_phase2);

// After combat ends: return to whatever was playing before.
void audio_end_combat_bgm(void);

#endif