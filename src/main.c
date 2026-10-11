#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#include <stdio.h>
#include "map.h"
#include "camera.h"
#include "dialogue.h"
#include "gamestate.h"
#include "combat.h"
#include "room.h"
#include "entity.h"
#include "trigger.h"
#include "party.h"
#include "skills.h"
#include "characters.h"
#include "stats.h"
#include "time_system.h"
#include "world.h"
#include "hud.h"
#include "items.h"
#include "inventory.h"
#include "save.h"
#include "pause_menu.h"
#include "shop.h"
#include "title.h"
#include "social_stats.h"
#include "activities.h"
#include "notify.h"
#include "notify_sdl2.h"
#include "bond.h"
#include "events.h"
#include "status_screen.h"
#include "equipment.h"
#include "dungeon.h"
#include "summon.h"
#include "summon_menu.h"
#include "audio.h"
#include "enemy_registry.h"
#include "dungeon_builder.h"
#include "dungeon_db.h"

#define WINDOW_TITLE       "Opal"
#define WINDOW_WIDTH       1280
#define WINDOW_HEIGHT      720
#define PLAYER_SPEED       4
#define NPC_INTERACT_DIST  48
#define ENEMY_TRIGGER_DIST 48

typedef struct {
    float x, y;
    int   width, height;
} Player;

int player_near(Player* p, float ex, float ey, int ew, int eh, int dist) {
    float dx = (p->x + p->width  / 2) - (ex + ew / 2);
    float dy = (p->y + p->height / 2) - (ey + eh / 2);
    return (dx*dx + dy*dy) < (float)(dist * dist);
}

Bond* find_bond(BondDB* db, const char* npc_name) {
    for (int i = 0; i < db->count; i++)
        if (strcmp(db->bonds[i].npc_name_match, npc_name) == 0)
            return &db->bonds[i];
    return NULL;
}


// Sync entity defeated state with gamestate flags after a load.
// Without this, enemies killed before saving stay dead even when loading
// a save from before they were defeated.
static void sync_entities_to_flags(RoomManager* rm, GameState* gs) {
    for (int ri = 0; ri < rm->count; ri++) {
        Room* r = &rm->rooms[ri];
        for (int ei = 0; ei < r->entities.count; ei++) {
            Entity* e = &r->entities.entities[ei];
            if (e->type != ENTITY_ENEMY) continue;
            int flag_val = gamestate_get_flag(gs, e->win_flag);
            printf("[SYNC] room=%d entity=%s win_flag=%s flag=%d old_defeated=%d\n",
                   ri, e->name, e->win_flag, flag_val, e->defeated);
            e->defeated = flag_val;
        }
    }
}

static int any_overlay_open(CombatSystem* cs, DialogueSystem* ds,
                             PauseMenu* pm, ShopMenu* sm,
                             ActivityMenu* am, BondMenu* cm,
                             StatusScreen* ss) {
    return cs->active || ds->active ||
           pm->state != PAUSE_CLOSED     ||
           sm->state != SHOP_CLOSED      ||
           am->state != ACTIVITY_CLOSED  ||
           cm->state != BOND_MENU_CLOSED ||
           ss->state != STATUS_CLOSED;
}

extern SummonDB        g_summon_db;
extern SummonStock     g_summon_stock;
extern SummonCompendium g_summon_comp;
SummonMenu      g_summon_menu;

int g_game_over          = 0;  // 1 = showing game over screen
int g_reload_save_slot   = -1; // >= 0 = request to load this slot
int g_free_time_prompt   = 0;
int g_free_time_selected = 0;
TimeSlot g_prev_slot     = TIMESLOT_MORNING;

