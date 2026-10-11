#include "save.h"
#include "stats.h"
#include "inventory.h"
#include "gamestate.h"
#include "time_system.h"
#include "social_stats.h"
#include "bond.h"
#include "dungeon.h"
#include "summon.h"
#include "audio.h"
#include <stdio.h>
#include <string.h>

// ── Safe overworld respawn ───────────────────────────────────────────────────
// On load, always place the player at a known-valid overworld position.
// Never trust raw saved coordinates — they may be dungeon coords or near walls.
#define SAFE_SPAWN_X  200.0f
#define SAFE_SPAWN_Y  400.0f
#define SAFE_ROOM_ID  0

static char g_save_dir[256] = "";  // empty = use cwd (default behavior)

void save_set_directory(const char* dir) {
    if (!dir || dir[0] == '\0') {
        g_save_dir[0] = '\0';
        return;
    }
    snprintf(g_save_dir, 256, "%s", dir);
    int n = strlen(g_save_dir);
    if (n > 0 && (g_save_dir[n-1] == '/' || g_save_dir[n-1] == '\\'))
        g_save_dir[n-1] = '\0';
}

static void get_save_path(int slot, char* path, int len) {
    if (g_save_dir[0])
        snprintf(path, len, "%s/opal_save%d.sav", g_save_dir, slot);
    else
        snprintf(path, len, "opal_save%d.sav", slot);
}

// ── Low-level IO ─────────────────────────────────────────────────────────────

int save_game(int slot, SaveData* data) {
    if (slot < 0 || slot >= SAVE_SLOTS) return 0;
    char path[256];
    get_save_path(slot, path, 256);
    FILE* f = fopen(path, "wb");
    if (!f) {
        printf("Save failed: could not open %s\n", path);
        return 0;
    }
    int magic   = SAVE_MAGIC;
    int version = SAVE_VERSION;
    size_t w = 0;
    w += fwrite(&magic,   sizeof(int),      1, f);
    w += fwrite(&version, sizeof(int),      1, f);
    w += fwrite(data,     sizeof(SaveData), 1, f);
    fclose(f);
    if (w != 3) {
        printf("Save warning: wrote %zu/3 blocks to slot %d\n", w, slot);
        return 0;
    }
    printf("Saved to slot %d (%.1f KB)\n",
           slot, (float)sizeof(SaveData) / 1024.0f);
    return 1;
}

int load_game(int slot, SaveData* data) {
    if (slot < 0 || slot >= SAVE_SLOTS) return 0;
    char path[256];
    get_save_path(slot, path, 256);
    FILE* f = fopen(path, "rb");
    if (!f) return 0;

    int magic = 0, version = 0;
    if (fread(&magic,   sizeof(int), 1, f) != 1 ||
        fread(&version, sizeof(int), 1, f) != 1) {
        printf("Load failed: bad header in slot %d\n", slot);
        fclose(f); return 0;
    }
    if (magic != SAVE_MAGIC) {
        printf("Load failed: bad magic in slot %d (0x%X)\n", slot, magic);
        fclose(f); return 0;
    }
    if (version != SAVE_VERSION) {
        printf("Load failed: version mismatch slot %d (file=%d expect=%d)\n",
               slot, version, SAVE_VERSION);
        fclose(f); return 0;
    }
    if (fread(data, sizeof(SaveData), 1, f) != 1) {
        printf("Load failed: truncated data in slot %d\n", slot);
        fclose(f); return 0;
    }
    fclose(f);

    // Sanity-clamp all counts so loops never go out of bounds
    if (data->party_count    < 1 || data->party_count    > PARTY_MAX)
        data->party_count    = 1;
    if (data->slot_count     < 0 || data->slot_count     > INVENTORY_MAX_SLOTS)
        data->slot_count     = 0;
    if (data->flag_count     < 0 || data->flag_count     > MAX_FLAGS)
        data->flag_count     = 0;
    if (data->var_count      < 0 || data->var_count      > MAX_VARS)
        data->var_count      = 0;
    if (data->knowledge_count< 0 || data->knowledge_count> MAX_ENEMY_TYPES)
        data->knowledge_count= 0;
    if (data->yen            < 0)
        data->yen            = 0;

    printf("Loaded slot %d\n", slot);
    return 1;
}

int delete_save(int slot) {
    char path[256];
    get_save_path(slot, path, 256);
    return remove(path) == 0;
}

int save_exists(int slot) {
    if (slot < 0 || slot >= SAVE_SLOTS) return 0;
    char path[256];
    get_save_path(slot, path, 256);
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    int magic = 0, version = 0;
    int ok = (fread(&magic,   sizeof(int), 1, f) == 1 &&
              fread(&version, sizeof(int), 1, f) == 1 &&
              magic   == SAVE_MAGIC &&
              version == SAVE_VERSION);
    fclose(f);
    return ok;
}

