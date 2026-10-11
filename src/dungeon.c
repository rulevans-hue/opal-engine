#include "dungeon.h"
#include "enemy_registry.h"
#include "audio.h"
#include "camera.h"
#include "save.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <math.h>

// The boss appears once the party has reached the final floor.
// The team meeting there sets treasure_<clear_flag>.
static int boss_ready(DungeonSystem* ds_sys) {
    Dungeon* p = dungeon_get_active(ds_sys);
    if (!p) return 0;
    char flag[128];
    snprintf(flag, 128, "treasure_%s", p->clear_flag);
    return gamestate_get_flag(ds_sys->gs, flag);
}


// ── External dungeon builders ─────────────────────────────────────────────────
// Each dungeon is defined in its own file (dungeon_1.c, dungeon_2.c, etc.)
void build_dungeon_1(Dungeon* p, EnemyRegistry* reg);
extern EnemyRegistry g_enemy_registry;

// ── Dungeon map initialiser ───────────────────────────────────────────────────
// Replaces map_init for dungeon rooms — no hardcoded overworld building.
// Only border walls, bottom edge open for south transitions.

void dungeon_map_init(Map* m) {
    // Pure open floor — no border walls
    // Room transitions fire when the player walks off the map edge (coordinate < 0
    // or > map size). Walls are added per-room by the map_make_* functions.
    for (int r = 0; r < MAP_ROWS; r++)
        for (int c = 0; c < MAP_COLS; c++)
            m->tiles[r][c] = TILE_FLOOR;
}


// Find a valid spawn position — scan outward from target until open floor found
static void find_open_spawn(Map* m, float target_x, float target_y,
                             float* out_x, float* out_y,
                             int player_w, int player_h) {
    // Try expanding rings around the target until we find open space
    int tc = (int)target_x / TILE_SIZE;
    int tr = (int)target_y / TILE_SIZE;
    for (int radius = 0; radius < 20; radius++) {
        for (int dr = -radius; dr <= radius; dr++) {
            for (int dc = -radius; dc <= radius; dc++) {
                if (abs(dr) != radius && abs(dc) != radius) continue;
                int c = tc + dc;
                int r = tr + dr;
                if (c < 1 || c >= MAP_COLS-1) continue;
                if (r < 1 || r >= MAP_ROWS-1) continue;
                // Check player-sized area
                int clear = 1;
                for (int pr = 0; pr < (player_h/TILE_SIZE)+1 && clear; pr++)
                    for (int pc = 0; pc < (player_w/TILE_SIZE)+1 && clear; pc++)
                        if (map_is_wall(m, (c+pc)*TILE_SIZE, (r+pr)*TILE_SIZE))
                            clear = 0;
                if (clear) {
                    *out_x = (float)(c * TILE_SIZE + 2);
                    *out_y = (float)(r * TILE_SIZE + 2);
                    return;
                }
            }
        }
    }
    // Fallback — no open tile found near the target (usually because the
    // target was off-map). Use the map center, which is always in-bounds,
    // instead of the raw target, so the player can never spawn in the void.
    *out_x = (float)((MAP_COLS / 2) * TILE_SIZE + 2);
    *out_y = (float)((MAP_ROWS / 2) * TILE_SIZE + 2);
}

// ─── Forward declarations ─────────────────────────────────────────────────────


static void enter_room_body(DungeonSystem* ds_sys, int room_id,
                              float tx, float ty,
                              float* player_x, float* player_y);
static void enter_room(DungeonSystem* ds_sys, int room_id,
                       float* player_x, float* player_y);
static void enter_room_from(DungeonSystem* ds_sys, int room_id,
                             int from_dir,
                             float* player_x, float* player_y);
static void try_descend_stairs(DungeonSystem* ds_sys,
                               float* player_x, float* player_y);
static void try_ascend_stairs(DungeonSystem* ds_sys,
                              float* player_x, float* player_y);
static void render_minimap(DungeonSystem* ds_sys,
                            SDL_Renderer* renderer, TTF_Font* font);
static void update_enemies(DungeonSystem* ds_sys,
                            float player_x, float player_y);

// ─── Map builders ─────────────────────────────────────────────────────────────
// Each room type gets a distinct map layout so they feel different.
// All rendering-agnostic — just tile data.

// ─── Init ─────────────────────────────────────────────────────────────────────

void dungeon_init(DungeonSystem* ds_sys,
                  Party* party, Inventory* inv, GameState* gs,
                  TimeSystem* ts, NotifySystem* notify,
                  SkillDB* skilldb, ItemDB* itemdb, EquipDB* equipdb,
                  DialogueSystem* ds, CombatSystem* cs,
                  DungeonDB* dungeon_db) {
    memset(ds_sys, 0, sizeof(DungeonSystem));
    ds_sys->party              = party;
    ds_sys->inventory          = inv;
    ds_sys->gs                 = gs;
    ds_sys->ts                 = ts;
    ds_sys->notify             = notify;
    ds_sys->skilldb            = skilldb;
    ds_sys->itemdb             = itemdb;
    ds_sys->equipdb            = equipdb;
    ds_sys->ds                 = ds;
    ds_sys->cs                 = cs;
    ds_sys->dungeon_db          = dungeon_db;
    ds_sys->ambush         = 0;
    ds_sys->alert_decay_counter = 0;
    ds_sys->boss_engaged_this_visit = 0;
    ds_sys->current_dungeon_idx = 0;
    ds_sys->dungeon_count       = 0;

    ds_sys->dungeon_count = dungeon_db->count;
    ds_sys->current_dungeon_idx = 0;

    // Build the first available dungeon now so the struct is populated
    if (dungeon_db->count > 0)
        dungeon_db_build(dungeon_db, 0, &ds_sys->dungeon, &g_enemy_registry);
}

// ─── Dungeon reset ────────────────────────────────────────────────────────────
// Rebuilds dungeon from scratch — called before restoring a save so all
// dungeon state (enemies, chests, cleared rooms) starts from initial state.

void dungeon_reset_active(DungeonSystem* ds_sys) {
    int idx = ds_sys->current_dungeon_idx;
    if (idx < 0 || idx >= ds_sys->dungeon_count) return;
    dungeon_db_build(ds_sys->dungeon_db, idx, &ds_sys->dungeon, &g_enemy_registry);
}

// ─── Entry / Exit ─────────────────────────────────────────────────────────────

int dungeon_can_enter(DungeonSystem* ds_sys) {
    // Find the active dungeon from the DB
    int idx = dungeon_db_get_active(ds_sys->dungeon_db, ds_sys->gs);
    if (idx < 0) return 0;  // no dungeon available

    // Can only enter during Afternoon, After School, Evening, or Night
    TimeSlot slot = ds_sys->ts->slot;
    return (slot == TIMESLOT_AFTERNOON ||
            slot == TIMESLOT_AFTERSCHOOL ||
            slot == TIMESLOT_EVENING ||
            slot == TIMESLOT_NIGHT);
}

void dungeon_enter(DungeonSystem* ds_sys, float* player_x, float* player_y) {
    // Select which dungeon to enter and build it
    int idx = dungeon_db_get_active(ds_sys->dungeon_db, ds_sys->gs);
    if (idx < 0) return;
    if (idx != ds_sys->current_dungeon_idx) {
        // Different dungeon than what is loaded — rebuild
        ds_sys->current_dungeon_idx = idx;
        dungeon_db_build(ds_sys->dungeon_db, idx, &ds_sys->dungeon, &g_enemy_registry);
        ds_sys->alert_decay_counter = 0;
    ds_sys->boss_engaged_this_visit = 0;
    }

    Dungeon* p = &ds_sys->dungeon;

    // Reset all transient state
    p->active              = 1;
    p->current_floor       = 0;
    ds_sys->in_combat      = 0;
    ds_sys->combat_is_boss = 0;
    ds_sys->preemptive     = 0;
    ds_sys->ambush         = 0;
    ds_sys->safe_menu_open   = 0;
    ds_sys->fast_travel_open = 0;
    ds_sys->chest_open       = 0;
    ds_sys->transition_cooldown = 0;  // must be 0 so movement works immediately

    // Use enter_room so safe room restoration fires and spawn is set correctly
    enter_room(ds_sys, 0, player_x, player_y);

    notify_info(ds_sys->notify, "Entered the Hungerscape.");
}

void dungeon_exit(DungeonSystem* ds_sys, float* player_x, float* player_y) {
    Dungeon* p = dungeon_get_active(ds_sys);
    if (!p) return;
    p->active = 0;

    // Exiting costs one time slot
    timesystem_advance(ds_sys->ts);

    // Return player to overworld spawn
    *player_x = 200.0f;
    *player_y = 400.0f;

    notify_info(ds_sys->notify, "Left the Hungerscape.");
}

// ─── Helpers ──────────────────────────────────────────────────────────────────

Dungeon* dungeon_get_active(DungeonSystem* ds_sys) {
    if (!ds_sys || ds_sys->dungeon_count == 0) return NULL;
    return &ds_sys->dungeon;
}