void handle_input(SDL_Event* event, int* running, const Uint8* keys,
                  Player* player, RoomManager* rm, DialogueSystem* ds,
                  GameState* gs, CombatSystem* cs, Entity** active_enemy,
                  TimeSystem* ts, WorldSystem* ws, Party* party,
                  PauseMenu* pm, ShopMenu* shop_menu, SocialStats* ss,
                  ActivityDB* activitydb, ActivityMenu* activity_menu,
                  NotifySystem* ns, BondDB* cdb,
                  BondMenu* cmenu, StatusScreen* status_screen,
                  EquipDB* equipdb, DungeonSystem* dungeon_sys) {

    Room* room = roommanager_current(rm);

    while (SDL_PollEvent(event)) {

        // Summon menu intercepts all input when open
        if (g_summon_menu.state != PMENU_CLOSED) {
            if (event->type != SDL_QUIT)
                summon_menu_handle_input(&g_summon_menu, event,
                                          &g_summon_stock, &g_summon_db,
                                          &g_summon_comp, ns, cs->inventory);
            continue;
        }

        // Game over screen intercepts all input
        if (g_game_over) {
            if (event->type == SDL_KEYDOWN) {
                SDL_Keycode gk = event->key.keysym.sym;
                if (gk == SDLK_r) {
                    // Find most recent save and request load from main loop
                    for (int s = 0; s < 3; s++) {
                        if (save_exists(s)) { g_reload_save_slot = s; break; }
                    }
                    g_game_over = 0;
                } else if (gk == SDLK_q || gk == SDLK_ESCAPE) {
                    *running = 0;
                }
            }
            continue;
        }

        // Free time prompt intercepts all input when active
        if (g_free_time_prompt) {
            if (event->type == SDL_KEYDOWN) {
                SDL_Keycode fk = event->key.keysym.sym;
                int hungerscape = gamestate_get_flag(gs, "hungerscape_unlocked");
                // Build dynamic option list
                // opt_ids: 0=activities, 1=hungerscape, 2=pass_time
                int opt_ids[3];
                int num_opts = 0;
                opt_ids[num_opts++] = 0;  // always: Activities
                if (hungerscape) opt_ids[num_opts++] = 1;  // Hungerscape if unlocked
                opt_ids[num_opts++] = 2;  // always: Pass Time

                if (fk == SDLK_UP)
                    g_free_time_selected = (g_free_time_selected - 1 + num_opts) % num_opts;
                else if (fk == SDLK_DOWN)
                    g_free_time_selected = (g_free_time_selected + 1) % num_opts;
                else if (fk == SDLK_e || fk == SDLK_RETURN) {
                    int chosen = opt_ids[g_free_time_selected];
                    g_free_time_prompt = 0;
                    g_free_time_selected = 0;
                    if (chosen == 0) {
                        // Activities
                        activitymenu_open(activity_menu);
                    } else if (chosen == 1) {
                        // Enter Hungerscape
                        if (dungeon_can_enter(dungeon_sys)) {
                            *active_enemy = NULL;
                            dungeon_enter(dungeon_sys, &player->x, &player->y);
                            audio_update_dungeon_bgm(0, 1);
                        } else {
                            notify_info(ns, "Cannot enter the Hungerscape now.");
                        }
                    } else {
                        // Pass Time — just dismiss, update prev_slot to avoid re-trigger
                        notify_info(ns, "You spend the time resting.");
                        g_prev_slot = ts->slot;
                    }
                } else if (fk == SDLK_ESCAPE) {
                    // Dismiss without action
                    g_free_time_prompt = 0;
                    g_free_time_selected = 0;
                    g_prev_slot = ts->slot;
                }
            }
            continue;
        }

        if ((dungeon_get_active(dungeon_sys) != NULL && dungeon_get_active(dungeon_sys)->active)) {
            if (event->type == SDL_QUIT) { *running = 0; continue; }

            // ── Combat / skill learn ──────────────────────────────────────────
            // Combat active covers skill learn screen (active stays 1 during learn)
            if (cs->active) {
                if (event->type == SDL_KEYDOWN)
                    combat_handle_input(cs, event, gs);
                continue;
            }

            // ── Dialogue (boss pre/post battle) ───────────────────────────────
            if (ds->active) {
                if (event->type == SDL_KEYDOWN) {
                    SDL_Keycode key = event->key.keysym.sym;
                    if (ds->nodes[ds->current_node].choice_count == 0) {
                        if (key == SDLK_e || key == SDLK_RETURN)
                            dialogue_advance(ds, 0, gs, cdb, ns);
                    } else {
                        if (key == SDLK_1) dialogue_advance(ds, 0, gs, cdb, ns);
                        if (key == SDLK_2) dialogue_advance(ds, 1, gs, cdb, ns);
                        if (key == SDLK_3) dialogue_advance(ds, 2, gs, cdb, ns);
                        if (key == SDLK_4) dialogue_advance(ds, 3, gs, cdb, ns);
                    }
                }
                continue;
            }

            // ── Overlays — same as overworld ──────────────────────────────────
            if (pm->state != PAUSE_CLOSED) {
                // Inside dungeon: block save unless triggered from safe room,
                // block load entirely (Summon 5 accurate)
                DungeonRoom* cur_room = dungeon_current_room(dungeon_sys);
                int in_safe = cur_room && cur_room->type == ROOM_SAFE;

                if (pm->state == PAUSE_SAVE && !in_safe) {
                    // Not in safe room — block save
                    if (event->type == SDL_KEYDOWN &&
                        (event->key.keysym.sym == SDLK_e ||
                         event->key.keysym.sym == SDLK_RETURN)) {
                        notify_info(ns, "You can only save in a Safe Room.");
                        pause_menu_close(pm);
                    } else if (event->type == SDL_KEYDOWN &&
                               event->key.keysym.sym == SDLK_ESCAPE) {
                        pause_menu_close(pm);
                    }
                    continue;
                }

                // In safe room + save: intercept to inject dungeon_sys
                if (pm->state == PAUSE_SAVE && in_safe &&
                    event->type == SDL_KEYDOWN &&
                    (event->key.keysym.sym == SDLK_e ||
                     event->key.keysym.sym == SDLK_RETURN)) {
                    SaveData sd;
                    save_data_from_state(&sd, cs->party, cs->inventory,
                                         gs, ts, ss, cdb,
                                         player->x, player->y,
                                         room->id, dungeon_sys);
                    save_game(pm->selected, &sd);
                    pm->slots[pm->selected] = sd;
                    notify_success(ns, "Game saved.");
                    pause_menu_close(pm);
                    continue;
                }

                // In safe room + load: intercept to inject dungeon_sys
                if (pm->state == PAUSE_LOAD && in_safe &&
                    event->type == SDL_KEYDOWN &&
                    (event->key.keysym.sym == SDLK_e ||
                     event->key.keysym.sym == SDLK_RETURN)) {
                    if (pm->slots[pm->selected].used) {
                        float lx, ly; int lr;
                        dungeon_reset_active(dungeon_sys);
                        save_data_to_state(&pm->slots[pm->selected],
                                           cs->party, cs->inventory,
                                           gs, ts, ss, cdb,
                                           &lx, &ly, &lr, dungeon_sys);
                        // Exit dungeon and return to overworld
                        { Dungeon* _p = dungeon_get_active(dungeon_sys); if (_p) _p->active = 0; }
                        player->x = lx;
                        player->y = ly;
                        roommanager_load(rm, lr, &lx, &ly);
                        sync_entities_to_flags(rm, gs);
                        notify_info(ns, "Game loaded.");
                        pause_menu_close(pm);
                    }
                    continue;
                }

                if (pm->state == PAUSE_LOAD && !in_safe) {
                    // Can only load from a safe room
                    if (event->type == SDL_KEYDOWN &&
                        (event->key.keysym.sym == SDLK_e ||
                         event->key.keysym.sym == SDLK_RETURN ||
                         event->key.keysym.sym == SDLK_ESCAPE)) {
                        notify_info(ns, "You can only load in a Safe Room.");
                        pause_menu_close(pm);
                    }
                    continue;
                }

                float load_x = player->x, load_y = player->y;
                int load_room = room->id, loaded = 0;
                pause_menu_handle_input(pm, event,
                                        cs->party, cs->inventory,
                                        gs, ts, ss, cdb,
                                        player->x, player->y,
                                        room->id,
                                        &load_x, &load_y,
                                        &load_room, &loaded);
                if (loaded) {
                    // Re-apply with dungeon_sys to restore dungeon state
                    dungeon_reset_active(dungeon_sys);
                    {
                        float lx2 = load_x, ly2 = load_y; int lr2 = load_room;
                        save_data_to_state(&pm->slots[pm->selected],
                                           cs->party, cs->inventory,
                                           gs, ts, ss, cdb,
                                           &lx2, &ly2, &lr2, dungeon_sys);
                    }
                    { Dungeon* _p = dungeon_get_active(dungeon_sys); if (_p) _p->active = 0; }
                    player->x = load_x;
                    player->y = load_y;
                    roommanager_load(rm, load_room, &load_x, &load_y);
                    sync_entities_to_flags(rm, gs);
                    notify_info(ns, "Game loaded.");
                }
                continue;
            }

            if (activity_menu->state != ACTIVITY_CLOSED) {
                activitymenu_handle_input(activity_menu, event,
                                          activitydb, ss, ts,
                                          cs->party, ns, cs->inventory);
                continue;
            }

            if (cmenu->state != BOND_MENU_CLOSED) {
                bondmenu_handle_input(cmenu, event, cdb, ss, ns, ts);
                continue;
            }

            if (status_screen->state != STATUS_CLOSED) {
                statusscreen_handle_input(status_screen, event,
                                          cs->party, equipdb);
                continue;
            }

            // ── Keydown events ────────────────────────────────────────────────
            if (event->type != SDL_KEYDOWN) continue;
            {
                SDL_Keycode key = event->key.keysym.sym;
                

                // Escape: close overlays, open safe-room menu, or pause
                if (key == SDLK_ESCAPE) {
                    if (activity_menu->state != ACTIVITY_CLOSED)
                        activitymenu_close(activity_menu);
                    else if (cmenu->state != BOND_MENU_CLOSED)
                        bondmenu_close(cmenu);
                    else if (status_screen->state != STATUS_CLOSED)
                        statusscreen_close(status_screen);
                    else {
                        DungeonRoom* dr = dungeon_current_room(dungeon_sys);
                        if (dr && dr->type == ROOM_SAFE)
                            dungeon_handle_input(dungeon_sys, event,
                                                 &player->x, &player->y);
                        else
                            pause_menu_open(pm);
                    }
                    continue;
                }

                // Tab: status screen
                if (key == SDLK_TAB) {
                    statusscreen_open(status_screen);
                    continue;
                }

                // Q: equip screen
                if (key == SDLK_q) {
                    statusscreen_open(status_screen);
                    status_screen->state = STATUS_EQUIP;
                    continue;
                }

                // F: activities (can do activities in Hungerscape)
                if (key == SDLK_f) {
                    activitymenu_open(activity_menu);
                    continue;
                }

                // C: bond menu
                if (key == SDLK_c) {
                    bondmenu_open(cmenu);
                    continue;
                }

                // Dialogue choices
                if (key == SDLK_1) { if (ds->active) dialogue_advance(ds, 0, gs, cdb, ns); continue; }
                if (key == SDLK_2) { if (ds->active) dialogue_advance(ds, 1, gs, cdb, ns); continue; }
                if (key == SDLK_3) { if (ds->active) dialogue_advance(ds, 2, gs, cdb, ns); continue; }
                if (key == SDLK_4) { if (ds->active) dialogue_advance(ds, 3, gs, cdb, ns); continue; }

                // Everything else goes to dungeon input (E, T, stairs)
                dungeon_handle_input(dungeon_sys, event,
                                     &player->x, &player->y);
            }
            continue;
        }

        if (pm->state != PAUSE_CLOSED) {
            float load_x = player->x, load_y = player->y;
            int load_room = room->id, loaded = 0;
            pause_menu_handle_input(pm, event,
                                    cs->party, cs->inventory,
                                    gs, ts, ss, cdb,
                                    player->x, player->y,
                                    room->id,
                                    &load_x, &load_y,
                                    &load_room, &loaded);
            if (loaded) {
                dungeon_reset_active(&dungeon_sys);
                player->x = load_x;
                player->y = load_y;
                roommanager_load(rm, load_room, &load_x, &load_y);
                sync_entities_to_flags(rm, gs);
                notify_info(ns, "Game loaded.");
            }
            continue;
        }

        if (shop_menu->state != SHOP_CLOSED) {
            shopmenu_handle_input(shop_menu, event,
                                  cs->inventory, cs->itemdb);
            continue;
        }

        if (activity_menu->state != ACTIVITY_CLOSED) {
            activitymenu_handle_input(activity_menu, event,
                                      activitydb, ss, ts,
                                      cs->party, ns, cs->inventory);
            continue;
        }

        if (cmenu->state != BOND_MENU_CLOSED) {
            bondmenu_handle_input(cmenu, event, cdb, ss, ns, ts);
            continue;
        }

        if (status_screen->state != STATUS_CLOSED) {
            statusscreen_handle_input(status_screen, event,
                                      cs->party, equipdb);
            continue;
        }

        if (event->type == SDL_QUIT) { *running = 0; continue; }
        if (event->type != SDL_KEYDOWN) continue;

        if (cs->active) {
            combat_handle_input(cs, event, gs);
            return;
        }

        switch (event->key.keysym.sym) {
            case SDLK_ESCAPE:
                if (shop_menu->state != SHOP_CLOSED)
                    shopmenu_close(shop_menu);
                else if (activity_menu->state != ACTIVITY_CLOSED)
                    activitymenu_close(activity_menu);
                else if (cmenu->state != BOND_MENU_CLOSED)
                    bondmenu_close(cmenu);
                else if (status_screen->state != STATUS_CLOSED)
                    statusscreen_close(status_screen);
                else
                    pause_menu_open(pm);
                break;

            // Y: Summon menu
            case SDLK_y:
                if (!cs->active && !ds->active)
                    summon_menu_open(&g_summon_menu, &g_summon_comp, cs->inventory);
                break;

            case SDLK_TAB:
                if (!ds->active &&
                    shop_menu->state == SHOP_CLOSED &&
                    pm->state        == PAUSE_CLOSED &&
                    cmenu->state     == BOND_MENU_CLOSED)
                    statusscreen_open(status_screen);
                break;

            case SDLK_q:
                if (!ds->active &&
                    shop_menu->state == SHOP_CLOSED &&
                    pm->state        == PAUSE_CLOSED &&
                    cmenu->state     == BOND_MENU_CLOSED) {
                    statusscreen_open(status_screen);
                    status_screen->state = STATUS_EQUIP;
                }
                break;

            case SDLK_f:
                if (!ds->active &&
                    shop_menu->state == SHOP_CLOSED &&
                    pm->state        == PAUSE_CLOSED &&
                    cmenu->state     == BOND_MENU_CLOSED)
                    activitymenu_open(activity_menu);
                break;

            case SDLK_c:
                if (!ds->active &&
                    shop_menu->state     == SHOP_CLOSED &&
                    pm->state            == PAUSE_CLOSED &&
                    activity_menu->state == ACTIVITY_CLOSED)
                    bondmenu_open(cmenu);
                break;

            case SDLK_m:
                if (!ds->active && !cs->active &&
                    !any_overlay_open(cs, ds, pm, shop_menu,
                                      activity_menu, cmenu, status_screen)) {
                    if (dungeon_can_enter(dungeon_sys)) {
                        *active_enemy = NULL;  // clear stale overworld enemy ref
                        dungeon_enter(dungeon_sys, &player->x, &player->y);
                        audio_update_dungeon_bgm(0, 1);
                    } else if (!gamestate_get_flag(gs, "hungerscape_unlocked")) {
                        notify_info(ns,
                            "The Hungerscape is not yet accessible.");
                    } else {
                        notify_info(ns,
                            "Enter the Hungerscape in the afternoon or later.");
                    }
                }
                break;

            case SDLK_n:
                timesystem_advance(ts);
                if (ts->slot == TIMESLOT_MORNING)
                    dungeon_reset_alert(dungeon_sys);
                else
                    g_free_time_prompt = 1;
                break;

            case SDLK_F1:
                timesystem_sleep(ts);
                for (int i = 0; i < party->count; i++) {
                    party->members[i].hp = party->members[i].max_hp;
                    party->members[i].sp = party->members[i].max_sp;
                }
                dungeon_reset_alert(dungeon_sys);
                break;

            case SDLK_k:
                social_stats_add_exp(ss, SOCIAL_KNOWLEDGE,   3);
                social_stats_add_exp(ss, SOCIAL_CHARM,       2);
                social_stats_add_exp(ss, SOCIAL_GUTS,        1);
                social_stats_add_exp(ss, SOCIAL_KINDNESS,    2);
                social_stats_add_exp(ss, SOCIAL_PROFICIENCY, 1);
                break;

            case SDLK_e:
                if (!ds->active && shop_menu->state == SHOP_CLOSED &&
                    cmenu->state == BOND_MENU_CLOSED &&
                    status_screen->state == STATUS_CLOSED) {
                    for (int i = 0; i < room->entities.count; i++) {
                        Entity* e = &room->entities.entities[i];
                        if (e->type != ENTITY_NPC || !e->active) continue;
                        if (!player_near(player, e->x, e->y,
                                         e->width, e->height,
                                         NPC_INTERACT_DIST)) continue;
                        Bond* conf = find_bond(cdb, e->name);
                        if (conf && conf->current_rank == 0) {
                            conf->current_rank = 1;
                            notify_success(ns,
                                conf->ranks[0].rank_up_message);
                        }
                        char flag[128];
                        snprintf(flag, 128, "met_%s", e->name);
                        int start = gamestate_get_flag(gs, flag)
                                    ? e->dialogue_repeat
                                    : e->dialogue_start;
                        gamestate_set_flag(gs, flag, 1);
                        dialogue_start(ds, start);
                        break;
                    }
                } else if (ds->active &&
                           ds->nodes[ds->current_node].choice_count == 0) {
                    dialogue_advance(ds, 0, gs, cdb, ns);
                }
                break;

            case SDLK_1: if (ds->active) dialogue_advance(ds, 0, gs, cdb, ns); break;
            case SDLK_2: if (ds->active) dialogue_advance(ds, 1, gs, cdb, ns); break;
            case SDLK_3: if (ds->active) dialogue_advance(ds, 2, gs, cdb, ns); break;
            case SDLK_4: if (ds->active) dialogue_advance(ds, 3, gs, cdb, ns); break;
        }
    }

    if ((dungeon_get_active(dungeon_sys) != NULL && dungeon_get_active(dungeon_sys)->active)) return;
    if (any_overlay_open(cs, ds, pm, shop_menu, activity_menu, cmenu,
                         status_screen))
        return;

    float nx = player->x;
    float ny = player->y;
    if (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP])    ny -= PLAYER_SPEED;
    if (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN])  ny += PLAYER_SPEED;
    if (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT])  nx -= PLAYER_SPEED;
    if (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT]) nx += PLAYER_SPEED;

    int can_move = 1;
    for (int i = 0; i <= 4; i++) {
        float t = (float)i / 4.0f;
        if (map_is_wall(&room->map,
                (int)(nx + t*(player->width-1)),  (int)ny) ||
            map_is_wall(&room->map,
                (int)(nx + t*(player->width-1)),  (int)(ny+player->height-1)) ||
            map_is_wall(&room->map,
                (int)nx,                           (int)(ny + t*(player->height-1))) ||
            map_is_wall(&room->map,
                (int)(nx+player->width-1),         (int)(ny + t*(player->height-1)))) {
            can_move = 0; break;
        }
    }
    if (can_move) { player->x = nx; player->y = ny; }

    for (int i = 0; i < room->entities.count; i++) {
        Entity* e = &room->entities.entities[i];
        if (e->type != ENTITY_ENEMY || !e->active || e->defeated) continue;
        if (!cs->active && player_near(player, e->x, e->y,
                                        e->width, e->height,
                                        ENEMY_TRIGGER_DIST)) {
            e->pre_combat_x = player->x;
            e->pre_combat_y = player->y;
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
            *active_enemy = e;
            combat_start(cs, group, e->group_size, e->win_flag);
            break;
        }
    }
}

