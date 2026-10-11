#include "audio.h"
#include <SDL2/SDL.h>
#include <SDL2/SDL_mixer.h>
#include <stdio.h>
#include <string.h>

// ── Internal state ────────────────────────────────────────────────────────────
// All SDL_mixer details are private to this file.
// The rest of the codebase never sees Mix_* types.

#define AUDIO_FREQUENCY  44100
#define AUDIO_CHANNELS   2       // stereo
#define AUDIO_CHUNK_SIZE 2048
#define BGM_FADE_MS      500     // crossfade duration

static struct {
    int       initialized;
    Mix_Music* bgm_tracks[BGM_COUNT];
    Mix_Chunk* sfx_tracks[SFX_COUNT];
    BgmTrack   current_bgm;
    BgmTrack   pre_combat_bgm;  // BGM to restore after combat
    int        bgm_volume;      // 0-100
    int        sfx_volume;      // 0-100
} g_audio = {0};

// File paths for BGM tracks
static const char* bgm_paths[BGM_COUNT] = {
    "data/bgm/title.ogg",          // BGM_TITLE
    "data/bgm/overworld.ogg",      // BGM_OVERWORLD
    "data/bgm/dungeon.ogg",        // BGM_DUNGEON
    "data/bgm/dungeon_alert.ogg",  // BGM_DUNGEON_ALERT
    "data/bgm/safe_room.ogg",      // BGM_SAFE_ROOM
    "data/bgm/combat.ogg",         // BGM_COMBAT
    "data/bgm/boss.ogg",           // BGM_BOSS
    "data/bgm/boss_phase2.ogg",    // BGM_BOSS_PHASE2
    "data/bgm/victory.ogg",        // BGM_VICTORY
    "data/bgm/game_over.ogg",      // BGM_GAME_OVER
};

// File paths for SFX
static const char* sfx_paths[SFX_COUNT] = {
    "data/sfx/menu_move.wav",       // SFX_MENU_MOVE
    "data/sfx/menu_confirm.wav",    // SFX_MENU_CONFIRM
    "data/sfx/menu_cancel.wav",     // SFX_MENU_CANCEL
    "data/sfx/attack_hit.wav",      // SFX_ATTACK_HIT
    "data/sfx/weakness_hit.wav",    // SFX_WEAKNESS_HIT
    "data/sfx/miss.wav",            // SFX_MISS
    "data/sfx/team_attack.wav",         // SFX_TEAM_ATTACK
    "data/sfx/extra_turn.wav",        // SFX_EXTRA_TURN
    "data/sfx/level_up.wav",        // SFX_LEVEL_UP
    "data/sfx/room_transition.wav", // SFX_ROOM_TRANSITION
    "data/sfx/chest_open.wav",      // SFX_CHEST_OPEN
    "data/sfx/alert_rise.wav",      // SFX_ALERT_RISE
    "data/sfx/heal.wav",            // SFX_HEAL
    "data/sfx/dialogue_advance.wav",// SFX_DIALOGUE_ADVANCE
};

// ── Lifecycle ─────────────────────────────────────────────────────────────────

int audio_init(void) {
    if (Mix_OpenAudio(AUDIO_FREQUENCY, MIX_DEFAULT_FORMAT,
                      AUDIO_CHANNELS, AUDIO_CHUNK_SIZE) != 0) {
        printf("audio: Mix_OpenAudio failed: %s\n", Mix_GetError());
        return 0;
    }

    Mix_AllocateChannels(16);  // 16 simultaneous SFX channels

    g_audio.initialized   = 1;
    g_audio.current_bgm   = BGM_NONE;
    g_audio.pre_combat_bgm= BGM_NONE;
    g_audio.bgm_volume    = 70;
    g_audio.sfx_volume    = 80;

    Mix_VolumeMusic((g_audio.bgm_volume * MIX_MAX_VOLUME) / 100);
    Mix_Volume(-1,  (g_audio.sfx_volume * MIX_MAX_VOLUME) / 100);

    // Load all BGM tracks (missing files are skipped gracefully)
    for (int i = 0; i < BGM_COUNT; i++) {
        g_audio.bgm_tracks[i] = Mix_LoadMUS(bgm_paths[i]);
        if (!g_audio.bgm_tracks[i])
            printf("audio: BGM missing: %s\n", bgm_paths[i]);
    }

    // Load all SFX (missing files are skipped gracefully)
    for (int i = 0; i < SFX_COUNT; i++) {
        g_audio.sfx_tracks[i] = Mix_LoadWAV(sfx_paths[i]);
        if (!g_audio.sfx_tracks[i])
            printf("audio: SFX missing: %s\n", sfx_paths[i]);
    }

    printf("audio: initialized (SDL_mixer)\n");
    return 1;
}