DungeonFloor* dungeon_current_floor(DungeonSystem* ds_sys) {
    Dungeon* p = dungeon_get_active(ds_sys);
    if (!p || !p->active) return NULL;
    return &p->floors[p->current_floor];
}

DungeonRoom* dungeon_current_room(DungeonSystem* ds_sys) {
    DungeonFloor* f = dungeon_current_floor(ds_sys);
    if (!f) return NULL;
    return &f->rooms[f->current_room];
}

void dungeon_raise_alert(DungeonSystem* ds_sys, int amount) {
    Dungeon* p = dungeon_get_active(ds_sys);
    if (!p) return;
    p->alert_level += amount;
    audio_play_sfx(SFX_ALERT_RISE);
    if (p->alert_level > 100) p->alert_level = 100;

    char msg[64];
    snprintf(msg, 64, "Alert level: %d%%", p->alert_level);
    if (p->alert_level >= 80)
        notify_warning(ds_sys->notify, msg);
    else
        notify_info(ds_sys->notify, msg);
}

void dungeon_reset_alert(DungeonSystem* ds_sys) {
    if (dungeon_get_active(ds_sys)) dungeon_get_active(ds_sys)->alert_level = 0;
    notify_info(ds_sys->notify, "Alert level reset.");
}

// ─── Room transition ──────────────────────────────────────────────────────────

// Shared body used by both enter_room and enter_room_from
static void enter_room_body(DungeonSystem* ds_sys, int room_id,
                             float tx, float ty,
                             float* player_x, float* player_y) {
    DungeonFloor* f = dungeon_current_floor(ds_sys);
    if (!f || room_id < 0 || room_id >= f->room_count) return;

    // Find open spawn near target position
    {
        float sx, sy;
        find_open_spawn(&f->rooms[room_id].map, tx, ty, &sx, &sy, 32, 48);
        Dungeon* ap = dungeon_get_active(ds_sys);
        *player_x = ap ? (ap->player_x = sx) : sx;
        *player_y = ap ? (ap->player_y = sy) : sy;
    }
    f->current_room = room_id;
    DungeonRoom* r  = &f->rooms[room_id];

    // Reset boss engagement guard each room change
    ds_sys->boss_engaged_this_visit = 0;

    // First visit message
    if (!r->visited) {
        r->visited = 1;
        if (r->description[0])
            notify_info(ds_sys->notify, r->description);

        // If this is the final floor, trigger the team meeting
        // The meeting sets treasure_<flag>, which lets the boss appear
        Dungeon* vp = dungeon_get_active(ds_sys);
        DungeonFloor* vf = dungeon_current_floor(ds_sys);
        if (vp && vf && vf->stairs_room_id < 0) {
            char tflag[128];
            snprintf(tflag, 128, "treasure_%s", vp->clear_flag);
            if (!gamestate_get_flag(ds_sys->gs, tflag)) {
                gamestate_set_flag(ds_sys->gs, tflag, 1);
                // Trigger dungeon-specific team meeting dialogue
                Dungeon* tmp = dungeon_get_active(ds_sys);
                if (tmp && tmp->team_meeting_node >= 0 &&
                    !gamestate_get_flag(ds_sys->gs, tmp->team_meeting_flag))
                    dialogue_start(ds_sys->ds, tmp->team_meeting_node);
            }
        }
    }

    // Safe room: restore HP/SP immediately on entry
    if (r->type == ROOM_SAFE) {
        for (int i = 0; i < ds_sys->party->count; i++) {
            ds_sys->party->members[i].hp = ds_sys->party->members[i].max_hp;
            ds_sys->party->members[i].sp = ds_sys->party->members[i].max_sp;
        }
        notify_success(ds_sys->notify, "HP and SP fully restored!");
        audio_play_sfx(SFX_HEAL);
        audio_update_dungeon_bgm(0, 1);  // safe room BGM
        ds_sys->safe_menu_open   = 0;
        ds_sys->fast_travel_open = 0;
    } else {
        Dungeon* bgm_p = dungeon_get_active(ds_sys);
        audio_update_dungeon_bgm(bgm_p ? bgm_p->alert_level : 0, 0);
    }

    audio_play_sfx(SFX_ROOM_TRANSITION);
    ds_sys->transition_cooldown = 60;
}

static void enter_room(DungeonSystem* ds_sys, int room_id,
                        float* player_x, float* player_y) {
    // Default: spawn at the true map center (MAP is MAP_COLS x MAP_ROWS tiles;
    // the old 640,368 was the legacy SDL window center = only the quarter-map,
    // which dropped the player outside the room).
    enter_room_body(ds_sys, room_id,
                    MAP_COLS * TILE_SIZE / 2.0f, MAP_ROWS * TILE_SIZE / 2.0f,
                    player_x, player_y);
}

static void enter_room_from(DungeonSystem* ds_sys, int room_id,
                             int from_dir,
                             float* player_x, float* player_y) {
    // Spawn near the edge opposite to the direction we travelled
    int map_w = MAP_COLS * TILE_SIZE;
    int map_h = MAP_ROWS * TILE_SIZE;
    int edge  = 96;
    float cx = map_w / 2.0f, cy = map_h / 2.0f;   // true map center
    float tx = cx, ty = cy;
    switch (from_dir) {
        case EXIT_NORTH: tx = cx;            ty = map_h - edge; break;
        case EXIT_SOUTH: tx = cx;            ty = edge;         break;
        case EXIT_EAST:  tx = edge;          ty = cy;           break;
        case EXIT_WEST:  tx = map_w - edge;  ty = cy;           break;
        default: break;
    }
    enter_room_body(ds_sys, room_id, tx, ty, player_x, player_y);
}

static void try_descend_stairs(DungeonSystem* ds_sys,
                                float* player_x, float* player_y) {
    Dungeon*       p = dungeon_get_active(ds_sys);
    if (!p) return;
    DungeonFloor* f = dungeon_current_floor(ds_sys);
    if (!f) return;

    if (f->stairs_room_id < 0) {
        notify_info(ds_sys->notify, "This is the final floor.");
        return;
    }

    if (p->current_floor + 1 >= p->floor_count) {
        notify_info(ds_sys->notify, "No further descent.");
        return;
    }

    p->current_floor++;
    p->floors[p->current_floor].current_room = 0;
    notify_info(ds_sys->notify, p->floors[p->current_floor].name);
    enter_room(ds_sys, 0, player_x, player_y);
}

static void try_ascend_stairs(DungeonSystem* ds_sys,
                               float* player_x, float* player_y) {
    Dungeon* p = dungeon_get_active(ds_sys);
    if (!p) return;

    if (p->current_floor <= 0) {
        notify_info(ds_sys->notify, "This is the first floor.");
        return;
    }

    p->current_floor--;
    DungeonFloor* f = &p->floors[p->current_floor];
    // Return to the stairs room on the previous floor
    int stairs = f->stairs_room_id;
    if (stairs < 0 || stairs >= f->room_count) stairs = 0;
    f->current_room = stairs;
    notify_info(ds_sys->notify, f->name);
    enter_room(ds_sys, stairs, player_x, player_y);
}

// ─── Ambush / Preemptive roll ─────────────────────────────────────────────────
// Based on average party AG vs average enemy AG.
// Returns: 1 = preemptive (player advantage), -1 = ambush, 0 = normal

static int roll_encounter_type(DungeonSystem* ds_sys, EntityList* entities) {
    // Average party agility
    int party_ag = 0;
    for (int i = 0; i < ds_sys->party->count; i++)
        party_ag += ds_sys->party->members[i].ag;
    party_ag /= ds_sys->party->count;

    // Average enemy agility from first enemy group
    int enemy_ag = 10;
    for (int i = 0; i < entities->count; i++) {
        Entity* e = &entities->entities[i];
        if (e->type == ENTITY_ENEMY && e->active && !e->defeated) {
            int total = 0;
            for (int j = 0; j < e->group_size; j++)
                total += e->group[j].ag;
            if (e->group_size > 0) enemy_ag = total / e->group_size;
            break;
        }
    }

    int roll = rand() % 100;
    // Higher party AG = more likely preemptive, less likely ambush
    int preemptive_chance = 15 + (party_ag - enemy_ag) * 3;
    int ambush_chance     = 15 - (party_ag - enemy_ag) * 3;
    if (preemptive_chance < 5)  preemptive_chance = 5;
    if (preemptive_chance > 40) preemptive_chance = 40;
    if (ambush_chance < 5)      ambush_chance = 5;
    if (ambush_chance > 40)     ambush_chance = 40;

    if (roll < preemptive_chance) return  1;
    if (roll < preemptive_chance + ambush_chance) return -1;
    return 0;
}

// ─── Combat trigger ───────────────────────────────────────────────────────────