void render(SDL_Renderer* renderer, Player* player, RoomManager* rm,
            Camera* cam, DialogueSystem* ds, TTF_Font* font,
            CombatSystem* cs, TimeSystem* ts, WorldSystem* ws,
            PauseMenu* pm, ShopMenu* shop_menu, SocialStats* ss,
            ActivityDB* activitydb, ActivityMenu* activity_menu,
            NotifySystem* ns, BondDB* cdb, BondMenu* cmenu,
            StatusScreen* status_screen, EquipDB* equipdb,
            DungeonSystem* dungeon_sys) {

    if (cs->active) {
        combat_render(renderer, cs, font);
        summon_menu_render(&g_summon_menu, renderer, font, &g_summon_stock, cs->skilldb);
    notify_render(ns, renderer, font);

    // Game over screen — drawn on top of everything
    extern int g_game_over;
    if (g_game_over) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 220);
        SDL_Rect ov = {0, 0, 1280, 720};
        SDL_RenderFillRect(renderer, &ov);

        // Red panel
        SDL_SetRenderDrawColor(renderer, 80, 0, 0, 255);
        SDL_Rect panel = {340, 220, 600, 280};
        SDL_RenderFillRect(renderer, &panel);
        SDL_SetRenderDrawColor(renderer, 200, 40, 40, 255);
        SDL_RenderDrawRect(renderer, &panel);

        SDL_Color red    = {220,  60,  60, 255};
        SDL_Color white  = {255, 255, 255, 255};
        SDL_Color gray   = {150, 150, 180, 255};

        // "GAME OVER" text
        { SDL_Surface* s = TTF_RenderText_Blended(font, "GAME  OVER", red);
          if (s) { SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
          SDL_Rect d = {640 - s->w/2, 260, s->w, s->h};
          SDL_RenderCopy(renderer, t, NULL, &d);
          SDL_FreeSurface(s); SDL_DestroyTexture(t); } }

        { SDL_Surface* s = TTF_RenderText_Blended(font,
              "Prota has fallen. The mission ends here.", white);
          if (s) { SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
          SDL_Rect d = {640 - s->w/2, 320, s->w, s->h};
          SDL_RenderCopy(renderer, t, NULL, &d);
          SDL_FreeSurface(s); SDL_DestroyTexture(t); } }

        { SDL_Surface* s = TTF_RenderText_Blended(font,
              "R: Load last save     Q / Esc: Quit", gray);
          if (s) { SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
          SDL_Rect d = {640 - s->w/2, 420, s->w, s->h};
          SDL_RenderCopy(renderer, t, NULL, &d);
          SDL_FreeSurface(s); SDL_DestroyTexture(t); } }
    }
        SDL_RenderPresent(renderer);
        return;
    }

    if ((dungeon_get_active(dungeon_sys) != NULL && dungeon_get_active(dungeon_sys)->active)) {
        dungeon_render(dungeon_sys, renderer, font,
                       player->x, player->y, cam);
        // All overlays work inside the dungeon
        if (ds->active)
            dialogue_render(renderer, ds, font);
        statusscreen_render(status_screen, renderer, font,
                            cs->party, cs->skilldb, equipdb);
        activitymenu_render(activity_menu, renderer, font,
                            activitydb, ss, ts, cs->inventory);
        shopmenu_render(shop_menu, renderer, font,
                        cs->inventory, cs->itemdb);
        bondmenu_render(cmenu, renderer, font, cdb, ss);
        if (pm->state != PAUSE_CLOSED)
            pause_menu_render(pm, renderer, font, cs->party);
        summon_menu_render(&g_summon_menu, renderer, font, &g_summon_stock, cs->skilldb);
    notify_render(ns, renderer, font);

    // Game over screen — drawn on top of everything
    extern int g_game_over;
    if (g_game_over) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 220);
        SDL_Rect ov = {0, 0, 1280, 720};
        SDL_RenderFillRect(renderer, &ov);

        // Red panel
        SDL_SetRenderDrawColor(renderer, 80, 0, 0, 255);
        SDL_Rect panel = {340, 220, 600, 280};
        SDL_RenderFillRect(renderer, &panel);
        SDL_SetRenderDrawColor(renderer, 200, 40, 40, 255);
        SDL_RenderDrawRect(renderer, &panel);

        SDL_Color red    = {220,  60,  60, 255};
        SDL_Color white  = {255, 255, 255, 255};
        SDL_Color gray   = {150, 150, 180, 255};

        // "GAME OVER" text
        { SDL_Surface* s = TTF_RenderText_Blended(font, "GAME  OVER", red);
          if (s) { SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
          SDL_Rect d = {640 - s->w/2, 260, s->w, s->h};
          SDL_RenderCopy(renderer, t, NULL, &d);
          SDL_FreeSurface(s); SDL_DestroyTexture(t); } }

        { SDL_Surface* s = TTF_RenderText_Blended(font,
              "Prota has fallen. The mission ends here.", white);
          if (s) { SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
          SDL_Rect d = {640 - s->w/2, 320, s->w, s->h};
          SDL_RenderCopy(renderer, t, NULL, &d);
          SDL_FreeSurface(s); SDL_DestroyTexture(t); } }

        { SDL_Surface* s = TTF_RenderText_Blended(font,
              "R: Load last save     Q / Esc: Quit", gray);
          if (s) { SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
          SDL_Rect d = {640 - s->w/2, 420, s->w, s->h};
          SDL_RenderCopy(renderer, t, NULL, &d);
          SDL_FreeSurface(s); SDL_DestroyTexture(t); } }
    }
        SDL_RenderPresent(renderer);
        return;
    }

    Room* room = roommanager_current(rm);
    SDL_SetRenderDrawColor(renderer, 20, 20, 40, 255);
    SDL_RenderClear(renderer);
    map_render(renderer, &room->map, cam);

    for (int i = 0; i < room->entities.count; i++) {
        Entity* e = &room->entities.entities[i];
        if (!e->active) continue;
        if (e->type == ENTITY_NPC)
            SDL_SetRenderDrawColor(renderer, 100, 220, 100, 255);
        else {
            if (e->defeated) continue;
            SDL_SetRenderDrawColor(renderer, 220, 60, 60, 255);
        }
        SDL_Rect rect = {
            (int)(e->x - cam->x), (int)(e->y - cam->y),
            e->width, e->height
        };
        SDL_RenderFillRect(renderer, &rect);
        if (e->type == ENTITY_NPC &&
            player_near(player, e->x, e->y, e->width, e->height,
                        NPC_INTERACT_DIST) && !ds->active) {
            SDL_Color white = {255, 255, 255, 255};
            SDL_Surface* surf = TTF_RenderText_Blended(font,
                                "Press E to talk", white);
            SDL_Texture* tex  = SDL_CreateTextureFromSurface(renderer, surf);
            SDL_Rect dst = {
                (int)(e->x - cam->x) - 20, (int)(e->y - cam->y) - 30,
                surf->w, surf->h
            };
            SDL_RenderCopy(renderer, tex, NULL, &dst);
            SDL_FreeSurface(surf);
            SDL_DestroyTexture(tex);
        }
    }

    SDL_SetRenderDrawColor(renderer, 255, 220, 100, 255);
    SDL_Rect prect = {
        (int)(player->x - cam->x), (int)(player->y - cam->y),
        player->width, player->height
    };
    SDL_RenderFillRect(renderer, &prect);

    // Hungerscape entry hint
    if (gamestate_get_flag(dungeon_sys->gs, "hungerscape_unlocked")) {
        SDL_Color hint = {100, 180, 255, 255};
        SDL_Surface* hs = TTF_RenderText_Blended(font,
                          "M: Enter Hungerscape", hint);
        if (hs) {
            SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
            SDL_Rect hdst = { 20, 52, hs->w, hs->h };
            SDL_RenderCopy(renderer, ht, NULL, &hdst);
            SDL_FreeSurface(hs);
            SDL_DestroyTexture(ht);
        }
    }
    // Summon hint
    { SDL_Color pc = {180, 100, 220, 255};
      Summon* ap = summon_active(&g_summon_stock);
      if (ap) {
        char phint[64]; snprintf(phint, 64, "Y: Summon (%s)", ap->name);
        SDL_Surface* ps = TTF_RenderText_Blended(font, phint, pc);
        if (ps) { SDL_Texture* pt = SDL_CreateTextureFromSurface(renderer, ps);
        SDL_Rect pd = {20, 100, ps->w, ps->h};
        SDL_RenderCopy(renderer, pt, NULL, &pd);
        SDL_FreeSurface(ps); SDL_DestroyTexture(pt); } } }


    SDL_Color gray = {150, 150, 180, 255};
    SDL_Surface* rname = TTF_RenderText_Blended(font, room->name, gray);
    SDL_Texture* rnt   = SDL_CreateTextureFromSurface(renderer, rname);
    SDL_Rect rndst = { 20, 20, rname->w, rname->h };
    SDL_RenderCopy(renderer, rnt, NULL, &rndst);
    SDL_FreeSurface(rname);
    SDL_DestroyTexture(rnt);

    hud_render(renderer, font, ts, ws, ss, dungeon_sys);
    statusscreen_render(status_screen, renderer, font,
                        cs->party, cs->skilldb, equipdb);
    dialogue_render(renderer, ds, font);
    activitymenu_render(activity_menu, renderer, font, activitydb, ss, ts, cs->inventory);
    shopmenu_render(shop_menu, renderer, font, cs->inventory, cs->itemdb);
    bondmenu_render(cmenu, renderer, font, cdb, ss);
    pause_menu_render(pm, renderer, font, cs->party);
    summon_menu_render(&g_summon_menu, renderer, font, &g_summon_stock, cs->skilldb);
    notify_render(ns, renderer, font);

    // Game over screen — drawn on top of everything
    extern int g_game_over;
    if (g_game_over) {
        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 220);
        SDL_Rect ov = {0, 0, 1280, 720};
        SDL_RenderFillRect(renderer, &ov);

        // Red panel
        SDL_SetRenderDrawColor(renderer, 80, 0, 0, 255);
        SDL_Rect panel = {340, 220, 600, 280};
        SDL_RenderFillRect(renderer, &panel);
        SDL_SetRenderDrawColor(renderer, 200, 40, 40, 255);
        SDL_RenderDrawRect(renderer, &panel);

        SDL_Color red    = {220,  60,  60, 255};
        SDL_Color white  = {255, 255, 255, 255};
        SDL_Color gray   = {150, 150, 180, 255};

        // "GAME OVER" text
        { SDL_Surface* s = TTF_RenderText_Blended(font, "GAME  OVER", red);
          if (s) { SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
          SDL_Rect d = {640 - s->w/2, 260, s->w, s->h};
          SDL_RenderCopy(renderer, t, NULL, &d);
          SDL_FreeSurface(s); SDL_DestroyTexture(t); } }

        { SDL_Surface* s = TTF_RenderText_Blended(font,
              "Prota has fallen. The mission ends here.", white);
          if (s) { SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
          SDL_Rect d = {640 - s->w/2, 320, s->w, s->h};
          SDL_RenderCopy(renderer, t, NULL, &d);
          SDL_FreeSurface(s); SDL_DestroyTexture(t); } }

        { SDL_Surface* s = TTF_RenderText_Blended(font,
              "R: Load last save     Q / Esc: Quit", gray);
          if (s) { SDL_Texture* t = SDL_CreateTextureFromSurface(renderer, s);
          SDL_Rect d = {640 - s->w/2, 420, s->w, s->h};
          SDL_RenderCopy(renderer, t, NULL, &d);
          SDL_FreeSurface(s); SDL_DestroyTexture(t); } }
    }

    // Free time prompt overlay (drawn on top of everything)
    if (g_free_time_prompt) {
        int hungerscape = gamestate_get_flag(dungeon_sys->gs, "hungerscape_unlocked");
        // Build same dynamic option list as input handler
        const char* all_opts[] = {"Activities", "Enter Hungerscape", "Pass Time"};
        int opt_ids[3]; int num_opts = 0;
        opt_ids[num_opts++] = 0;
        if (hungerscape) opt_ids[num_opts++] = 1;
        opt_ids[num_opts++] = 2;

        SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
        SDL_SetRenderDrawColor(renderer, 0, 0, 0, 160);
        SDL_Rect ov = {0, 0, 1280, 720};
        SDL_RenderFillRect(renderer, &ov);

        int pw = 400, ph = 80 + num_opts * 44;
        int px = (1280 - pw) / 2, py = (720 - ph) / 2;

        SDL_SetRenderDrawColor(renderer, 10, 10, 35, 245);
        SDL_Rect panel = {px, py, pw, ph};
        SDL_RenderFillRect(renderer, &panel);
        SDL_SetRenderDrawColor(renderer, 80, 80, 180, 255);
        SDL_RenderDrawRect(renderer, &panel);

        SDL_Color yellow = {255, 220, 100, 255};
        SDL_Color white  = {255, 255, 255, 255};
        SDL_Color gray   = {150, 150, 180, 255};

        // Title
        { SDL_Surface* _s = TTF_RenderText_Blended(font, "What will you do?", yellow);
          if (_s) { SDL_Texture* _t = SDL_CreateTextureFromSurface(renderer, _s);
          SDL_Rect _d = {px + pw/2 - _s->w/2, py + 12, _s->w, _s->h};
          SDL_RenderCopy(renderer, _t, NULL, &_d);
          SDL_FreeSurface(_s); SDL_DestroyTexture(_t); } }

        // Options
        for (int i = 0; i < num_opts; i++) {
            int oy = py + 60 + i * 44;
            if (i == g_free_time_selected) {
                SDL_SetRenderDrawColor(renderer, 40, 40, 90, 255);
                SDL_Rect sel = {px + 8, oy - 4, pw - 16, 36};
                SDL_RenderFillRect(renderer, &sel);
            }
            char label[64];
            snprintf(label, 64, "%s %s",
                     i == g_free_time_selected ? ">" : " ", all_opts[opt_ids[i]]);
            SDL_Color col = i == g_free_time_selected ? yellow : white;
            SDL_Surface* _s = TTF_RenderText_Blended(font, label, col);
            if (_s) { SDL_Texture* _t = SDL_CreateTextureFromSurface(renderer, _s);
            SDL_Rect _d = {px + 20, oy, _s->w, _s->h};
            SDL_RenderCopy(renderer, _t, NULL, &_d);
            SDL_FreeSurface(_s); SDL_DestroyTexture(_t); }
        }
        // Hint
        { SDL_Surface* _s = TTF_RenderText_Blended(font,
              "Up/Down: select  E: confirm", gray);
          if (_s) { SDL_Texture* _t = SDL_CreateTextureFromSurface(renderer, _s);
          SDL_Rect _d = {px + 20, py + ph - 26, _s->w, _s->h};
          SDL_RenderCopy(renderer, _t, NULL, &_d);
          SDL_FreeSurface(_s); SDL_DestroyTexture(_t); } }
    }

    SDL_RenderPresent(renderer);
}