// Returns version of save in slot, or 0 if missing/bad magic
int save_get_version(int slot) {
    if (slot < 0 || slot >= SAVE_SLOTS) return 0;
    char path[256];
    get_save_path(slot, path, 256);
    FILE* f = fopen(path, "rb");
    if (!f) return 0;
    int magic = 0, version = 0;
    int ok = (fread(&magic,   sizeof(int), 1, f) == 1 &&
              fread(&version, sizeof(int), 1, f) == 1 &&
              magic == SAVE_MAGIC);
    fclose(f);
    return ok ? version : 0;
}

void save_build_timestamp(SaveData* sd, TimeSystem* ts) {
    snprintf(sd->timestamp, 32, "Day %d - %s %s",
             ts->day,
             dayofweek_name(ts->day_of_week),
             timeslot_name(ts->slot));
}

// ── Save direction: copy LIVE state → SaveData ───────────────────────────────

void save_data_from_state(SaveData* sd, Party* party, Inventory* inv,
                          GameState* gs, TimeSystem* ts, SocialStats* ss,
                          BondDB* cdb,
                          float player_x, float player_y,
                          int current_room,
                          DungeonSystem* dungeon_sys) {
    memset(sd, 0, sizeof(SaveData));
    sd->used = 1;

    // Player position: always store an overworld-safe position unless saving
    // outside the dungeon. The dungeon spawn is handled separately via the
    // safe-room ID and dungeon state.
    int in_dungeon = (dungeon_sys &&
                      dungeon_get_active((DungeonSystem*)dungeon_sys) &&
                      dungeon_get_active((DungeonSystem*)dungeon_sys)->active);
    sd->player_x     = (player_x > 32.0f && player_x < 5000.0f && !in_dungeon)
                       ? player_x : SAFE_SPAWN_X;
    sd->player_y     = (player_y > 32.0f && player_y < 5000.0f && !in_dungeon)
                       ? player_y : SAFE_SPAWN_Y;
    sd->current_room = (current_room >= 0 && !in_dungeon)
                       ? current_room : SAFE_ROOM_ID;

    // ── Party ─────────────────────────────────────────────────────────────────
    sd->party_count = (party->count > 0 && party->count <= PARTY_MAX)
                      ? party->count : 1;
    for (int i = 0; i < sd->party_count; i++)
        sd->members[i] = party->members[i];

    // ── Inventory ─────────────────────────────────────────────────────────────
    sd->slot_count = inv->slot_count;
    for (int i = 0; i < inv->slot_count; i++)
        sd->slots[i] = inv->slots[i];
    sd->yen = inv->yen;

    // ── Flags / vars / knowledge ──────────────────────────────────────────────
    sd->flag_count = gs->flag_count;
    for (int i = 0; i < gs->flag_count; i++)
        sd->flags[i] = gs->flags[i];
    sd->var_count = gs->var_count;
    for (int i = 0; i < gs->var_count; i++)
        sd->vars[i] = gs->vars[i];
    sd->knowledge_count = gs->knowledge_count;
    for (int i = 0; i < gs->knowledge_count; i++)
        sd->knowledge[i] = gs->knowledge[i];

    // ── Time ──────────────────────────────────────────────────────────────────
    sd->day         = ts->day;
    sd->day_of_week = (int)ts->day_of_week;
    sd->slot        = (int)ts->slot;
    sd->weather     = (int)ts->weather;
    sd->month       = ts->month;
    sd->date        = ts->date;

    // ── Social ────────────────────────────────────────────────────────────────
    for (int i = 0; i < SOCIAL_COUNT; i++) {
        sd->social_exp[i]  = ss->exp[i];
        sd->social_rank[i] = ss->rank[i];
    }

    // ── Bonds ────────────────────────────────────────────────────────────
    for (int i = 0; i < cdb->count && i < MAX_BONDS; i++) {
        sd->bond_rank[i]   = cdb->bonds[i].current_rank;
        sd->bond_points[i] = cdb->bonds[i].points;
    }

    // ── Dungeon ───────────────────────────────────────────────────────────────
    if (dungeon_sys) {
        DungeonSystem* ds = (DungeonSystem*)dungeon_sys;
        Dungeon*        p  = &ds->dungeon;

        // Always save as inactive — player re-enters via Hungerscape on next session
        sd->dungeon_active        = 0;
        sd->dungeon_idx           = ds->current_dungeon_idx;
        sd->dungeon_current_floor = p->current_floor;
        sd->dungeon_alert_level   = p->alert_level;

        if (p->current_floor >= 0 && p->current_floor < p->floor_count)
            sd->dungeon_current_room =
                p->floors[p->current_floor].current_room;

        // Boss defeat state from floor 3 (Throne)
        sd->dungeon_boss_defeated = 0;
        if (p->floor_count >= 3) {
            DungeonFloor* f3 = &p->floors[2];
            for (int r = 0; r < f3->room_count; r++) {
                if (f3->rooms[r].type == ROOM_BOSS) {
                    sd->dungeon_boss_defeated = f3->rooms[r].boss.defeated;
                    break;
                }
            }
        }

        // Per-floor per-room state: chest opened, cleared, visited
        for (int fl = 0; fl < p->floor_count && fl < 5; fl++) {
            DungeonFloor* f = &p->floors[fl];
            for (int rm = 0;
                 rm < f->room_count && rm < SAVE_MAX_CHESTS; rm++) {
                sd->dungeon_chest_opened[fl][rm] = f->rooms[rm].chest.opened;
                sd->dungeon_room_cleared[fl][rm] = f->rooms[rm].cleared;
                sd->dungeon_room_visited[fl][rm] = f->rooms[rm].visited;
            }
        }
    }

    // ── Summon stock ─────────────────────────────────────────────────────────
    extern SummonStock g_summon_stock;
    sd->summon_count      = g_summon_stock.count;
    sd->summon_active_idx = g_summon_stock.active_idx;
    for (int i = 0; i < g_summon_stock.count && i < 12; i++) {
        Summon* p = &g_summon_stock.stock[i];
        strncpy(sd->summon_names[i], p->name, 63);
        sd->summon_levels[i]       = p->level;
        sd->summon_arcana[i]       = (int)p->arcana;
        sd->summon_skill_counts[i] = p->skill_count;
        for (int j = 0; j < p->skill_count && j < 8; j++)
            sd->summon_skill_ids[i][j] = p->skill_ids[j];
    }

    // ── Compendium ────────────────────────────────────────────────────────────
    extern SummonCompendium g_summon_comp;
    sd->comp_count = g_summon_comp.count;
    for (int i = 0; i < g_summon_comp.count && i < 64; i++) {
        Summon* cp = &g_summon_comp.entries[i];
        strncpy(sd->comp_names[i], cp->name, 63);
        sd->comp_levels[i]       = cp->level;
        sd->comp_arcana[i]       = (int)cp->arcana;
        sd->comp_skill_counts[i] = cp->skill_count;
        for (int j = 0; j < cp->skill_count && j < 8; j++)
            sd->comp_skill_ids[i][j] = cp->skill_ids[j];
    }

    save_build_timestamp(sd, ts);
}