static void trigger_combat(DungeonSystem* ds_sys, Entity* e,
                            float player_x, float player_y) {
    CombatSystem* cs = ds_sys->cs;
    DungeonRoom*  r  = dungeon_current_room(ds_sys);
    if (!r) return;

    // Build combat enemy group
    CombatEnemy group[MAX_GROUP_SIZE];
    for (int j = 0; j < e->group_size; j++) {
        EnemyTemplate* t = &e->group[j];
        group[j] = make_combat_enemy(
            t->name, t->hp, t->sp,
            t->st, t->ma, t->en, t->ag, t->lu,
            t->exp_reward, e->win_flag);
        for (int k = 0; k < ELEMENT_COUNT; k++)
            statblock_set_affinity(&group[j].stats, k, t->affinities[k]);
        group[j].stats.skill_count = t->skill_count;
        for (int k = 0; k < t->skill_count; k++)
            group[j].stats.skill_ids[k] = t->skill_ids[k];
        group[j].loot_count = t->loot_count;
        for (int k = 0; k < t->loot_count; k++) {
            group[j].loot_ids[k]     = t->loot_ids[k];
            group[j].loot_chances[k] = t->loot_chances[k];
        }
    }

    // Determine encounter type
    int enc = roll_encounter_type(ds_sys, &r->entities);
    ds_sys->preemptive = (enc ==  1);
    ds_sys->ambush     = (enc == -1);

    if (ds_sys->preemptive) {
        notify_success(ds_sys->notify, "Preemptive strike!");
        // All enemies start knocked down
        for (int j = 0; j < e->group_size; j++)
            group[j].stats.knocked_down = 1;
    } else if (ds_sys->ambush) {
        notify_warning(ds_sys->notify, "Ambushed!");
        dungeon_raise_alert(ds_sys, 30);
    }

    e->pre_combat_x = player_x;
    e->pre_combat_y = player_y;
    ds_sys->in_combat       = 1;
    ds_sys->combat_is_boss  = 0;

    combat_start(cs, group, e->group_size, e->win_flag);
}

static void trigger_boss(DungeonSystem* ds_sys, float player_x,
                          float player_y) {
    DungeonRoom* r = dungeon_current_room(ds_sys);
    if (!r || r->type != ROOM_BOSS) return;
    if (r->boss.defeated) return;

    BossConfig* boss = &r->boss;

    int pre_node = boss->pre_battle_node;
    if (pre_node >= 0)
        dialogue_start(ds_sys->ds, pre_node);

    // Combat starts after dialogue via gamestate flag
    gamestate_set_flag(ds_sys->gs, "boss_dialogue_done", 0);
    ds_sys->in_combat      = 1;
    ds_sys->combat_is_boss  = 1;
    gamestate_set_flag(ds_sys->gs, "in_boss_combat", 1);

    audio_start_combat_bgm(1, 0);
    CombatEnemy be = boss->enemy;
    combat_start(ds_sys->cs, &be, 1, "defeated_boss");
}

// ─── On combat end ────────────────────────────────────────────────────────────

void dungeon_on_combat_end(DungeonSystem* ds_sys, int player_won) {
    if (!player_won) {
        ds_sys->in_combat = 0;
        // Expelled from dungeon on defeat — handled by main.c
        return;
    }

    DungeonRoom* r = dungeon_current_room(ds_sys);
    ds_sys->in_combat = 0;

    if (ds_sys->combat_is_boss) {
        r->boss.defeated = 1;
        r->cleared       = 1;
        Dungeon* ap = dungeon_get_active(ds_sys);
        if (ap) gamestate_set_flag(ds_sys->gs, ap->clear_flag, 1);
        notify_success(ds_sys->notify, "The Shadow has fallen!");

        // Give dungeon treasure from BossConfig
        if (r->boss.treasure_equip_id >= 0) {
            EquipmentPiece* ep = equip_get(ds_sys->equipdb,
                                            r->boss.treasure_equip_id);
            if (ep) {
                equip_set(ds_sys->equipdb,
                          &ds_sys->party->members[0],
                          ep->slot, r->boss.treasure_equip_id);
                char tmsg[80];
                snprintf(tmsg, 80, "Obtained the Dungeon Treasure: %s!", ep->name);
                notify_success(ds_sys->notify, tmsg);
            }
        } else if (r->boss.treasure_item_id >= 0) {
            inventory_add(ds_sys->inventory,
                          r->boss.treasure_item_id,
                          r->boss.treasure_item_qty);
            notify_success(ds_sys->notify, "Obtained the Dungeon Treasure!");
        }

        // Long cooldown after boss fight — player watches dialogue,
        // then the dungeon_cleared_exit flag fires to exit cleanly
        ds_sys->transition_cooldown = 300;

        // Start post-battle dialogue
        if (r->boss.post_battle_node >= 0)
            dialogue_start(ds_sys->ds, r->boss.post_battle_node);
        return;
    }

    // Normal combat — raise alert, mark room cleared
    dungeon_raise_alert(ds_sys, 10);
    // Cooldown so player doesn't immediately re-enter combat
    ds_sys->transition_cooldown = 60;

    if (r) {
        for (int i = 0; i < r->entities.count; i++) {
            Entity* e = &r->entities.entities[i];
            if (e->type == ENTITY_ENEMY) e->defeated = 1;
        }
        r->cleared = 1;
    }
}

// ─── Boss phase check ─────────────────────────────────────────────────────────
// Called from dungeon_update each frame while in boss combat.

static void boss_check_phase(DungeonSystem* ds_sys) {
    DungeonRoom* r = dungeon_current_room(ds_sys);
    if (!r || r->type != ROOM_BOSS) return;

    BossConfig*   boss = &r->boss;
    CombatSystem* cs   = ds_sys->cs;
    if (!cs->active || boss->in_phase2 || boss->defeated) return;
    if (boss->phase2_skill_count == 0 && boss->phase2_immune_elem < 0) return;

    for (int i = 0; i < cs->enemy_count; i++) {
        StatBlock* sb = &cs->enemies[i].stats;
        if (strcmp(sb->name, boss->enemy.stats.name) != 0) continue;
        if (!sb->alive) continue;

        int hp_pct = (sb->hp * 100) / sb->max_hp;
        if (hp_pct <= boss->phase2_hp_pct) {
            boss->in_phase2 = 1;

            sb->skill_count = boss->phase2_skill_count;
            for (int k = 0; k < boss->phase2_skill_count; k++)
                sb->skill_ids[k] = boss->phase2_skill_ids[k];

            if (boss->phase2_immune_elem >= 0)
                statblock_set_affinity(sb, boss->phase2_immune_elem, AFF_NULL);

            char msg[80];
            snprintf(msg, 80, "%s enters Phase 2!", sb->name);
            notify_warning(ds_sys->notify, msg);

            char log[80];
            snprintf(log, 80, "%s's power surges!", sb->name);
            combat_log(cs, log);
        }
        break;
    }
}

// ─── Chest interaction ────────────────────────────────────────────────────────

static void open_chest(DungeonSystem* ds_sys) {
    DungeonRoom* r = dungeon_current_room(ds_sys);
    if (!r || r->type != ROOM_CHEST) return;
    if (r->chest.opened) {
        notify_info(ds_sys->notify, "This chest is empty.");
        return;
    }

    r->chest.opened = 1;
    audio_play_sfx(SFX_CHEST_OPEN);

    if (r->chest.is_dungeon_treasure) {
        // Dungeon treasure chests are only opened after boss defeat
        // via dungeon_on_combat_end — not directly by the player
        notify_info(ds_sys->notify,
            "A powerful aura surrounds this treasure...");
        notify_info(ds_sys->notify,
            "Defeat the ruler first to claim it.");
        return;  // Don't open it
    }

    if (r->chest.equip_id >= 0) {
        EquipmentPiece* ep = equip_get(ds_sys->equipdb, r->chest.equip_id);
        if (ep) {
            // Give to first party member's appropriate slot
            equip_set(ds_sys->equipdb,
                      &ds_sys->party->members[0],
                      ep->slot, r->chest.equip_id);
            char msg[80];
            snprintf(msg, 80, "Obtained %s!", ep->name);
            notify_success(ds_sys->notify, msg);
        }
    } else if (r->chest.item_id >= 0) {
        inventory_add(ds_sys->inventory,
                      r->chest.item_id, r->chest.item_qty);
        Item* item = itemdb_get(ds_sys->itemdb, r->chest.item_id);
        if (item) {
            char msg[80];
            snprintf(msg, 80, "Found %s x%d!", item->name, r->chest.item_qty);
            notify_success(ds_sys->notify, msg);
        }
    }

    // Quest hook: if this chest is tagged with a flag, set it now.
    if (r->chest.sets_flag[0]) {
        gamestate_set_flag(ds_sys->gs, r->chest.sets_flag, 1);
    }
}



// ─── Enemy movement ───────────────────────────────────────────────────────────
// Enemies have two states: patrol (wandering on a fixed path) and chase
// (moving toward the player when within detection radius).
// This is pure game logic — no rendering. Port unchanged to 3D.