// Global enemy registry — shared across dungeon.c, dungeon_builder.c, dungeon_N.c
extern EnemyRegistry g_enemy_registry;
DungeonDB      g_dungeon_db;


int main(int argc, char* argv[]) {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO) != 0) {
        printf("SDL failed: %s\n", SDL_GetError());
        return 1;
    }
    if (TTF_Init() != 0) {
        printf("TTF failed: %s\n", TTF_GetError());
        SDL_Quit(); return 1;
    }

    audio_init();

    TTF_Font* font = TTF_OpenFont("assets/fonts/Roboto.ttf", 22);
    if (!font) {
        printf("Font failed: %s\n", TTF_GetError());
        TTF_Quit(); SDL_Quit(); return 1;
    }

    SDL_Window* window = SDL_CreateWindow(
        WINDOW_TITLE,
        SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED,
        WINDOW_WIDTH, WINDOW_HEIGHT, SDL_WINDOW_SHOWN
    );
    SDL_Renderer* renderer = SDL_CreateRenderer(window, -1,
                                                SDL_RENDERER_ACCELERATED);

    SkillDB skilldb;   skilldb_init(&skilldb);
    Party   party;     build_starter_party(&party);
    EquipDB equipdb;   equipdb_init(&equipdb);

    // Enemy registry and dungeon DB must be initialized before room/dungeon builders
    summondb_init(&g_summon_db);
    summon_stock_init(&g_summon_stock, &g_summon_db);
    memset(&g_summon_comp, 0, sizeof(SummonCompendium));
    summon_menu_init(&g_summon_menu);
    // Register Opaline in compendium at start
    summon_register(&g_summon_comp, summon_active(&g_summon_stock));

    enemy_registry_init(&g_enemy_registry);
    dungeon_db_init(&g_dungeon_db);

    RoomManager rm;    roommanager_init(&rm);
    Camera cam    = { 0, 0 };
    Player player = { 64, 64, 32, 48 };

    GameState gs;      gamestate_init(&gs);
    DialogueSystem ds; dialogue_init(&ds, &gs);
    ItemDB itemdb;     itemdb_init(&itemdb);
    Inventory inventory; inventory_init(&inventory);
    NotifySystem notify; notify_init(&notify);
    BondDB bond_db; bonddb_init(&bond_db);

    CombatSystem cs;
    combat_init(&cs, &party, &skilldb, &gs, &inventory, &itemdb,
                &notify, &bond_db);

    TimeSystem ts;     timesystem_init(&ts);
    WorldSystem ws;    worldsystem_init(&ws);
    SocialStats social; social_stats_init(&social);
    ActivityDB activitydb; activitydb_init(&activitydb);
    ActivityMenu activity_menu; activitymenu_init(&activity_menu);
    BondMenu bond_menu; bondmenu_init(&bond_menu);
    EventSystem event_system; eventsystem_init(&event_system);
    StatusScreen status_screen; statusscreen_init(&status_screen);
    PauseMenu pause_menu; pause_menu_init(&pause_menu);
    ShopMenu shop_menu; shopmenu_init(&shop_menu);
    shop_menu.bonds = &bond_db;

    DungeonSystem dungeon_sys;
    dungeon_init(&dungeon_sys, &party, &inventory, &gs, &ts, &notify,
                 &skilldb, &itemdb, &equipdb, &ds, &cs,
                 &g_dungeon_db);

    TitleScreen title; title_init(&title);
    audio_play_bgm(BGM_TITLE);

    while (!title_is_done(&title)) {
        SDL_Event tev;
        while (SDL_PollEvent(&tev)) {
            if (tev.type == SDL_QUIT) {
                TTF_CloseFont(font);
                SDL_DestroyRenderer(renderer);
                SDL_DestroyWindow(window);
                TTF_Quit(); SDL_Quit(); return 0;
            }
            title_handle_input(&title, &tev);
        }
        SDL_SetRenderDrawColor(renderer, 5, 5, 20, 255);
        SDL_RenderClear(renderer);
        title_render(&title, renderer, font);
        SDL_RenderPresent(renderer);
        SDL_Delay(16);
    }

    audio_play_bgm(BGM_OVERWORLD);

    if (title.state == TITLE_DONE_LOAD && title.load_slot >= 0) {
        SaveData sd = title.slots[title.load_slot];
        int room_id; float lx, ly;
        dungeon_reset_active(&dungeon_sys);
        save_data_to_state(&sd, &party, &inventory, &gs, &ts, &social,
                           &bond_db, &lx, &ly, &room_id, &dungeon_sys);
        player.x = lx; player.y = ly;
        roommanager_load(&rm, room_id, &lx, &ly);
        sync_entities_to_flags(&rm, &gs);
        notify_info(&notify, "Game loaded.");
    }

    int running              = 1;
    int post_combat_cooldown = 0;

    // Free time prompt — shown at the start of each free slot
    g_prev_slot = ts.slot;
    int transition_cooldown  = 0;
    int last_day             = ts.day;
    SDL_Event event;
    Entity* active_enemy = NULL;

    while (running) {
        const Uint8* keys = SDL_GetKeyboardState(NULL);
        Room* room = roommanager_current(&rm);

        // Alert reset on new day
        if (ts.day != last_day) {
            last_day = ts.day;
            if (ts.slot == TIMESLOT_MORNING)
                dungeon_reset_alert(&dungeon_sys);
        }

        // Dungeon game over
        if (gamestate_get_flag(&gs, "dungeon_game_over")) {
            gamestate_set_flag(&gs, "dungeon_game_over", 0);
            { Dungeon* _p = dungeon_get_active(&dungeon_sys); if (_p) _p->active = 0; }
            for (int i = 0; i < party.count; i++) {
                party.members[i].hp    = party.members[i].max_hp / 4;
                party.members[i].sp    = party.members[i].max_sp / 4;
                party.members[i].alive = 1;
            }
            notify_warning(&notify,
                "You were expelled from the Hungerscape...");
        }

        // Dungeon cleared — auto-exit after post-battle dialogue
        if (gamestate_get_flag(&gs, "dungeon_cleared_exit")) {
            gamestate_set_flag(&gs, "dungeon_cleared_exit", 0);
            dungeon_exit(&dungeon_sys, &player.x, &player.y);
            notify_success(&notify, "Dungeon cleared! The party escapes safely.");
            audio_play_bgm(BGM_OVERWORLD);
        }

        // Save/Load requests from safe room
        if (gamestate_get_flag(&gs, "request_save")) {
            gamestate_set_flag(&gs, "request_save", 0);
            pause_menu_open(&pause_menu);
            pause_menu.state = PAUSE_SAVE;
        }
        if (gamestate_get_flag(&gs, "request_load")) {
            gamestate_set_flag(&gs, "request_load", 0);
            pause_menu_open(&pause_menu);
            pause_menu.state = PAUSE_LOAD;
        }

        // Post-combat
        if (cs.just_ended) {
            cs.just_ended = 0;
            int was_dungeon = (dungeon_get_active(&dungeon_sys) != NULL &&
                               dungeon_get_active(&dungeon_sys)->active);
            if (was_dungeon) {
                if (!cs.won_last_battle) audio_play_bgm(BGM_GAME_OVER);
                dungeon_on_combat_end(&dungeon_sys, cs.won_last_battle);
                if (!cs.won_last_battle) {
                    // Party wiped — show game over screen
                    g_game_over = 1;
                    // Restore party to 1 HP so they can continue if player retries
                    for (int i = 0; i < party.count; i++) {
                        party.members[i].hp    = 1;
                        party.members[i].sp    = 0;
                        party.members[i].alive = 1;
                        party.members[i].status = STATUS_NONE;
                    }
                    // Expel from dungeon
                    { Dungeon* _p = dungeon_get_active(&dungeon_sys); if (_p) _p->active = 0; }
                } else if (cs.won_last_battle) {
                    // Check if we fled from the boss — if so, expel
                    // (boss room is inaccessible after fleeing)
                    DungeonRoom* br = dungeon_current_room(&dungeon_sys);
                    if (br && br->type == ROOM_BOSS && !br->boss.defeated) {
                        // Fled from boss — leave the dungeon
                        { Dungeon* _p = dungeon_get_active(&dungeon_sys); if (_p) _p->active = 0; }
                        player.x = 200.0f;
                        player.y = 400.0f;
                        notify_warning(&notify,
                            "You fled from the ruler. Regroup and try again.");
                    }
                    gamestate_set_flag(&gs, "dungeon_game_over", 0);
                    audio_end_combat_bgm();
                }
            } else {
                if (active_enemy) {
                    if (cs.won_last_battle) active_enemy->defeated = 1;
                    player.x = active_enemy->pre_combat_x;
                    player.y = active_enemy->pre_combat_y;
                }
                active_enemy = NULL;
                if (!cs.won_last_battle) {
                    audio_play_bgm(BGM_GAME_OVER);
                    g_game_over = 1;
                }
            }
            post_combat_cooldown = 30;
        }

        // Handle game over retry load
        if (g_reload_save_slot >= 0) {
            SaveData sd;
            load_game(g_reload_save_slot, &sd);
            save_data_to_state(&sd, &party, &inventory, &gs, &ts, &social,
                               &bond_db,
                               &player.x, &player.y,
                               &rm.current,
                               &dungeon_sys);
            notify_info(&notify, "Returned to last save.");
            audio_play_bgm(BGM_OVERWORLD);
            g_reload_save_slot = -1;
        }

        if (post_combat_cooldown > 0) {
            post_combat_cooldown--;
            render(renderer, &player, &rm, &cam, &ds, font,
                   &cs, &ts, &ws, &pause_menu, &shop_menu, &social,
                   &activitydb, &activity_menu, &notify,
                   &bond_db, &bond_menu, &status_screen,
                   &equipdb, &dungeon_sys);
            SDL_Delay(16);
            continue;
        }

        if (!ds.active) {
            for (int i = 0; i < bond_db.count; i++) {
                Bond* c = &bond_db.bonds[i];
                char flag[128];
                snprintf(flag, 128, "hangout_%s", c->name);
                if (gamestate_get_flag(&gs, flag)) {
                    gamestate_set_flag(&gs, flag, 0);
                    bond_hangout(c, &social, &notify, &ts);
                }
            }
        }

        if (!ds.active && gamestate_get_flag(&gs, "open_shop")) {
            gamestate_set_flag(&gs, "open_shop", 0);
            gamestate_set_flag(&gs, "shop_choice_open", 0);
            Room* cur = roommanager_current(&rm);
            if (cur->has_shop) shopmenu_open(&shop_menu, &cur->shop);
        }


        handle_input(&event, &running, keys, &player, &rm,
                     &ds, &gs, &cs, &active_enemy,
                     &ts, &ws, &party, &pause_menu, &shop_menu,
                     &social, &activitydb, &activity_menu, &notify,
                     &bond_db, &bond_menu, &status_screen,
                     &equipdb, &dungeon_sys);

        // Dungeon tile movement — runs every frame when dungeon is active
        // and no overlay is open (same guard as overworld movement)
        if ((dungeon_get_active(&dungeon_sys) != NULL && dungeon_get_active(&dungeon_sys)->active) && !cs.active && !ds.active &&
            !any_overlay_open(&cs, &ds, &pause_menu, &shop_menu,
                               &activity_menu, &bond_menu,
                               &status_screen))
            dungeon_move(&dungeon_sys, keys,
                         &player.x, &player.y,
                         player.width, player.height);

        audio_update();
        notify_update(&notify);

        // Detect free time slot start
        if (ts.slot != g_prev_slot) {
            g_prev_slot = ts.slot;
            Dungeon* _ap = dungeon_get_active(&dungeon_sys);
            int in_dungeon = _ap && _ap->active;
            if (!in_dungeon &&
                (ts.slot == TIMESLOT_AFTERNOON   ||
                 ts.slot == TIMESLOT_AFTERSCHOOL ||
                 ts.slot == TIMESLOT_EVENING     ||
                 ts.slot == TIMESLOT_NIGHT)) {
                g_free_time_prompt   = 1;
                g_free_time_selected = 0;
            }
        }

        int force_dialogue = -1;
        eventsystem_update(&event_system, &ts, &gs, &notify,
                           &social, &ws, &force_dialogue);
        if (force_dialogue >= 0 && !ds.active && !cs.active)
            dialogue_start(&ds, force_dialogue);

        combat_update(&cs, &gs);

        if ((dungeon_get_active(&dungeon_sys) != NULL && dungeon_get_active(&dungeon_sys)->active))
            dungeon_update(&dungeon_sys, player.x, player.y);

        if (!(dungeon_get_active(&dungeon_sys) != NULL && dungeon_get_active(&dungeon_sys)->active) &&
            !any_overlay_open(&cs, &ds, &pause_menu, &shop_menu,
                               &activity_menu, &bond_menu,
                               &status_screen) &&
            transition_cooldown == 0) {
            int target_room; float spawn_x, spawn_y;
            if (triggerlist_check(&room->triggers,
                                  player.x, player.y,
                                  player.width, player.height,
                                  &target_room, &spawn_x, &spawn_y)) {
                roommanager_load(&rm, target_room, &spawn_x, &spawn_y);
                player.x = spawn_x; player.y = spawn_y;
                transition_cooldown = 60;
            }
        }

        if (transition_cooldown > 0) transition_cooldown--;

        camera_update(&cam, player.x, player.y,
                      player.width, player.height,
                      MAP_COLS * TILE_SIZE, MAP_ROWS * TILE_SIZE);

        render(renderer, &player, &rm, &cam, &ds, font,
               &cs, &ts, &ws, &pause_menu, &shop_menu, &social,
               &activitydb, &activity_menu, &notify,
               &bond_db, &bond_menu, &status_screen,
               &equipdb, &dungeon_sys);

        SDL_Delay(16);
    }

    TTF_CloseFont(font);
    SDL_DestroyRenderer(renderer);
    SDL_DestroyWindow(window);
    audio_shutdown();
    TTF_Quit(); SDL_Quit();
    return 0;
}