void audio_shutdown(void) {
    if (!g_audio.initialized) return;
    Mix_HaltMusic();
    for (int i = 0; i < BGM_COUNT; i++)
        if (g_audio.bgm_tracks[i]) Mix_FreeMusic(g_audio.bgm_tracks[i]);
    for (int i = 0; i < SFX_COUNT; i++)
        if (g_audio.sfx_tracks[i]) Mix_FreeChunk(g_audio.sfx_tracks[i]);
    Mix_CloseAudio();
    g_audio.initialized = 0;
}

// ── BGM control ───────────────────────────────────────────────────────────────

void audio_play_bgm(BgmTrack track) {
    if (!g_audio.initialized) return;
    if (track == g_audio.current_bgm) return;  // already playing
    if (track < 0 || track >= BGM_COUNT) return;

    g_audio.current_bgm = track;

    if (!g_audio.bgm_tracks[track]) {
        // File missing — stop current music
        Mix_FadeOutMusic(BGM_FADE_MS);
        return;
    }

    if (Mix_PlayingMusic()) {
        Mix_FadeOutMusic(BGM_FADE_MS);
        // Note: SDL_mixer doesn't support simultaneous fade-out + fade-in
        // cleanly without callbacks. We fade out then play immediately.
        // For a polished crossfade, a timer callback would be needed.
    }

    Mix_FadeInMusic(g_audio.bgm_tracks[track], -1, BGM_FADE_MS);
    Mix_VolumeMusic((g_audio.bgm_volume * MIX_MAX_VOLUME) / 100);
}

void audio_stop_bgm(int fade_ms) {
    if (!g_audio.initialized) return;
    g_audio.current_bgm = BGM_NONE;
    if (fade_ms > 0)
        Mix_FadeOutMusic(fade_ms);
    else
        Mix_HaltMusic();
}

void audio_pause_bgm(void) {
    if (!g_audio.initialized) return;
    Mix_PauseMusic();
}

void audio_resume_bgm(void) {
    if (!g_audio.initialized) return;
    Mix_ResumeMusic();
}

BgmTrack audio_current_bgm(void) {
    return g_audio.current_bgm;
}

// ── SFX control ───────────────────────────────────────────────────────────────

void audio_play_sfx(SfxId sfx) {
    if (!g_audio.initialized) return;
    if (sfx < 0 || sfx >= SFX_COUNT) return;
    if (!g_audio.sfx_tracks[sfx]) return;
    Mix_PlayChannel(-1, g_audio.sfx_tracks[sfx], 0);
}

// ── Volume control ────────────────────────────────────────────────────────────

void audio_set_bgm_volume(int vol) {
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    g_audio.bgm_volume = vol;
    if (g_audio.initialized)
        Mix_VolumeMusic((vol * MIX_MAX_VOLUME) / 100);
}

void audio_set_sfx_volume(int vol) {
    if (vol < 0) vol = 0;
    if (vol > 100) vol = 100;
    g_audio.sfx_volume = vol;
    if (g_audio.initialized)
        Mix_Volume(-1, (vol * MIX_MAX_VOLUME) / 100);
}

int audio_get_bgm_volume(void) { return g_audio.bgm_volume; }
int audio_get_sfx_volume(void) { return g_audio.sfx_volume; }

// ── Update ────────────────────────────────────────────────────────────────────

void audio_update(void) {
    // Currently a no-op — SDL_mixer handles streaming internally.
    // In a 3D engine this might pump an audio thread or update 3D positions.
}

// ── Convenience helpers ───────────────────────────────────────────────────────

void audio_update_dungeon_bgm(int alert_level, int in_safe_room) {
    if (in_safe_room) {
        audio_play_bgm(BGM_SAFE_ROOM);
    } else if (alert_level >= 50) {
        audio_play_bgm(BGM_DUNGEON_ALERT);
    } else {
        audio_play_bgm(BGM_DUNGEON);
    }
}

void audio_start_combat_bgm(int is_boss, int is_phase2) {
    // Remember what was playing so we can restore it after combat
    g_audio.pre_combat_bgm = g_audio.current_bgm;
    if (is_boss && is_phase2)
        audio_play_bgm(BGM_BOSS_PHASE2);
    else if (is_boss)
        audio_play_bgm(BGM_BOSS);
    else
        audio_play_bgm(BGM_COMBAT);
}

void audio_end_combat_bgm(void) {
    // Play victory sting then return to pre-combat BGM
    // For now just return to pre-combat BGM
    // A proper implementation would play BGM_VICTORY briefly first
    if (g_audio.pre_combat_bgm != BGM_NONE)
        audio_play_bgm(g_audio.pre_combat_bgm);
    g_audio.pre_combat_bgm = BGM_NONE;
}