#define ENEMY_DETECT_RADIUS  200.0f   // pixels — enemy notices player
#define ENEMY_CHASE_SPEED      1.5f   // pixels per frame when chasing
#define ENEMY_PATROL_SPEED     0.6f   // pixels per frame when patrolling
#define ENEMY_PATROL_RANGE    80.0f   // how far from spawn point to patrol

static void update_enemies(DungeonSystem* ds_sys,
                            float player_x, float player_y) {
    DungeonRoom* r = dungeon_current_room(ds_sys);
    if (!r || r->type != ROOM_NORMAL || r->cleared) return;

    for (int i = 0; i < r->entities.count; i++) {
        Entity* e = &r->entities.entities[i];
        if (e->type != ENTITY_ENEMY || !e->active || e->defeated) continue;

        float dx = (player_x + 16) - (e->x + e->width  / 2);
        float dy = (player_y + 24) - (e->y + e->height / 2);
        float dist2 = dx*dx + dy*dy;
        float dist  = 0.0f;

        // Simple sqrt approximation for normalisation
        if (dist2 > 0.0f) {
            // Newton-Raphson sqrt approximation (good enough for AI)
            float estimate = dist2 * 0.5f;
            float x2 = dist2 * 0.5f;
            long  i2 = 0x5f3759df - (*(long*)&x2 >> 1);
            float y2 = *(float*)&i2;
            y2 = y2 * (1.5f - (x2 * y2 * y2));
            dist = 1.0f / y2;
        }

        if (dist < ENEMY_DETECT_RADIUS && dist > 0.0f) {
            // Chase mode — move toward player
            float speed = ENEMY_CHASE_SPEED;
            float nx = e->x + (dx / dist) * speed;
            float ny = e->y + (dy / dist) * speed;

            // Wall collision before moving
            Map* m = &r->map;
            if (!map_is_wall(m, (int)(nx + 4), (int)(e->y + 4)) &&
                !map_is_wall(m, (int)(nx + e->width - 4), (int)(e->y + 4)) &&
                !map_is_wall(m, (int)(nx + 4), (int)(e->y + e->height - 4)) &&
                !map_is_wall(m, (int)(nx + e->width - 4), (int)(e->y + e->height - 4)))
                e->x = nx;

            if (!map_is_wall(m, (int)(e->x + 4), (int)(ny + 4)) &&
                !map_is_wall(m, (int)(e->x + e->width - 4), (int)(ny + 4)) &&
                !map_is_wall(m, (int)(e->x + 4), (int)(ny + e->height - 4)) &&
                !map_is_wall(m, (int)(e->x + e->width - 4), (int)(ny + e->height - 4)))
                e->y = ny;

        } else {
            // Patrol mode — oscillate around spawn point using entity index as phase
            float phase = (float)(i * 60);  // stagger patrol phase per enemy
            float t = SDL_GetTicks() / 1000.0f + phase;
            float patrol_x = e->pre_combat_x + SDL_sinf(t * 0.8f) * ENEMY_PATROL_RANGE;
            float patrol_y = e->pre_combat_y + SDL_cosf(t * 0.5f) * (ENEMY_PATROL_RANGE * 0.5f);

            // Move toward patrol point
            float ptx = patrol_x - e->x;
            float pty = patrol_y - e->y;
            float pd = ptx*ptx + pty*pty;
            if (pd > 1.0f) {
                float plen = pd * 0.5f;
                long  pi2  = 0x5f3759df - (*(long*)&plen >> 1);
                float py2  = *(float*)&pi2;
                py2 = py2 * (1.5f - (plen * py2 * py2));
                float pspeed = ENEMY_PATROL_SPEED;
                float pnx = e->x + (ptx * py2) * pspeed;
                float pny = e->y + (pty * py2) * pspeed;
                Map* m = &r->map;
                if (!map_is_wall(m, (int)(pnx + 4), (int)(e->y + 4)) &&
                    !map_is_wall(m, (int)(pnx + e->width - 4), (int)(e->y + e->height - 4)))
                    e->x = pnx;
                if (!map_is_wall(m, (int)(e->x + 4), (int)(pny + 4)) &&
                    !map_is_wall(m, (int)(e->x + e->width - 4), (int)(pny + e->height - 4)))
                    e->y = pny;
            }
        }
    }
}

// ─── Update ───────────────────────────────────────────────────────────────────

void dungeon_update(DungeonSystem* ds_sys, float player_x, float player_y) {
    Dungeon* p = dungeon_get_active(ds_sys);
    if (!p || !p->active) return;

    // Alert decay — slowly reduces alert level over time when not in combat.
    // Roughly -1 every 5 seconds at 60 FPS = 300 frames.
    if (!ds_sys->in_combat && p->alert_level > 0) {
        ds_sys->alert_decay_counter++;
        if (ds_sys->alert_decay_counter >= 300) {
            ds_sys->alert_decay_counter = 0;
            p->alert_level--;
            if (p->alert_level < 0) p->alert_level = 0;
        }
    }

    // Alert = 100 — overwhelmed, game over
    if (p->alert_level >= 100) {
        notify_warning(ds_sys->notify,
                       "Alert maxed! Shadows overwhelm the party!");
        gamestate_set_flag(ds_sys->gs, "dungeon_game_over", 1);
        dungeon_exit(ds_sys, &(float){0}, &(float){0});
        return;
    }

    // Deadline passed without clearing
    if (ds_sys->ts->day >= p->deadline_day &&
        !gamestate_get_flag(ds_sys->gs, p->clear_flag)) {
        notify_warning(ds_sys->notify,
                       "The deadline has passed! It's too late...");
        gamestate_set_flag(ds_sys->gs, "dungeon_game_over", 1);
        return;
    }

    if (ds_sys->transition_cooldown > 0) {
        ds_sys->transition_cooldown--;
        return;
    }

    if (ds_sys->in_combat) {
        // Boss phase check runs every frame during boss fight
        if (ds_sys->combat_is_boss) boss_check_phase(ds_sys);
        return;
    }

    // Post-boss: once dialogue finishes, spike alert and auto-exit
    if (gamestate_get_flag(ds_sys->gs, "boss_post_dialogue_done") &&
        !ds_sys->ds->active) {
        gamestate_set_flag(ds_sys->gs, "boss_post_dialogue_done", 0);
        p->alert_level = 100;
        notify_warning(ds_sys->notify,
            "The dungeon is collapsing! Escaping...");
        // Set a flag — main.c will call dungeon_exit and reset player pos
        gamestate_set_flag(ds_sys->gs, "dungeon_cleared_exit", 1);
        // Restore HP/SP as reward
        for (int i = 0; i < ds_sys->party->count; i++) {
            ds_sys->party->members[i].hp = ds_sys->party->members[i].max_hp;
            ds_sys->party->members[i].sp = ds_sys->party->members[i].max_sp;
        }
        return;
    }

    DungeonRoom* r = dungeon_current_room(ds_sys);
    if (!r) return;

    // Save current player pos
    p->player_x = player_x;
    p->player_y = player_y;

    // Move enemies each frame
    update_enemies(ds_sys, player_x, player_y);

    // Check for enemy proximity (normal rooms only)
    if (r->type == ROOM_NORMAL && !r->cleared) {
        for (int i = 0; i < r->entities.count; i++) {
            Entity* e = &r->entities.entities[i];
            if (e->type != ENTITY_ENEMY || !e->active || e->defeated)
                continue;

            float dx = (player_x + 16) - (e->x + 16);
            float dy = (player_y + 24) - (e->y + 16);
            float dist2 = dx*dx + dy*dy;
            if (dist2 < 48.0f * 48.0f) {
                fprintf(stderr, "TRIGGER_COMBAT: room_type=%d room_name='%s' entity_x=%.1f entity_y=%.1f\n",
                r->type, r->name, e->x, e->y);
                trigger_combat(ds_sys, e, player_x, player_y);
                return;
            }
        }
    }

    // Boss room — trigger combat on entry once the boss has appeared.
    // Player just needs to be in the room; no positional check.
    if (r->type == ROOM_BOSS && !r->boss.defeated) {
        if (!boss_ready(ds_sys)) {
            // Boss hasn't appeared yet: throne room is empty. Free to scout.
            return;
        }
        // Boss is here: trigger combat immediately on room entry.
        // Guard with a one-shot flag so it only fires once per visit.
        if (!ds_sys->boss_engaged_this_visit) {
            ds_sys->boss_engaged_this_visit = 1;
            trigger_boss(ds_sys, player_x, player_y);
            return;
        }
    }
}

// ─── Input ────────────────────────────────────────────────────────────────────