// ── Load direction: copy SaveData → LIVE state ───────────────────────────────

void save_data_to_state(SaveData* sd, Party* party, Inventory* inv,
                        GameState* gs, TimeSystem* ts, SocialStats* ss,
                        BondDB* cdb,
                        float* player_x, float* player_y,
                        int* current_room,
                        DungeonSystem* dungeon_sys) {
    // Always spawn at safe overworld position on load
    *player_x     = SAFE_SPAWN_X;
    *player_y     = SAFE_SPAWN_Y;
    *current_room = (sd->current_room >= 0) ? sd->current_room : SAFE_ROOM_ID;

    // ── Party ─────────────────────────────────────────────────────────────────
    party->count = sd->party_count;
    for (int i = 0; i < sd->party_count; i++)
        party->members[i] = sd->members[i];

    // ── Inventory ─────────────────────────────────────────────────────────────
    inv->slot_count = sd->slot_count;
    for (int i = 0; i < sd->slot_count; i++)
        inv->slots[i] = sd->slots[i];
    inv->yen = sd->yen;

    // ── Summon stock ─────────────────────────────────────────────────────────
    extern SummonStock g_summon_stock;
    extern SummonDB    g_summon_db;
    if (sd->summon_count > 0) {
        g_summon_stock.count      = 0;
        g_summon_stock.active_idx = sd->summon_active_idx;
        for (int i = 0; i < sd->summon_count && i < 12; i++) {
            int found = 0;
            for (int j = 0; j < g_summon_db.count; j++) {
                if (strcmp(g_summon_db.entries[j].name,
                           sd->summon_names[i]) == 0) {
                    Summon p = g_summon_db.entries[j];
                    p.level       = sd->summon_levels[i];
                    p.skill_count = sd->summon_skill_counts[i];
                    for (int k = 0; k < p.skill_count; k++)
                        p.skill_ids[k] = sd->summon_skill_ids[i][k];
                    g_summon_stock.stock[g_summon_stock.count++] = p;
                    found = 1;
                    break;
                }
            }
            if (!found) {
                // Unknown summon — add Opaline as fallback
                if (g_summon_db.count > 0)
                    g_summon_stock.stock[g_summon_stock.count++] =
                        g_summon_db.entries[0];
            }
        }
    }

    // ── Compendium ────────────────────────────────────────────────────────────
    extern SummonCompendium g_summon_comp;
    g_summon_comp.count = 0;
    for (int i = 0; i < sd->comp_count && i < 64; i++) {
        Summon base = {0};
        for (int j = 0; j < g_summon_db.count; j++) {
            if (strcmp(g_summon_db.entries[j].name, sd->comp_names[i]) == 0) {
                base = g_summon_db.entries[j]; break;
            }
        }
        if (!base.valid) { strncpy(base.name, sd->comp_names[i], 63); base.valid = 1; }
        base.level       = sd->comp_levels[i];
        base.arcana      = (Arcana)sd->comp_arcana[i];
        base.skill_count = sd->comp_skill_counts[i];
        for (int j = 0; j < base.skill_count; j++)
            base.skill_ids[j] = sd->comp_skill_ids[i][j];
        g_summon_comp.entries[g_summon_comp.count++] = base;
    }

    // ── Flags / vars / knowledge ──────────────────────────────────────────────
    gs->flag_count = sd->flag_count;
    for (int i = 0; i < sd->flag_count; i++)
        gs->flags[i] = sd->flags[i];
    gs->var_count = sd->var_count;
    for (int i = 0; i < sd->var_count; i++)
        gs->vars[i] = sd->vars[i];
    gs->knowledge_count = sd->knowledge_count;
    for (int i = 0; i < sd->knowledge_count; i++)
        gs->knowledge[i] = sd->knowledge[i];

    // ── Time ──────────────────────────────────────────────────────────────────
    ts->day         = sd->day;
    ts->day_of_week = (DayOfWeek)sd->day_of_week;
    ts->slot        = (TimeSlot)sd->slot;
    ts->weather     = (Weather)sd->weather;
    ts->month       = sd->month;
    ts->date        = sd->date;

    // ── Social ────────────────────────────────────────────────────────────────
    for (int i = 0; i < SOCIAL_COUNT; i++) {
        ss->exp[i]  = sd->social_exp[i];
        ss->rank[i] = sd->social_rank[i];
    }

    // ── Bonds ────────────────────────────────────────────────────────────
    for (int i = 0; i < cdb->count && i < MAX_BONDS; i++) {
        cdb->bonds[i].current_rank = sd->bond_rank[i];
        cdb->bonds[i].points       = sd->bond_points[i];
    }

    // ── Dungeon ───────────────────────────────────────────────────────────────
    // Rebuild dungeon from scratch first so all rooms/enemies reset,
    // then apply only what the save recorded on top.
    if (dungeon_sys) {
        DungeonSystem* ds = (DungeonSystem*)dungeon_sys;

        dungeon_reset_active(ds);

        ds->current_dungeon_idx =
            (sd->dungeon_idx >= 0 && sd->dungeon_idx < ds->dungeon_count)
            ? sd->dungeon_idx : 0;
        Dungeon* p = &ds->dungeon;
        p->active = 0;
        p->alert_level   = sd->dungeon_alert_level;
        p->current_floor = (sd->dungeon_current_floor >= 0 &&
                            sd->dungeon_current_floor < p->floor_count)
                           ? sd->dungeon_current_floor : 0;

        int cf = p->current_floor;
        if (cf < p->floor_count) {
            int nr = sd->dungeon_current_room;
            p->floors[cf].current_room =
                (nr >= 0 && nr < p->floors[cf].room_count) ? nr : 0;
        }

        for (int fl = 0; fl < p->floor_count && fl < 5; fl++) {
            DungeonFloor* f = &p->floors[fl];
            for (int rm = 0;
                 rm < f->room_count && rm < SAVE_MAX_CHESTS; rm++) {
                f->rooms[rm].chest.opened = sd->dungeon_chest_opened[fl][rm];
                f->rooms[rm].cleared      = sd->dungeon_room_cleared[fl][rm];
                f->rooms[rm].visited      = sd->dungeon_room_visited[fl][rm];
                if (f->rooms[rm].cleared) {
                    for (int en = 0; en < f->rooms[rm].entities.count; en++)
                        f->rooms[rm].entities.entities[en].defeated = 1;
                }
            }
        }

        if (sd->dungeon_boss_defeated && p->floor_count >= 3) {
            DungeonFloor* f3 = &p->floors[2];
            for (int rm = 0; rm < f3->room_count; rm++) {
                if (f3->rooms[rm].type == ROOM_BOSS) {
                    f3->rooms[rm].boss.defeated = 1;
                    f3->rooms[rm].cleared       = 1;
                    break;
                }
            }
        }
    }
}