void dungeon_handle_input(DungeonSystem* ds_sys, SDL_Event* event,
                          float* player_x, float* player_y) {
    Dungeon* p = dungeon_get_active(ds_sys);
    if (!p || !p->active) return;
    if (ds_sys->in_combat) return;
    if (ds_sys->transition_cooldown > 0) return;

    DungeonFloor* f = dungeon_current_floor(ds_sys);
    DungeonRoom*  r = dungeon_current_room(ds_sys);
    if (!f || !r) return;

    // ── Overlay menus eat all input ───────────────────────────────────────────
    if (ds_sys->safe_menu_open) {
        if (event->type != SDL_KEYDOWN) return;
        SDL_Keycode key = event->key.keysym.sym;
        switch (key) {
            case SDLK_UP:
                ds_sys->safe_menu_selected =
                    (ds_sys->safe_menu_selected - 1 + 4) % 4; break;
            case SDLK_DOWN:
                ds_sys->safe_menu_selected =
                    (ds_sys->safe_menu_selected + 1) % 4; break;
            case SDLK_ESCAPE:
                ds_sys->safe_menu_open = 0; break;
            case SDLK_e:
            case SDLK_RETURN:
                switch (ds_sys->safe_menu_selected) {
                    case 0: // Rest
                        notify_success(ds_sys->notify, "HP and SP restored.");
                        for (int i = 0; i < ds_sys->party->count; i++) {
                            ds_sys->party->members[i].hp =
                                ds_sys->party->members[i].max_hp;
                            ds_sys->party->members[i].sp =
                                ds_sys->party->members[i].max_sp;
                        }
                        ds_sys->safe_menu_open = 0;
                        break;
                    case 1: // Save
                        gamestate_set_flag(ds_sys->gs, "request_save", 1);
                        ds_sys->safe_menu_open = 0;
                        break;
                    case 2: // Load
                        gamestate_set_flag(ds_sys->gs, "request_load", 1);
                        ds_sys->safe_menu_open = 0;
                        break;
                    case 3: // Exit Dungeon
                        ds_sys->safe_menu_open = 0;
                        dungeon_exit(ds_sys, player_x, player_y);
                        break;
                }
                break;
        }
        return;
    }

    if (ds_sys->fast_travel_open) {
        if (event->type != SDL_KEYDOWN) return;
        SDL_Keycode key = event->key.keysym.sym;

        // Build list of all visited safe rooms across all floors
        // (done inline since we need count for wrap-around)
        int total_safe = 0;
        for (int fl = 0; fl < p->floor_count; fl++)
            for (int rm = 0; rm < p->floors[fl].room_count; rm++)
                if (p->floors[fl].rooms[rm].type == ROOM_SAFE &&
                    p->floors[fl].rooms[rm].visited)
                    total_safe++;
        if (total_safe == 0) { ds_sys->fast_travel_open = 0; return; }

        switch (key) {
            case SDLK_UP:
                ds_sys->fast_travel_selected =
                    (ds_sys->fast_travel_selected - 1 + total_safe)
                    % total_safe; break;
            case SDLK_DOWN:
                ds_sys->fast_travel_selected =
                    (ds_sys->fast_travel_selected + 1) % total_safe; break;
            case SDLK_ESCAPE:
                ds_sys->fast_travel_open = 0; break;
            case SDLK_e:
            case SDLK_RETURN: {
                // Find the Nth visited safe room
                int idx = 0;
                int traveled = 0;
                for (int fl = 0; fl < p->floor_count && !traveled; fl++) {
                    for (int rm = 0; rm < p->floors[fl].room_count && !traveled; rm++) {
                        if (p->floors[fl].rooms[rm].type == ROOM_SAFE &&
                            p->floors[fl].rooms[rm].visited) {
                            if (idx == ds_sys->fast_travel_selected) {
                                // Update floor and room before entering
                                p->current_floor = fl;
                                p->floors[fl].current_room = rm;
                                // Clear cooldown so movement works immediately
                                ds_sys->transition_cooldown = 0;
                                enter_room(ds_sys, rm, player_x, player_y);
                                char msg[64];
                                snprintf(msg, 64, "Traveled to %s (Floor %d)",
                                         p->floors[fl].rooms[rm].name,
                                         p->floors[fl].floor_number);
                                notify_success(ds_sys->notify, msg);
                                traveled = 1;
                            }
                            idx++;
                        }
                    }
                }
                if (!traveled)
                    notify_info(ds_sys->notify, "Already here.");
                ds_sys->fast_travel_open = 0;
                break;
            }
        }
        return;
    }

    // ── Keyboard events ───────────────────────────────────────────────────────
    if (event->type == SDL_KEYDOWN) {
        SDL_Keycode key = event->key.keysym.sym;

        // E: interact
        if (key == SDLK_e || key == SDLK_RETURN) {
            if (r->type == ROOM_SAFE) {
                ds_sys->safe_menu_open     = 1;
                ds_sys->safe_menu_selected = 0;
            } else if (r->type == ROOM_CHEST) {
                open_chest(ds_sys);
            } else if (r->id == f->stairs_room_id) {
                // If on floor > 0, ask direction; otherwise just descend
                Dungeon* sp = dungeon_get_active(ds_sys);
                if (sp && sp->current_floor > 0 && f->stairs_room_id >= 0) {
                    // Show ascend option — for now default to descend,
                    // use Shift or second press to ascend
                    // Simple: E = descend to next, Tab in stairs room = ascend
                    try_descend_stairs(ds_sys, player_x, player_y);
                } else {
                    try_descend_stairs(ds_sys, player_x, player_y);
                }
            }
            return;
        }

        // T: fast travel — available in any safe room
        // Shows all visited safe rooms across the entire dungeon
        if (key == SDLK_t && r->type == ROOM_SAFE) {
            // Count visited safe rooms across all floors
            int total = 0;
            for (int fl = 0; fl < p->floor_count; fl++)
                for (int rm = 0; rm < p->floors[fl].room_count; rm++)
                    if (p->floors[fl].rooms[rm].type == ROOM_SAFE &&
                        p->floors[fl].rooms[rm].visited)
                        total++;
            if (total > 0) {
                ds_sys->fast_travel_open     = 1;
                ds_sys->fast_travel_selected = 0;
            } else {
                notify_info(ds_sys->notify, "No other safe rooms discovered yet.");
            }
            return;
        }

        // U: ascend stairs (any room on floors > 0)
        if (key == SDLK_u) {
            Dungeon* up = dungeon_get_active(ds_sys);
            if (up && up->current_floor > 0)
                try_ascend_stairs(ds_sys, player_x, player_y);
            else
                notify_info(ds_sys->notify, "You are on the first floor.");
            return;
        }

        // Escape: open safe room menu if in safe room, else handled by main
        if (key == SDLK_ESCAPE && r->type == ROOM_SAFE) {
            ds_sys->safe_menu_open     = 1;
            ds_sys->safe_menu_selected = 2; // pre-select Exit
            return;
        }
    }
}

// ── Dungeon movement — called every frame from main when dungeon is active ─────
// Mirrors the overworld movement logic exactly.
// Room transitions fire when the player walks to the map boundary.
void dungeon_move(DungeonSystem* ds_sys, const Uint8* keys,
                  float* player_x, float* player_y,
                  int player_w, int player_h) {
    { Dungeon* _ap = dungeon_get_active(ds_sys); if (!_ap || !_ap->active) return; }
    if (ds_sys->in_combat) return;
    if (ds_sys->transition_cooldown > 0) return;
    if (ds_sys->safe_menu_open || ds_sys->fast_travel_open) return;

    DungeonFloor* f = dungeon_current_floor(ds_sys);
    DungeonRoom*  r = dungeon_current_room(ds_sys);
    if (!f || !r) return;

    float nx = *player_x;
    float ny = *player_y;

    if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])    ny -= DUNGEON_SPEED;
    if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])  ny += DUNGEON_SPEED;
    if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])  nx -= DUNGEON_SPEED;
    if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) nx += DUNGEON_SPEED;

    if (nx == *player_x && ny == *player_y) return;

    // Check map boundary transitions — fire when player enters edge zone (32px)
    int map_w = MAP_COLS * TILE_SIZE;
    int map_h = MAP_ROWS * TILE_SIZE;
    int edge  = 32;

    if (ny < edge && r->exits[EXIT_NORTH] >= 0) {
        enter_room_from(ds_sys, r->exits[EXIT_NORTH], EXIT_NORTH, player_x, player_y);
        return;
    }
    if (ny + player_h > map_h - edge && r->exits[EXIT_SOUTH] >= 0) {
        enter_room_from(ds_sys, r->exits[EXIT_SOUTH], EXIT_SOUTH, player_x, player_y);
        return;
    }
    if (nx < edge && r->exits[EXIT_WEST] >= 0) {
        enter_room_from(ds_sys, r->exits[EXIT_WEST], EXIT_WEST, player_x, player_y);
        return;
    }
    if (nx + player_w > map_w - edge && r->exits[EXIT_EAST] >= 0) {
        enter_room_from(ds_sys, r->exits[EXIT_EAST], EXIT_EAST, player_x, player_y);
        return;
    }

    // Wall collision — same 5-point check as overworld
    int can_move = 1;
    for (int i = 0; i <= 4; i++) {
        float t = (float)i / 4.0f;
        if (map_is_wall(&r->map, (int)(nx + t*(player_w-1)), (int)ny) ||
            map_is_wall(&r->map, (int)(nx + t*(player_w-1)), (int)(ny+player_h-1)) ||
            map_is_wall(&r->map, (int)nx,                    (int)(ny + t*(player_h-1))) ||
            map_is_wall(&r->map, (int)(nx+player_w-1),       (int)(ny + t*(player_h-1)))) {
            can_move = 0;
            break;
        }
    }

    if (can_move) {
        *player_x = nx;
        *player_y = ny;
        { Dungeon* _ap2 = dungeon_get_active(ds_sys); if (_ap2) { _ap2->player_x = nx; _ap2->player_y = ny; } }
    }
}

// Godot-port helper: check if the player has walked into an edge zone and, if
// so, perform the room transition. Returns 1 if a transition fired.
int dungeon_check_edge_transition(DungeonSystem* ds_sys,
                                  float* player_x, float* player_y,
                                  int player_w, int player_h) {
    { Dungeon* _ap = dungeon_get_active(ds_sys); if (!_ap || !_ap->active) return 0; }
    if (ds_sys->in_combat) return 0;
    if (ds_sys->transition_cooldown > 0) return 0;
    if (ds_sys->safe_menu_open || ds_sys->fast_travel_open) return 0;

    DungeonRoom* r = dungeon_current_room(ds_sys);
    if (!r) return 0;

    float nx = *player_x;
    float ny = *player_y;
    int map_w = MAP_COLS * TILE_SIZE;
    int map_h = MAP_ROWS * TILE_SIZE;
    int edge  = 32;

    // Doorway must match the Godot visual gap: door_half = 3 tiles each side of
    // center => span of 7 tiles. Center of map in pixels:
    float cx = map_w / 2.0f;
    float cy = map_h / 2.0f;
    float door_span = 3.5f * TILE_SIZE;  // ~half the 7-tile gap

    float px_center = nx + player_w / 2.0f;
    float py_center = ny + player_h / 2.0f;

    if (ny < edge && r->exits[EXIT_NORTH] >= 0 &&
        fabsf(px_center - cx) <= door_span) {
        enter_room_from(ds_sys, r->exits[EXIT_NORTH], EXIT_NORTH, player_x, player_y);
        return 1;
    }
    if (ny + player_h > map_h - edge && r->exits[EXIT_SOUTH] >= 0 &&
        fabsf(px_center - cx) <= door_span) {
        enter_room_from(ds_sys, r->exits[EXIT_SOUTH], EXIT_SOUTH, player_x, player_y);
        return 1;
    }
    if (nx < edge && r->exits[EXIT_WEST] >= 0 &&
        fabsf(py_center - cy) <= door_span) {
        enter_room_from(ds_sys, r->exits[EXIT_WEST], EXIT_WEST, player_x, player_y);
        return 1;
    }
    if (nx + player_w > map_w - edge && r->exits[EXIT_EAST] >= 0 &&
        fabsf(py_center - cy) <= door_span) {
        enter_room_from(ds_sys, r->exits[EXIT_EAST], EXIT_EAST, player_x, player_y);
        return 1;
    }
    return 0;
}

// ─── Render helpers (SDL2 only — replace entire dungeon_render for 3D port) ──

static void draw_text_d(SDL_Renderer* r, TTF_Font* font,
                        const char* text, SDL_Color color, int x, int y) {
    if (!text || !text[0]) return;
    SDL_Surface* surf = TTF_RenderText_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_Rect dst = { x, y, surf->w, surf->h };
    SDL_RenderCopy(r, tex, NULL, &dst);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

static void draw_rect_d(SDL_Renderer* r, int x, int y, int w, int h,
                        Uint8 rr, Uint8 g, Uint8 b, Uint8 a) {
    SDL_SetRenderDrawColor(r, rr, g, b, a);
    SDL_Rect rect = { x, y, w, h };
    SDL_RenderFillRect(r, &rect);
}

static void draw_border_d(SDL_Renderer* r, int x, int y, int w, int h,
                          Uint8 rr, Uint8 g, Uint8 b, Uint8 a) {
    SDL_SetRenderDrawColor(r, rr, g, b, a);
    SDL_Rect rect = { x, y, w, h };
    SDL_RenderDrawRect(r, &rect);
}

void dungeon_render(DungeonSystem* ds_sys, SDL_Renderer* renderer,
                    TTF_Font* font, float player_x, float player_y,
                    Camera* cam) {
    Dungeon*       p = dungeon_get_active(ds_sys);
    DungeonRoom*  r = dungeon_current_room(ds_sys);
    DungeonFloor* f = dungeon_current_floor(ds_sys);
    if (!p || !p->active || !r || !f) return;

    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color yellow = {255, 220, 100, 255};
    SDL_Color red    = {220,  60,  60, 255};
    SDL_Color gray   = {150, 150, 180, 255};
    SDL_Color green  = {100, 220, 100, 255};
    SDL_Color cyan   = { 80, 200, 220, 255};

    // Background tint depends on room type
    switch (r->type) {
        case ROOM_SAFE:
            SDL_SetRenderDrawColor(renderer, 10, 10, 40, 255); break;
        case ROOM_BOSS:
            SDL_SetRenderDrawColor(renderer, 30, 5,  5,  255); break;
        case ROOM_CHEST:
            SDL_SetRenderDrawColor(renderer, 15, 20, 10, 255); break;
        default:
            SDL_SetRenderDrawColor(renderer, 15, 10, 25, 255); break;
    }
    SDL_RenderClear(renderer);

    // Tile map
    map_render(renderer, &r->map, cam);

    // Entities (enemies as red, chest as gold, boss as deep red)
    if (r->type == ROOM_NORMAL || r->type == ROOM_BOSS) {
        if (r->type == ROOM_BOSS && !r->boss.defeated) {
            if (boss_ready(ds_sys)) {
                // Boss has manifested
                SDL_SetRenderDrawColor(renderer, 180, 20, 20, 255);
                SDL_Rect brect = {
                    (int)(640 - cam->x - 24), (int)(450 - cam->y - 32), 48, 64
                };
                SDL_RenderFillRect(renderer, &brect);
                SDL_Color boss_col = {255, 80, 80, 255};
                draw_text_d(renderer, font, "Shadow King", boss_col,
                            (int)(620 - cam->x), (int)(410 - cam->y));

                // Treasure glowing object — above the boss
                SDL_SetRenderDrawColor(renderer, 255, 200, 50, 200);
                SDL_Rect trect = {
                    (int)(640 - cam->x - 12), (int)(370 - cam->y), 24, 24
                };
                SDL_RenderFillRect(renderer, &trect);
                SDL_Color trcol = {255, 240, 120, 255};
                draw_text_d(renderer, font, "Treasure", trcol,
                            (int)(620 - cam->x), (int)(345 - cam->y));
            } else {
                // Empty throne room — show empty throne and atmosphere hint
                SDL_SetRenderDrawColor(renderer, 60, 50, 80, 255);
                SDL_Rect throne = {
                    (int)(620 - cam->x), (int)(430 - cam->y), 40, 50
                };
                SDL_RenderFillRect(renderer, &throne);
                SDL_Color hint_col = {140, 120, 180, 255};
                draw_text_d(renderer, font,
                    "An empty throne...", hint_col,
                    (int)(560 - cam->x), (int)(390 - cam->y));
            }
        } else {
            for (int i = 0; i < r->entities.count; i++) {
                Entity* e = &r->entities.entities[i];
                if (!e->active || e->defeated) continue;
                if (e->type != ENTITY_ENEMY) continue;

                int ex2 = (int)(e->x - cam->x);
                int ey2 = (int)(e->y - cam->y);

                // Enemy body — color varies by group type
                Uint8 er = 180 + (i * 20) % 60;
                Uint8 eg = 20  + (i * 10) % 40;
                Uint8 eb = 20  + (i * 15) % 40;
                SDL_SetRenderDrawColor(renderer, er, eg, eb, 255);
                SDL_Rect erect = { ex2, ey2, e->width, e->height };
                SDL_RenderFillRect(renderer, &erect);

                // Enemy border
                SDL_SetRenderDrawColor(renderer, 255, 80, 80, 255);
                SDL_RenderDrawRect(renderer, &erect);

                // Enemy name above
                SDL_Color enamecol = {255, 120, 120, 255};
                draw_text_d(renderer, font, e->name,
                            enamecol, ex2 - 10, ey2 - 20);

                // Exclamation mark when near player (alert radius 80px)
                float pdx = (e->x + e->width/2) - (player_x + 16);
                float pdy = (e->y + e->height/2) - (player_y + 24);
                float dist2 = pdx*pdx + pdy*pdy;
                if (dist2 < 80.0f * 80.0f) {
                    SDL_Color alert_col = {255, 255, 80, 255};
                    draw_text_d(renderer, font, "!",
                                alert_col, ex2 + e->width/2 - 4, ey2 - 36);
                }
            }
        }
    }

    if (r->type == ROOM_CHEST && !r->chest.opened) {
        // Gold chest box
        SDL_SetRenderDrawColor(renderer, 220, 180, 40, 255);
        SDL_Rect crect = {
            (int)(400 - cam->x), (int)(300 - cam->y), 32, 32
        };
        SDL_RenderFillRect(renderer, &crect);
        draw_text_d(renderer, font, "E: Open",
                    yellow, (int)(384 - cam->x), (int)(270 - cam->y));
    } else if (r->type == ROOM_CHEST && r->chest.opened) {
        // Empty chest
        SDL_SetRenderDrawColor(renderer, 80, 60, 10, 255);
        SDL_Rect crect = {
            (int)(400 - cam->x), (int)(300 - cam->y), 32, 32
        };
        SDL_RenderFillRect(renderer, &crect);
    }

    // Player
    SDL_SetRenderDrawColor(renderer, 255, 220, 100, 255);
    SDL_Rect prect = {
        (int)(player_x - cam->x), (int)(player_y - cam->y), 32, 48
    };
    SDL_RenderFillRect(renderer, &prect);

    // Stairs indicator
    if (r->id == f->stairs_room_id) {
        draw_text_d(renderer, font, "E: Descend", cyan,
                    (int)(380 - cam->x), (int)(250 - cam->y));
    }
    // Always show ascend hint when above floor 1
    if (p->current_floor > 0) {
        draw_text_d(renderer, font, "U: Go up a floor", cyan, 20, 620);
    }

    // ── Dungeon HUD (top strip) ───────────────────────────────────────────────
    draw_rect_d(renderer, 0, 0, 1280, 52, 0, 0, 0, 200);

    // Dungeon name + floor
    char floor_str[64];
    snprintf(floor_str, 64, "%s | %s | Floor %d",
             p->name, f->name, f->floor_number);
    draw_text_d(renderer, font, floor_str, yellow, 12, 8);

    // Room name
    draw_text_d(renderer, font, r->name, white, 12, 30);

    // Alert bar (right side) — compact layout to avoid overlap
    // Deadline sits to the right of the alert bar
    int days_left = p->deadline_day - ds_sys->ts->day;
    if (days_left < 0) days_left = 0;
    char deadline_str[32];
    snprintf(deadline_str, 32, "%dd left", days_left);
    SDL_Color deadline_col = days_left <= 3 ? red :
                             days_left <= 7 ? (SDL_Color){255,180,40,255} :
                             gray;
    draw_text_d(renderer, font, deadline_str, deadline_col, 1160, 8);

    int alert_bar_x = 840;
    draw_text_d(renderer, font, "Alert:", gray, alert_bar_x, 8);
    draw_rect_d(renderer, alert_bar_x + 58, 12, 160, 12, 30, 10, 10, 255);
    int alert_fill = (p->alert_level * 160) / 100;
    SDL_Color alert_col = p->alert_level > 70 ?
        (SDL_Color){255, 40, 40, 255} :
        p->alert_level > 40 ?
        (SDL_Color){255, 180, 40, 255} :
        (SDL_Color){100, 220, 100, 255};
    draw_rect_d(renderer,
                alert_bar_x + 58, 12, alert_fill, 12,
                alert_col.r, alert_col.g, alert_col.b, 255);
    char alert_pct[16];
    snprintf(alert_pct, 16, "%d%%", p->alert_level);
    draw_text_d(renderer, font, alert_pct,
                p->alert_level > 70 ? red : white,
                alert_bar_x + 224, 8);

    // ── Minimap ───────────────────────────────────────────────────────────────
    render_minimap(ds_sys, renderer, font);

    // ── Exit list (bottom strip) ──────────────────────────────────────────────
    draw_rect_d(renderer, 0, 670, 1280, 50, 0, 0, 0, 200);
    const char* dir_names[] = { "^", "v", ">", "<" };
    int ex = 20;
    for (int d = 0; d < 4; d++) {
        if (r->exits[d] >= 0) {
            char exit_str[32];
            DungeonRoom* target = &f->rooms[r->exits[d]];
            snprintf(exit_str, 32, "[%s] %s", dir_names[d], target->name);
            SDL_Color ecol =
                target->type == ROOM_BOSS  ? red :
                target->type == ROOM_SAFE  ? green :
                target->type == ROOM_CHEST ? yellow : white;
            draw_text_d(renderer, font, exit_str, ecol, ex, 680);
            ex += 320;
        }
    }

    // ── Safe room menu overlay ────────────────────────────────────────────────
    if (ds_sys->safe_menu_open) {
        draw_rect_d(renderer,  440, 180, 400, 240, 10, 10, 40, 240);
        draw_border_d(renderer, 440, 180, 400, 240, 80, 80, 200, 255);
        draw_text_d(renderer, font, "Safe Room", yellow, 580, 194);
        const char* opts[] = { "Rest", "Save", "Load", "Exit Dungeon" };
        for (int i = 0; i < 4; i++) {
            char label[32];
            snprintf(label, 32, "%s %s",
                     i == ds_sys->safe_menu_selected ? ">" : " ", opts[i]);
            draw_text_d(renderer, font, label,
                        i == ds_sys->safe_menu_selected ? yellow : white,
                        460, 228 + i * 42);
        }
        draw_text_d(renderer, font, "Up/Down: select  E: confirm  Esc: close",
                    gray, 460, 400);
    }

    // ── Fast travel menu overlay ──────────────────────────────────────────────
    if (ds_sys->fast_travel_open) {
        // Build full list of visited safe rooms across all floors
        int total_safe = 0;
        for (int fl2 = 0; fl2 < p->floor_count; fl2++)
            for (int rm2 = 0; rm2 < p->floors[fl2].room_count; rm2++)
                if (p->floors[fl2].rooms[rm2].type == ROOM_SAFE &&
                    p->floors[fl2].rooms[rm2].visited)
                    total_safe++;

        int panel_h = 80 + total_safe * 44;
        if (panel_h < 140) panel_h = 140;
        draw_rect_d(renderer,  380, 180, 520, panel_h, 5, 10, 40, 245);
        draw_border_d(renderer, 380, 180, 520, panel_h, 60, 80, 200, 255);
        draw_text_d(renderer, font, "Fast Travel — Safe Rooms", yellow, 400, 192);

        int idx2 = 0;
        for (int fl2 = 0; fl2 < p->floor_count; fl2++) {
            for (int rm2 = 0; rm2 < p->floors[fl2].room_count; rm2++) {
                DungeonRoom* sr = &p->floors[fl2].rooms[rm2];
                if (sr->type != ROOM_SAFE || !sr->visited) continue;

                int is_sel = (idx2 == ds_sys->fast_travel_selected);
                int is_cur = (fl2 == p->current_floor &&
                              rm2 == p->floors[fl2].current_room);

                if (is_sel)
                    draw_rect_d(renderer, 382, 228 + idx2*44, 516, 38,
                                40, 40, 90, 255);

                char label[80];
                snprintf(label, 80, "%s F%d: %s%s",
                         is_sel ? ">" : " ",
                         p->floors[fl2].floor_number,
                         sr->name,
                         is_cur ? " (here)" : "");
                draw_text_d(renderer, font, label,
                            is_sel ? yellow : (is_cur ? cyan : white),
                            400, 236 + idx2 * 44);
                idx2++;
            }
        }
        draw_text_d(renderer, font, "Up/Down: select  E: travel  Esc: cancel",
                    gray, 400, 228 + total_safe * 44);
    }
}

// ── Public wrappers for the handcrafted-room Godot pipeline ──────────────────
int dungeon_use_exit_dir(DungeonSystem* ds_sys, int dir,
                         float* player_x, float* player_y) {
    Dungeon* p = dungeon_get_active(ds_sys);
    if (!p || !p->active) return 0;
    if (ds_sys->in_combat) return 0;
    if (ds_sys->transition_cooldown > 0) return 0;
    DungeonRoom* r = dungeon_current_room(ds_sys);
    if (!r) return 0;
    if (dir < 0 || dir > 3) return 0;
    if (r->exits[dir] < 0) return 0;
    enter_room_from(ds_sys, r->exits[dir], (ExitDir)dir, player_x, player_y);
    return 1;
}

int dungeon_trigger_encounter(DungeonSystem* ds_sys, int entity_index,
                              float player_x, float player_y) {
    Dungeon* p = dungeon_get_active(ds_sys);
    if (!p || !p->active) return 0;
    if (ds_sys->in_combat) return 0;
    DungeonRoom* r = dungeon_current_room(ds_sys);
    if (!r) return 0;
    if (entity_index < 0 || entity_index >= r->entities.count) return 0;
    Entity* e = &r->entities.entities[entity_index];
    if (e->type != ENTITY_ENEMY || !e->active || e->defeated) return 0;
    trigger_combat(ds_sys, e, player_x, player_y);
    return 1;
}

// ─── Minimap ─────────────────────────────────────────────────────────────────
// Rendered as a small overlay in the top-left of the dungeon HUD.
// Shows all rooms on the current floor, color-coded by type.
// Only visited rooms are shown filled; unvisited adjacent rooms shown as outlines.
// Pure data-driven — no renderer-specific logic leaks into game state.

#define MMAP_ROOM_W   26
#define MMAP_ROOM_H   20
#define MMAP_GAP       6
#define MMAP_ORIGIN_X 14
#define MMAP_ORIGIN_Y 58

// Build a grid position for each room based on its exit connections.
// We do a simple BFS layout: room 0 at (0,0), each exit offsets by 1.
static void minimap_build_grid(DungeonFloor* f,
                                int* grid_x, int* grid_y) {
    int visited[MAX_DUNGEON_ROOMS] = {0};
    // Queue
    int queue[MAX_DUNGEON_ROOMS];
    int head = 0, tail = 0;

    for (int i = 0; i < f->room_count; i++) {
        grid_x[i] = -99;
        grid_y[i] = -99;
    }

    grid_x[0] = 0;
    grid_y[0] = 0;
    queue[tail++] = 0;
    visited[0] = 1;

    // Direction offsets: N=up, S=down, E=right, W=left
    int dx[] = {0, 0, 1, -1};
    int dy[] = {-1, 1, 0, 0};

    while (head < tail) {
        int cur = queue[head++];
        for (int d = 0; d < 4; d++) {
            int next = f->rooms[cur].exits[d];
            if (next < 0 || next >= f->room_count) continue;
            if (visited[next]) continue;
            visited[next] = 1;
            grid_x[next] = grid_x[cur] + dx[d];
            grid_y[next] = grid_y[cur] + dy[d];
            queue[tail++] = next;
        }
    }

    // Normalise so minimum is 0
    int min_x = 0, min_y = 0;
    for (int i = 0; i < f->room_count; i++) {
        if (grid_x[i] < min_x) min_x = grid_x[i];
        if (grid_y[i] < min_y) min_y = grid_y[i];
    }
    for (int i = 0; i < f->room_count; i++) {
        grid_x[i] -= min_x;
        grid_y[i] -= min_y;
    }
}

static void render_minimap(DungeonSystem* ds_sys,
                            SDL_Renderer* renderer, TTF_Font* font) {
    DungeonFloor* f = dungeon_current_floor(ds_sys);
    if (!f) return;

    // Build room grid positions
    int grid_x[MAX_DUNGEON_ROOMS];
    int grid_y[MAX_DUNGEON_ROOMS];
    minimap_build_grid(f, grid_x, grid_y);

    // Find grid bounds for background sizing
    int max_gx = 0, max_gy = 0;
    for (int i = 0; i < f->room_count; i++) {
        if (grid_x[i] > max_gx) max_gx = grid_x[i];
        if (grid_y[i] > max_gy) max_gy = grid_y[i];
    }

    int map_w = (max_gx + 1) * (MMAP_ROOM_W + MMAP_GAP) + MMAP_GAP;
    int map_h = (max_gy + 1) * (MMAP_ROOM_H + MMAP_GAP) + MMAP_GAP + 20;

    // Background panel
    draw_rect_d(renderer, MMAP_ORIGIN_X - 4, MMAP_ORIGIN_Y - 4,
                map_w + 8, map_h + 8, 5, 5, 20, 210);
    draw_border_d(renderer, MMAP_ORIGIN_X - 4, MMAP_ORIGIN_Y - 4,
                  map_w + 8, map_h + 8, 60, 60, 120, 255);

    SDL_Color gray = {120, 120, 160, 255};
    draw_text_d(renderer, font, "MAP", gray, MMAP_ORIGIN_X, MMAP_ORIGIN_Y);

    int room_origin_y = MMAP_ORIGIN_Y + 18;

    // Draw connection lines first (so rooms render on top)
    SDL_SetRenderDrawColor(renderer, 80, 80, 120, 255);
    for (int i = 0; i < f->room_count; i++) {
        if (!f->rooms[i].visited) continue;
        int rx = MMAP_ORIGIN_X + grid_x[i] * (MMAP_ROOM_W + MMAP_GAP);
        int ry = room_origin_y + grid_y[i] * (MMAP_ROOM_H + MMAP_GAP);
        int cx = rx + MMAP_ROOM_W / 2;
        int cy = ry + MMAP_ROOM_H / 2;

        int dx[] = {0, 0, 1, -1};
        int dy[] = {-1, 1, 0, 0};
        for (int d = 0; d < 4; d++) {
            int next = f->rooms[i].exits[d];
            if (next < 0 || next >= f->room_count) continue;
            if (!f->rooms[next].visited) continue;
            int nx2 = MMAP_ORIGIN_X + grid_x[next] * (MMAP_ROOM_W + MMAP_GAP);
            int ny2 = room_origin_y + grid_y[next] * (MMAP_ROOM_H + MMAP_GAP);
            int cx2 = nx2 + MMAP_ROOM_W / 2;
            int cy2 = ny2 + MMAP_ROOM_H / 2;
            SDL_RenderDrawLine(renderer, cx, cy, cx2, cy2);
        }
    }

    // Draw rooms
    for (int i = 0; i < f->room_count; i++) {
        int rx = MMAP_ORIGIN_X + grid_x[i] * (MMAP_ROOM_W + MMAP_GAP);
        int ry = room_origin_y + grid_y[i] * (MMAP_ROOM_H + MMAP_GAP);

        int is_current = (i == f->current_room);

        if (!f->rooms[i].visited) {
            // Adjacent unvisited rooms — faint outline only
            int adjacent = 0;
            for (int d = 0; d < 4 && !adjacent; d++) {
                int prev = f->rooms[i].exits[d];
                // Check if any visited room exits to this one
                for (int j = 0; j < f->room_count && !adjacent; j++)
                    for (int d2 = 0; d2 < 4 && !adjacent; d2++)
                        if (f->rooms[j].exits[d2] == i && f->rooms[j].visited)
                            adjacent = 1;
            }
            if (adjacent) {
                draw_border_d(renderer, rx, ry,
                              MMAP_ROOM_W, MMAP_ROOM_H,
                              40, 40, 60, 255);
            }
            continue;
        }

        // Visited room — fill by type
        Uint8 rr, g, b;
        switch (f->rooms[i].type) {
            case ROOM_SAFE:
                rr = 40; g = 160; b = 80; break;  // green
            case ROOM_CHEST:
                rr = 180; g = 140; b = 20; break; // gold
            case ROOM_BOSS:
                rr = f->rooms[i].boss.defeated ?
                     60 : 160;
                g  = 20; b = 20; break;            // red / dark if cleared
            default: // ROOM_NORMAL
                rr = f->rooms[i].cleared ?
                     40 : 80;
                g  = f->rooms[i].cleared ? 40 : 60;
                b  = f->rooms[i].cleared ? 80 : 120;
                break;
        }

        draw_rect_d(renderer, rx, ry, MMAP_ROOM_W, MMAP_ROOM_H, rr, g, b, 255);

        // Current room: bright white border + player dot
        if (is_current) {
            draw_border_d(renderer, rx - 1, ry - 1,
                          MMAP_ROOM_W + 2, MMAP_ROOM_H + 2,
                          255, 255, 255, 255);
            // Player dot
            SDL_SetRenderDrawColor(renderer, 255, 220, 80, 255);
            SDL_Rect dot = { rx + MMAP_ROOM_W/2 - 2,
                             ry + MMAP_ROOM_H/2 - 2, 4, 4 };
            SDL_RenderFillRect(renderer, &dot);
        } else {
            draw_border_d(renderer, rx, ry,
                          MMAP_ROOM_W, MMAP_ROOM_H,
                          rr + 40 > 255 ? 255 : rr + 40,
                          g  + 40 > 255 ? 255 : g  + 40,
                          b  + 40 > 255 ? 255 : b  + 40,
                          255);
        }

        // Stairs indicator
        if (i == f->stairs_room_id) {
            SDL_SetRenderDrawColor(renderer, 200, 200, 255, 255);
            SDL_RenderDrawLine(renderer,
                rx + MMAP_ROOM_W - 5, ry + 3,
                rx + MMAP_ROOM_W - 3, ry + MMAP_ROOM_H - 3);
        }
    }

    // Legend (tiny)
    int lx = MMAP_ORIGIN_X;
    int ly = room_origin_y + (max_gy + 1) * (MMAP_ROOM_H + MMAP_GAP) + 2;
    draw_rect_d(renderer, lx,    ly, 6, 6, 40,  160, 80,  255);
    draw_rect_d(renderer, lx+10, ly, 6, 6, 80,  60,  120, 255);
    draw_rect_d(renderer, lx+20, ly, 6, 6, 180, 140, 20,  255);
    draw_rect_d(renderer, lx+30, ly, 6, 6, 160, 20,  20,  255);
}