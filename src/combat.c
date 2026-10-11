#include "notify.h"
#include "combat.h"
#include "audio.h"
#include "summon.h"
#include "characters.h"
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <math.h>

// ─── Helpers ────────────────────────────────────────────────────────────────

void combat_log(CombatSystem* cs, const char* msg) {
    if (cs->log_count == LOG_LINES) {
        for (int i = 0; i < LOG_LINES - 1; i++)
            strcpy(cs->log[i], cs->log[i+1]);
        cs->log_count--;
    }
    strncpy(cs->log[cs->log_count++], msg, LOG_LINE_LEN - 1);
}

static void draw_text(SDL_Renderer* r, TTF_Font* font,
                      const char* text, SDL_Color color, int x, int y) {
    if (!text || text[0] == '\0') return;
    SDL_Surface* surf = TTF_RenderText_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_Rect dst = { x, y, surf->w, surf->h };
    SDL_RenderCopy(r, tex, NULL, &dst);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

static void draw_rect(SDL_Renderer* r, int x, int y, int w, int h,
                      Uint8 rr, Uint8 g, Uint8 b, Uint8 a) {
    SDL_SetRenderDrawColor(r, rr, g, b, a);
    SDL_Rect rect = { x, y, w, h };
    SDL_RenderFillRect(r, &rect);
}

static void draw_border(SDL_Renderer* r, int x, int y, int w, int h,
                        Uint8 rr, Uint8 g, Uint8 b, Uint8 a) {
    SDL_SetRenderDrawColor(r, rr, g, b, a);
    SDL_Rect rect = { x, y, w, h };
    SDL_RenderDrawRect(r, &rect);
}

static void draw_hp_bar(SDL_Renderer* r, int x, int y, int w,
                        int hp, int max_hp) {
    draw_rect(r, x, y, w, 8, 40, 10, 10, 255);
    if (max_hp > 0) {
        int fill = (hp * w) / max_hp;
        draw_rect(r, x, y, fill, 8, 60, 200, 60, 255);
    }
}

static void draw_sp_bar(SDL_Renderer* r, int x, int y, int w,
                        int sp, int max_sp) {
    draw_rect(r, x, y, w, 8, 10, 10, 60, 255);
    if (max_sp > 0) {
        int fill = (sp * w) / max_sp;
        draw_rect(r, x, y, fill, 8, 60, 100, 220, 255);
    }
}

// ─── Turn Order ─────────────────────────────────────────────────────────────

static int get_ag(CombatSystem* cs, int idx) {
    if (idx < PARTY_MAX)
        return cs->party->members[idx].ag;
    else
        return cs->enemies[idx - PARTY_MAX].stats.ag;
}

static void build_turn_order(CombatSystem* cs) {
    int total = 0;
    // Only include alive party members with valid names
    for (int i = 0; i < cs->party->count; i++) {
        if (cs->party->members[i].alive &&
            cs->party->members[i].name[0] != '\0')
            cs->turn_order[total++] = i;
    }
    for (int i = 0; i < cs->enemy_count; i++)
        cs->turn_order[total++] = PARTY_MAX + i;
    cs->turn_count = total;
    for (int i = 0; i < total - 1; i++)
        for (int j = 0; j < total - i - 1; j++)
            if (get_ag(cs, cs->turn_order[j]) <
                get_ag(cs, cs->turn_order[j+1])) {
                int tmp = cs->turn_order[j];
                cs->turn_order[j] = cs->turn_order[j+1];
                cs->turn_order[j+1] = tmp;
            }
    cs->turn_index = 0;
}

static int current_turn_idx(CombatSystem* cs) {
    return cs->turn_order[cs->turn_index];
}

static int is_party_turn(CombatSystem* cs) {
    return current_turn_idx(cs) < PARTY_MAX;
}

static void advance_turn(CombatSystem* cs) {
    int idx = current_turn_idx(cs);

    int chain_continues = 0;
    if (idx < PARTY_MAX && cs->party->members[idx].extra_turn)
        chain_continues = 1;

    if (idx < PARTY_MAX) {
        StatBlock* sb = &cs->party->members[idx];
        sb->extra_turn = 0;
        sb->can_hand_off = 0;
        if (sb->handoff_level > 0) {
            int recover_hp = sb->max_hp / 10;
            int recover_sp = sb->max_sp / 20;
            sb->hp += recover_hp;
            if (sb->hp > sb->max_hp) sb->hp = sb->max_hp;
            sb->sp += recover_sp;
            if (sb->sp > sb->max_sp) sb->sp = sb->max_sp;
            sb->handoff_level = 0;
        }
        sb->handoff_sp_discount = 0;
    } else {
        cs->enemies[idx - PARTY_MAX].stats.extra_turn = 0;
    }

    // Reset handoff chain if it didn't continue
    if (!chain_continues)
        cs->handoff_chain_count = 0;

    cs->turn_index = (cs->turn_index + 1) % cs->turn_count;
    int loops = 0;
    while (loops < cs->turn_count) {
        int cidx = current_turn_idx(cs);
        int alive = (cidx < PARTY_MAX)
                    ? cs->party->members[cidx].alive
                    : cs->enemies[cidx - PARTY_MAX].stats.alive;
        if (alive) break;
        cs->turn_index = (cs->turn_index + 1) % cs->turn_count;
        loops++;
    }
}

// ─── Win/Lose checks ────────────────────────────────────────────────────────

static int all_enemies_dead(CombatSystem* cs) {
    for (int i = 0; i < cs->enemy_count; i++)
        if (cs->enemies[i].stats.alive) return 0;
    return 1;
}

static int all_enemies_knocked_down(CombatSystem* cs) {
    for (int i = 0; i < cs->enemy_count; i++)
        if (cs->enemies[i].stats.alive &&
            !cs->enemies[i].stats.knocked_down) return 0;
    return 1;
}

// ─── Loot helpers ────────────────────────────────────────────────────────────

static int pick_loot(CombatEnemy* ce) {
    if (ce->loot_count == 0) return -1;
    int pool[MAX_LOOT_ENTRIES * 10];
    int pool_size = 0;
    for (int i = 0; i < ce->loot_count; i++) {
        int slots = ce->loot_chances[i] / 10;
        if (slots < 1) slots = 1;
        for (int w = 0; w < slots &&
             pool_size < (int)(sizeof(pool)/sizeof(pool[0])); w++)
            pool[pool_size++] = i;
    }
    if (pool_size == 0) return -1;
    return ce->loot_ids[pool[rand() % pool_size]];
}

static void try_steal(CombatSystem* cs, CombatEnemy* ce,
                      StatBlock* user, int steal_chance) {
    if (ce->loot_count == 0) return;
    if ((rand() % 100) >= steal_chance) return;
    int item_id = pick_loot(ce);
    if (item_id < 0 || !cs->inventory || !cs->itemdb) return;
    Item* item = itemdb_get(cs->itemdb, item_id);
    if (!item) return;
    inventory_add(cs->inventory, item_id, 1);
    char msg[LOG_LINE_LEN];
    snprintf(msg, LOG_LINE_LEN, "%s stole %s!", user->name, item->name);
    combat_log(cs, msg);
    if (cs->notify) notify_success(cs->notify, msg);
}

static void roll_drops(CombatSystem* cs, CombatEnemy* ce) {
    if (ce->loot_count == 0) return;
    if (!cs->inventory || !cs->itemdb) return;
    for (int i = 0; i < ce->loot_count; i++) {
        if ((rand() % 100) < ce->loot_chances[i]) {
            Item* item = itemdb_get(cs->itemdb, ce->loot_ids[i]);
            if (!item) continue;
            inventory_add(cs->inventory, ce->loot_ids[i], 1);
            char msg[LOG_LINE_LEN];
            snprintf(msg, LOG_LINE_LEN, "Got %s!", item->name);
            combat_log(cs, msg);
            if (cs->notify) notify_info(cs->notify, msg);
        }
    }
}

// ─── Init / Start ────────────────────────────────────────────────────────────

void combat_init(CombatSystem* cs, Party* party, SkillDB* skilldb,
                 GameState* gs, Inventory* inventory, ItemDB* itemdb,
                 NotifySystem* notify, BondDB* bonds) {
    memset(cs, 0, sizeof(CombatSystem));
    cs->party      = party;
    cs->skilldb    = skilldb;
    cs->gs         = gs;
    cs->inventory  = inventory;
    cs->itemdb     = itemdb;
    cs->notify     = notify;
    cs->bonds = bonds;
    cs->active     = 0;
    srand((unsigned)time(NULL));
}

void combat_start(CombatSystem* cs, CombatEnemy* enemies, int count,
                  const char* win_flag) {
    fprintf(stderr, "COMBAT_START: count=%d win_flag='%s' first_enemy='%s'\n",
    count, win_flag, count > 0 ? enemies[0].stats.name : "(none)");
    cs->enemy_count = count;
    cs->enemy_count          = count;
    cs->active               = 1;
    cs->just_ended           = 0;
    cs->won_last_battle      = 0;
    cs->log_count            = 0;
    cs->menu_selected        = 0;
    cs->skill_selected       = 0;
    cs->item_selected        = 0;
    cs->target_selected      = 0;
    cs->ally_target_selected = 0;
    cs->team_attack_available    = 0;
    cs->learn_count          = 0;
    cs->learn_index          = 0;
    cs->handoff_chain_count    = 0;
    strncpy(cs->win_flag, win_flag, 63);
    for (int i = 0; i < count; i++)
        cs->enemies[i] = enemies[i];
    for (int i = 0; i < cs->party->count; i++) {
        cs->party->members[i].knocked_down = 0;
        cs->party->members[i].extra_turn     = 0;
        cs->party->members[i].can_hand_off    = 0;
        cs->party->members[i].handoff_level       = 0;
        cs->party->members[i].handoff_sp_discount = 0;
        // Revive members who are down — every battle starts fresh
        if (!cs->party->members[i].alive) {
            cs->party->members[i].alive = 1;
            cs->party->members[i].hp = 1;
        }
    }
    build_turn_order(cs);
    // Set state based on whoever's actually first in turn order
    if (is_party_turn(cs))
        cs->state = COMBAT_PLAYER_TURN;
    else
        cs->state = COMBAT_ENEMY_TURN;
    combat_log(cs, "A battle begins!");
    audio_start_combat_bgm(0, 0);
}

CombatEnemy make_combat_enemy(const char* name,
                              int hp, int sp,
                              int st, int ma, int en, int ag, int lu,
                              int exp_reward,
                              const char* win_flag) {
    CombatEnemy e;
    memset(&e, 0, sizeof(CombatEnemy));
    statblock_init(&e.stats, name);
    e.stats.max_hp = e.stats.hp = hp;
    e.stats.max_sp = e.stats.sp = sp;
    e.stats.st = st; e.stats.ma = ma; e.stats.en = en;
    e.stats.ag = ag; e.stats.lu = lu;
    e.exp_reward = exp_reward;
    e.loot_count = 0;
    strncpy(e.win_flag, win_flag, 63);
    return e;
}

// ─── Extra Turn Helper ───────────────────────────────────────────────────────

static int roll_extra_turn(CombatSystem* cs) {
    if (!cs->bonds) return 0;
    int chance = bond_get_perk_value(cs->bonds,
                                           PERK_EXTRA_TURN_CHANCE);
    if (chance <= 0) return 0;
    return (rand() % 100) < chance;
}


// ─── Turn Start Status Processing ────────────────────────────────────────────
// Returns 0 = act normally, 1 = skip turn, 2 = confused (attack random ally)

typedef enum {
    TURN_NORMAL  = 0,
    TURN_SKIP    = 1,
    TURN_CONFUSE = 2
} TurnStartResult;

static TurnStartResult process_turn_start(CombatSystem* cs, StatBlock* sb) {
    if (sb->status == STATUS_NONE) return TURN_NORMAL;

    char msg[LOG_LINE_LEN];
    TurnStartResult result = TURN_NORMAL;

    switch (sb->status) {

        case STATUS_BURN:
            // Deals damage each turn
            {
                int dmg = sb->ma / 8 + 1;
                sb->hp -= dmg;
                if (sb->hp < 0) sb->hp = 0;
                snprintf(msg, LOG_LINE_LEN, "%s takes %d burn damage!",
                         sb->name, dmg);
                combat_log(cs, msg);
            }
            break;

        case STATUS_FREEZE:
            // Always skips turn
            snprintf(msg, LOG_LINE_LEN, "%s is frozen and can't move!", sb->name);
            combat_log(cs, msg);
            result = TURN_SKIP;
            break;

        case STATUS_SHOCK:
            // 50% chance to skip
            if (rand() % 2 == 0) {
                snprintf(msg, LOG_LINE_LEN, "%s is shocked and can't move!", sb->name);
                combat_log(cs, msg);
                result = TURN_SKIP;
            }
            break;

        case STATUS_DIZZY:
            // Acts normally but attacks may miss (handled in execute_skill)
            snprintf(msg, LOG_LINE_LEN, "%s is dizzy!", sb->name);
            combat_log(cs, msg);
            break;

        case STATUS_SLEEP:
            // Skips turn, restores a little HP
            {
                int heal = sb->max_hp / 10;
                sb->hp += heal;
                if (sb->hp > sb->max_hp) sb->hp = sb->max_hp;
                snprintf(msg, LOG_LINE_LEN,
                         "%s is asleep and recovers %d HP!", sb->name, heal);
                combat_log(cs, msg);
                result = TURN_SKIP;
            }
            break;

        case STATUS_CONFUSE:
            // 50% chance to act confused
            if (rand() % 2 == 0) {
                snprintf(msg, LOG_LINE_LEN, "%s is confused!", sb->name);
                combat_log(cs, msg);
                result = TURN_CONFUSE;
            }
            break;

        case STATUS_FEAR:
            // 30% chance to cower and skip
            if (rand() % 10 < 3) {
                snprintf(msg, LOG_LINE_LEN, "%s is too afraid to act!", sb->name);
                combat_log(cs, msg);
                result = TURN_SKIP;
            }
            break;

        case STATUS_DESPAIR:
            // Drains SP each turn, can't use skills (enforced in input handler)
            {
                int drain = sb->max_sp / 10 + 1;
                sb->sp -= drain;
                if (sb->sp < 0) sb->sp = 0;
                snprintf(msg, LOG_LINE_LEN,
                         "%s despairs — loses %d SP!", sb->name, drain);
                combat_log(cs, msg);
            }
            break;

        case STATUS_BRAINWASH:
            // 50% chance to act against own side (handled as CONFUSE equivalent)
            if (rand() % 2 == 0) {
                snprintf(msg, LOG_LINE_LEN, "%s is brainwashed!", sb->name);
                combat_log(cs, msg);
                result = TURN_CONFUSE;
            }
            break;

        case STATUS_FORGET:
            // Can only use basic attack (enforced in input handler)
            snprintf(msg, LOG_LINE_LEN, "%s forgot their skills!", sb->name);
            combat_log(cs, msg);
            break;

        default: break;
    }

    // Tick down the status counter
    statblock_process_status(sb);
    return result;
}

// ─── Skill Execution ─────────────────────────────────────────────────────────

static void execute_skill(CombatSystem* cs, StatBlock* user,
                          Skill* skill, StatBlock* target,
                          int user_is_party, int target_enemy_idx) {
    char msg[LOG_LINE_LEN];

    // Consume any incoming extra_turn / can_hand_off — this action starts
    // fresh. They'll re-set below only if a weakness/crit is earned now.
    user->extra_turn = 0;
    user->can_hand_off = 0;

    if (skill->is_heal) {
        int heal = (user->ma * skill->power) / 10;
        target->hp += heal;
        if (target->hp > target->max_hp) target->hp = target->max_hp;
        snprintf(msg, LOG_LINE_LEN, "%s heals %s for %d HP!",
                 user->name, target->name, heal);
        combat_log(cs, msg);
        return;
    }

    int dmg = 0;
    int is_phys = (skill->element == ELEM_PHYSICAL ||
                   skill->element == ELEM_GUN);

    if (is_phys)
        dmg = statblock_phys_damage(user, target, skill->power);
    else
        dmg = statblock_magic_damage(user, target, skill->power,
                                     skill->element);

    if (cs->bonds && target->status != STATUS_NONE) {
        int tech = bond_get_perk_value(cs->bonds,
                                             PERK_TECHNICAL_BONUS);
        if (tech > 0) dmg = dmg + (dmg * tech / 100);
    }

    Affinity aff = statblock_get_affinity(target, skill->element);

    if (skill->element == ELEM_GUN && aff == AFF_RESIST &&
        cs->bonds &&
        bond_has_perk(cs->bonds, PERK_GUN_IGNORE_RESIST))
        aff = AFF_NORMAL;

    if (cs->gs)
        gamestate_discover_affinity(cs->gs, target->name, skill->element);

    // Dizzy causes 30% miss chance
    if (user->status == STATUS_DIZZY && (rand() % 10) < 3) {
        char miss_msg[LOG_LINE_LEN];
        snprintf(miss_msg, LOG_LINE_LEN, "%s missed due to dizziness!",
                 user->name);
        combat_log(cs, miss_msg);
        return;
    }

    int hit_weakness = (aff == AFF_WEAK);
    int critical     = 0;

    if (is_phys) {
        int crit_chance = user->lu / 5 + 5;
        critical = (rand() % 100) < crit_chance;
        if (critical) dmg = dmg * 3 / 2;
    }

    if (aff == AFF_NULL) {
        snprintf(msg, LOG_LINE_LEN, "%s nulls the attack!", target->name);
        combat_log(cs, msg); return;
    }
    if (aff == AFF_REPEL) {
        snprintf(msg, LOG_LINE_LEN, "%s repels the attack!", target->name);
        combat_log(cs, msg);
        user->hp -= abs(dmg);
        if (user->hp < 0) user->hp = 0;
        if (user->hp == 0) {
            user->alive = 0;
            if (strcmp(user->name, "Prota") == 0)
                cs->state = COMBAT_LOSE;
        }
        return;
    }
    if (aff == AFF_ABSORB) {
        target->hp += abs(dmg);
        if (target->hp > target->max_hp) target->hp = target->max_hp;
        snprintf(msg, LOG_LINE_LEN, "%s absorbs the attack!", target->name);
        combat_log(cs, msg); return;
    }

    // Sanity clamp — guards against integer overflow / negative damage corruption
    if (dmg < 0) dmg = 0;
    if (dmg > 99999) dmg = 99999;
    target->hp -= dmg;
    if (target->hp < 0) target->hp = 0;
    if (target->hp > target->max_hp) target->hp = target->max_hp;

    // Freeze and Shock break on physical contact — also wakes sleeping targets
    if (is_phys && (target->status == STATUS_FREEZE ||
                    target->status == STATUS_SHOCK  ||
                    target->status == STATUS_SLEEP)) {
        char break_msg[LOG_LINE_LEN];
        snprintf(break_msg, LOG_LINE_LEN, "%s snapped out of %s!",
                 target->name, status_name(target->status));
        combat_log(cs, break_msg);
        statblock_clear_status(target);
    }

    if (critical)
        snprintf(msg, LOG_LINE_LEN, "Critical! %s hits %s for %d %s dmg!",
                 user->name, target->name, dmg, element_name(skill->element));
    else if (hit_weakness)
        snprintf(msg, LOG_LINE_LEN, "Weak! %s hits %s for %d %s dmg!",
                 user->name, target->name, dmg, element_name(skill->element));
    else
        snprintf(msg, LOG_LINE_LEN, "%s hits %s for %d %s dmg.",
                 user->name, target->name, dmg, element_name(skill->element));
    combat_log(cs, msg);

    // Steal on physical hit
    if (is_phys && user_is_party && target->alive &&
        target_enemy_idx >= 0 && cs->bonds) {
        int steal = bond_get_perk_value(cs->bonds, PERK_STEAL_CHANCE);
        if (steal > 0)
            try_steal(cs, &cs->enemies[target_enemy_idx], user, steal);
    }

    if (dmg > 0) {
        if (hit_weakness) audio_play_sfx(SFX_WEAKNESS_HIT);
        else              audio_play_sfx(SFX_ATTACK_HIT);
    } else {
        audio_play_sfx(SFX_MISS);
    }

    // Knockdown + Extra Turn
    // Only grant Extra Turn if the target wasn't already knocked down —
    // chaining Extra Turns on a downed enemy is not allowed.
    int was_knocked_down = target->knocked_down;
    if ((hit_weakness || critical) && target->alive) {
        target->knocked_down = 1;
        // Only grant Extra Turn if target wasn't already down
        if (!was_knocked_down) {
            user->extra_turn = 1;
            user->can_hand_off = 1;
            audio_play_sfx(SFX_EXTRA_TURN);
            combat_log(cs, "Extra Turn!");
        }

        if (user_is_party && cs->bonds) {
            int followup = bond_get_perk_value(cs->bonds,
                                                     PERK_FOLLOWUP_ATTACK);
            if (followup > 0 && (rand() % 100) < followup && target->alive) {
                int fdmg = statblock_phys_damage(user, target, 8);
                target->hp -= fdmg;
                if (target->hp < 0) target->hp = 0;
                snprintf(msg, LOG_LINE_LEN,
                         "Follow-up! %s strikes for %d more damage!",
                         user->name, fdmg);
                combat_log(cs, msg);
                if (target->hp == 0) {
                    target->alive = 0;
                    if (strcmp(target->name, "Prota") == 0)
                        cs->state = COMBAT_LOSE;
                    else if (target_enemy_idx >= 0)
                        roll_drops(cs, &cs->enemies[target_enemy_idx]);
                }
            }
        }
    }

    // Status infliction
    if (skill->inflicts != STATUS_NONE && target->alive) {
        int apply = 0;
        if (user_is_party && cs->bonds &&
            bond_has_perk(cs->bonds, PERK_STATUS_ALWAYS_HIT))
            apply = 1;
        else
            apply = (rand() % 100) < (int)(skill->status_chance * 100);

        if (apply) {
            int duration = 3;
            if (user_is_party && cs->bonds)
                duration += bond_get_perk_value(cs->bonds,
                                                      PERK_STATUS_DURATION);
            statblock_apply_status(target, skill->inflicts, duration);
            snprintf(msg, LOG_LINE_LEN, "%s is %s!",
                     target->name, status_name(skill->inflicts));
            combat_log(cs, msg);
        }
    }

    // Death + drops
    if (target->hp == 0) {
        target->alive = 0;
        if (strcmp(target->name, "Prota") == 0)
            cs->state = COMBAT_LOSE;
        else if (target_enemy_idx >= 0)
            roll_drops(cs, &cs->enemies[target_enemy_idx]);
    }
}

// ─── Enemy AI ────────────────────────────────────────────────────────────────

static void enemy_take_turn(CombatSystem* cs, int enemy_idx,
                             GameState* gs) {
    CombatEnemy* ce = &cs->enemies[enemy_idx];
    StatBlock* enemy = &ce->stats;
    char msg[LOG_LINE_LEN];

    {
        TurnStartResult tsr = process_turn_start(cs, enemy);
        if (tsr == TURN_SKIP) {
            advance_turn(cs);
            cs->state = is_party_turn(cs) ?
                        COMBAT_PLAYER_TURN : COMBAT_ENEMY_TURN;
            return;
        }
        if (tsr == TURN_CONFUSE) {
            // Confused enemy attacks a random ally (another enemy)
            int target_idx = -1;
            for (int attempts = 0; attempts < 10; attempts++) {
                int t = rand() % cs->enemy_count;
                if (cs->enemies[t].stats.alive && t != enemy_idx) {
                    target_idx = t; break;
                }
            }
            if (target_idx >= 0) {
                int dmg = statblock_phys_damage(enemy,
                              &cs->enemies[target_idx].stats, 10);
                cs->enemies[target_idx].stats.hp -= dmg;
                if (cs->enemies[target_idx].stats.hp < 0)
                    cs->enemies[target_idx].stats.hp = 0;
                snprintf(msg, LOG_LINE_LEN,
                         "%s attacks ally %s for %d!",
                         enemy->name,
                         cs->enemies[target_idx].stats.name, dmg);
                combat_log(cs, msg);
                if (cs->enemies[target_idx].stats.hp == 0)
                    cs->enemies[target_idx].stats.alive = 0;
            }
            advance_turn(cs);
            cs->state = is_party_turn(cs) ?
                        COMBAT_PLAYER_TURN : COMBAT_ENEMY_TURN;
            return;
        }
        // TURN_NORMAL — check if dead from burn damage
        if (!enemy->alive || enemy->hp == 0) {
            enemy->alive = 0;
            roll_drops(cs, ce);
            advance_turn(cs);
            cs->state = is_party_turn(cs) ?
                        COMBAT_PLAYER_TURN : COMBAT_ENEMY_TURN;
            return;
        }
    }

    int target_idx = -1;
    for (int attempts = 0; attempts < 10; attempts++) {
        int t = rand() % cs->party->count;
        if (cs->party->members[t].alive) { target_idx = t; break; }
    }
    if (target_idx == -1) return;

    int chosen_skill = -1, chosen_target = target_idx;

    if (enemy->skill_count > 0 && cs->skilldb) {
        for (int si = 0; si < enemy->skill_count && chosen_skill == -1; si++) {
            Skill* sk = skilldb_get(cs->skilldb, enemy->skill_ids[si]);
            if (!sk || sk->is_heal || sk->sp_cost > enemy->sp) continue;
            for (int pi = 0; pi < cs->party->count; pi++) {
                if (!cs->party->members[pi].alive) continue;
                if (statblock_get_affinity(&cs->party->members[pi],
                                            sk->element) == AFF_WEAK) {
                    chosen_skill = si; chosen_target = pi; break;
                }
            }
        }
        if (chosen_skill == -1) {
            int usable[8], uc = 0;
            for (int si = 0; si < enemy->skill_count; si++) {
                Skill* sk = skilldb_get(cs->skilldb, enemy->skill_ids[si]);
                if (!sk || sk->is_heal || sk->sp_cost > enemy->sp) continue;
                usable[uc++] = si;
            }
            if (uc > 0) chosen_skill = usable[rand() % uc];
        }
    }

    if (chosen_skill >= 0 && cs->skilldb) {
        Skill* sk = skilldb_get(cs->skilldb, enemy->skill_ids[chosen_skill]);
        if (sk) {
            enemy->sp -= sk->sp_cost;
            if (sk->target == TARGET_ALL_ALLIES ||
                sk->target == TARGET_ALL_ENEMIES) {
                for (int pi = 0; pi < cs->party->count; pi++) {
                    if (!cs->party->members[pi].alive) continue;
                    execute_skill(cs, enemy, sk,
                                 &cs->party->members[pi], 0, -1);
                }
            } else {
                execute_skill(cs, enemy, sk,
                             &cs->party->members[chosen_target], 0, -1);
            }
        }
    } else {
        StatBlock* target = &cs->party->members[target_idx];
        int dmg = statblock_phys_damage(enemy, target, 10);
        target->hp -= dmg;
        if (target->hp < 0) target->hp = 0;
        snprintf(msg, LOG_LINE_LEN, "%s attacks %s for %d damage!",
                 enemy->name, target->name, dmg);
        combat_log(cs, msg);
        if (target->hp == 0) {
            target->alive = 0;
            snprintf(msg, LOG_LINE_LEN, "%s is down!", target->name);
            combat_log(cs, msg);
            if (strcmp(target->name, "Prota") == 0) {
                cs->state = COMBAT_LOSE;
                combat_log(cs, "Prota has fallen. Game Over.");
                return;
            }
        }
    }

    advance_turn(cs);

    if (party_all_dead(cs->party)) {
        cs->state = COMBAT_LOSE;
        combat_log(cs, "The party has fallen...");
        return;
    }
    cs->state = is_party_turn(cs) ? COMBAT_PLAYER_TURN : COMBAT_ENEMY_TURN;
}

// ─── EXP + Skill Learn Queue ─────────────────────────────────────────────────

static void apply_exp_rewards(CombatSystem* cs, NotifySystem* ns) {
    int total_exp = 0;
    int total_yen = 0;
    for (int i = 0; i < cs->enemy_count; i++) {
        total_exp += cs->enemies[i].exp_reward;
        total_yen += (cs->enemies[i].exp_reward * 60) / 100;
    }

    // Award yen (shown in victory panel, not combat log)
    if (total_yen > 0 && cs->inventory)
        cs->inventory->yen += total_yen;

    if (cs->bonds) {
        int bonus = bond_get_exp_bonus(cs->bonds);
        if (bonus > 0)
            total_exp = total_exp + (total_exp * bonus / 100);
    }

    cs->learn_count = 0;
    cs->learn_index = 0;

    for (int i = 0; i < cs->party->count; i++) {
        if (!cs->party->members[i].alive) continue;
        StatBlock* m = &cs->party->members[i];
        int old_level   = m->level;
        int levels_gained = statblock_add_exp(m, total_exp);

        if (levels_gained > 0) {
            char msg[LOG_LINE_LEN];
            snprintf(msg, LOG_LINE_LEN, "%s leveled up to %d!",
                     m->name, m->level);
            combat_log(cs, msg);
            if (ns) notify_success(ns, msg);

            // Check every level crossed for a skill learn — one per level only
            for (int lv = old_level + 1;
                 lv <= m->level && cs->learn_count < MAX_PENDING_LEARNS; lv++) {
                int new_skill = characters_skill_at_level(m->name, lv);
                if (new_skill < 0) continue;

                // Don't offer if already known
                int already_known = 0;
                for (int s = 0; s < m->skill_count; s++)
                    if (m->skill_ids[s] == new_skill) {
                        already_known = 1; break;
                    }
                if (already_known) continue;

                PendingLearn* pl = &cs->learn_queue[cs->learn_count++];
                pl->party_member_idx = i;
                pl->new_skill_id     = new_skill;
                pl->replace_selected = 0;
                pl->choosing_replace = 0;
            }
        }
    }
}


// ─── Enemy Negotiation ────────────────────────────────────────────────────────
// Full Summon 5-style negotiation.
// Only available when exactly one enemy is alive.
// Enemy personality (derived from their stats) determines dialogue and demands.
// HP percentage affects success chance — lower HP = easier to negotiate.
//
// Personality types (derived from enemy stats):
//   Upbeat   (high LU)  — cheerful, wants flattery or money
//   Irritable (high ST) — aggressive, wants items or threatens to fight
//   Timid    (high EN)  — nervous, easily pleased, low demands
//   Gloomy   (high MA)  — melancholy, philosophical, unpredictable

typedef enum {
    PERSONALITY_UPBEAT,
    PERSONALITY_IRRITABLE,
    PERSONALITY_TIMID,
    PERSONALITY_GLOOMY
} EnemyPersonality;

static EnemyPersonality get_personality(StatBlock* sb) {
    int max_stat = sb->lu;
    EnemyPersonality p = PERSONALITY_UPBEAT;
    if (sb->st > max_stat) { max_stat = sb->st; p = PERSONALITY_IRRITABLE; }
    if (sb->en > max_stat) { max_stat = sb->en; p = PERSONALITY_TIMID;     }
    if (sb->ma > max_stat) {                     p = PERSONALITY_GLOOMY;    }
    return p;
}

static const char* personality_name(EnemyPersonality p) {
    switch (p) {
        case PERSONALITY_UPBEAT:    return "Upbeat";
        case PERSONALITY_IRRITABLE: return "Irritable";
        case PERSONALITY_TIMID:     return "Timid";
        case PERSONALITY_GLOOMY:    return "Gloomy";
        default:                    return "Unknown";
    }
}

// Generate what the enemy says at the start of negotiation
static const char* neg_greeting(StatBlock* sb, EnemyPersonality p) {
    int hp_pct = (sb->hp * 100) / (sb->max_hp > 0 ? sb->max_hp : 1);
    switch (p) {
        case PERSONALITY_UPBEAT:
            if (hp_pct < 30)
                return "H-hey, let's not be hasty! I'm sure we can work something out!";
            return "Oh! You want to talk? I love making new friends!";
        case PERSONALITY_IRRITABLE:
            if (hp_pct < 30)
                return "Tch... You got lucky. What do you want?";
            return "What?! You dare interrupt my battle with chitchat?!";
        case PERSONALITY_TIMID:
            if (hp_pct < 30)
                return "P-please don't hurt me anymore... I'll do anything!";
            return "Eek! Are you... talking to me? Um... okay...";
        case PERSONALITY_GLOOMY:
            if (hp_pct < 30)
                return "...Even if you defeat me, what does it change? Nothing lasts.";
            return "...What do you want? I was just thinking about the futility of existence.";
        default:
            return "...";
    }
}

// Generate what the enemy demands
static void neg_set_demand(CombatSystem* cs, int enemy_idx) {
    CombatEnemy*     ce = &cs->enemies[enemy_idx];
    StatBlock*       sb = &ce->stats;
    EnemyPersonality p  = get_personality(sb);

    // Base demand on personality and enemy level
    int level  = sb->level > 0 ? sb->level : 1;
    int hp_pct = (sb->hp * 100) / (sb->max_hp > 0 ? sb->max_hp : 1);

    // Weaker enemies demand less
    int demand_scale = hp_pct > 50 ? 100 : 50;

    switch (p) {
        case PERSONALITY_UPBEAT:
            // Wants money or compliments
            if (rand() % 2 == 0) {
                cs->neg_demand_type   = 0;  // money
                cs->neg_demand_amount = (level * 80 * demand_scale) / 100;
            } else {
                cs->neg_demand_type   = 2;  // flattery
                cs->neg_demand_amount = 0;
            }
            break;
        case PERSONALITY_IRRITABLE:
            // Wants an item or threatens
            if (rand() % 2 == 0) {
                cs->neg_demand_type   = 1;  // item
                cs->neg_demand_item   = 0;  // Medicine
                cs->neg_demand_amount = 1;
            } else {
                cs->neg_demand_type   = 0;  // money
                cs->neg_demand_amount = (level * 120 * demand_scale) / 100;
            }
            break;
        case PERSONALITY_TIMID:
            // Very small demands — just wants to be treated kindly
            if (rand() % 3 == 0) {
                cs->neg_demand_type   = 2;  // flattery
                cs->neg_demand_amount = 0;
            } else {
                cs->neg_demand_type   = 0;  // small amount of money
                cs->neg_demand_amount = (level * 40 * demand_scale) / 100;
                if (cs->neg_demand_amount < 50) cs->neg_demand_amount = 50;
            }
            break;
        case PERSONALITY_GLOOMY:
            // Philosophical demands — either nothing or a lot
            if (rand() % 2 == 0) {
                cs->neg_demand_type   = 2;  // existential flattery
                cs->neg_demand_amount = 0;
            } else {
                cs->neg_demand_type   = 1;  // item
                cs->neg_demand_item   = 10; // Snuff Soul
                cs->neg_demand_amount = 1;
            }
            break;
    }
}

static const char* neg_demand_text(CombatSystem* cs, ItemDB* db) {
    static char buf[128];
    switch (cs->neg_demand_type) {
        case 0:
            snprintf(buf, 128, "Give me %d yen.", cs->neg_demand_amount);
            return buf;
        case 1: {
            Item* item = itemdb_get(db, cs->neg_demand_item);
            snprintf(buf, 128, "Give me %s.", item ? item->name : "an item");
            return buf;
        }
        case 2:
            return "Say something nice about me.";
        default:
            return "...";
    }
}

// Calculate success chance based on HP%, party LU, and personality
static int neg_success_chance(CombatSystem* cs, int enemy_idx) {
    CombatEnemy* ce  = &cs->enemies[enemy_idx];
    StatBlock*   sb  = &ce->stats;
    int hp_pct = (sb->hp * 100) / (sb->max_hp > 0 ? sb->max_hp : 1);

    // Base: 30% at full HP, 70% at near-dead
    int base = 30 + ((100 - hp_pct) * 40) / 100;

    // Party luck bonus
    int avg_lu = 0;
    for (int i = 0; i < cs->party->count; i++)
        avg_lu += cs->party->members[i].lu;
    if (cs->party->count > 0) avg_lu /= cs->party->count;
    base += avg_lu / 5;

    // Personality modifier
    EnemyPersonality p = get_personality(sb);
    switch (p) {
        case PERSONALITY_TIMID:    base += 15; break;
        case PERSONALITY_GLOOMY:   base -= 5;  break;
        case PERSONALITY_IRRITABLE:base -= 10; break;
        default: break;
    }

    if (base < 5)   base = 5;
    if (base > 90)  base = 90;
    return base;
}

int combat_handoff_available(CombatSystem* cs, int from_idx) {
    if (!cs || from_idx < 0 || from_idx >= cs->party->count) return 0;
    return cs->party->members[from_idx].extra_turn ? 1 : 0;
}

int combat_hand_off(CombatSystem* cs, int from_idx, int to_idx) {
    if (!cs) return 0;
    if (from_idx < 0 || from_idx >= cs->party->count) return 0;
    if (to_idx   < 0 || to_idx   >= cs->party->count) return 0;
    if (from_idx == to_idx) return 0;
    StatBlock* from_sb = &cs->party->members[from_idx];
    StatBlock* to_sb   = &cs->party->members[to_idx];
    if (!from_sb->can_hand_off) return 0;
    if (!to_sb->alive) return 0;

    if (cs->handoff_chain_count >= 3) {
        combat_log(cs, "Hand-off chain limit reached!");
        return 0;
    }

    from_sb->extra_turn = 0;
    from_sb->can_hand_off = 0;
    int new_level = from_sb->handoff_level + 1;
    if (new_level > 3) new_level = 3;
    to_sb->handoff_level       = new_level;
    to_sb->handoff_sp_discount = 1;
    from_sb->handoff_level     = 0;

    int new_chain = cs->handoff_chain_count + 1;

    // Place receiver as next actor in turn order
    int cur_pos = cs->turn_index;
    for (int i = 0; i < PARTY_MAX + MAX_ENEMIES; i++) {
        if (cs->turn_order[i] == to_idx) {
            int target = (cur_pos + 1) % (PARTY_MAX + MAX_ENEMIES);
            int tmp = cs->turn_order[target];
            cs->turn_order[target] = to_idx;
            cs->turn_order[i] = tmp;
            break;
        }
    }
    advance_turn(cs);
    cs->state = COMBAT_PLAYER_TURN;
    cs->menu_selected = 0;

    // Restore chain count (advance_turn would have reset it because the
    // sender's extra_turn was already cleared by this point)
    cs->handoff_chain_count = new_chain;

    char msg[80];
    snprintf(msg, 80, "%s passed to %s! Boost x%.1f (chain %d/3)",
             from_sb->name, to_sb->name, new_level * 0.5f + 1.0f,
             cs->handoff_chain_count);
    combat_log(cs, msg);
    return 1;
}

static void start_negotiation(CombatSystem* cs, int enemy_idx) {
    cs->state         = COMBAT_NEGOTIATE;
    cs->neg_enemy_idx = enemy_idx;
    cs->neg_phase     = 0;  // greeting
    cs->neg_selected  = 0;
    cs->neg_result    = 0;
    neg_set_demand(cs, enemy_idx);
}

static void render_negotiation(SDL_Renderer* renderer, CombatSystem* cs,
                                TTF_Font* font) {
    if (cs->state != COMBAT_NEGOTIATE) return;

    int enemy_idx = cs->neg_enemy_idx;
    if (enemy_idx >= cs->enemy_count) return;

    CombatEnemy*     ce = &cs->enemies[enemy_idx];
    StatBlock*       sb = &ce->stats;
    EnemyPersonality p  = get_personality(sb);

    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color yellow = {255, 220, 100, 255};
    SDL_Color gray   = {150, 150, 180, 255};
    SDL_Color green  = {100, 220, 100, 255};
    SDL_Color red    = {220,  60,  60, 255};
    SDL_Color cyan   = { 80, 200, 220, 255};

    // Dark overlay
    draw_rect(renderer, 0, 0, 1280, 720, 0, 0, 0, 160);

    // Main negotiation panel
    draw_rect(renderer,   240, 100, 800, 520, 8, 8, 28, 255);
    draw_border(renderer, 240, 100, 800, 520, 100, 80, 200, 255);

    // Header
    char header[80];
    snprintf(header, 80, "Negotiation — %s [%s]",
             sb->name, personality_name(p));
    draw_text(renderer, font, header, yellow, 260, 112);

    // HP bar of enemy
    draw_text(renderer, font, "HP:", gray, 260, 140);
    draw_rect(renderer,  300, 142, 200, 10, 40, 10, 10, 255);
    int hpfill = sb->max_hp > 0 ? (sb->hp * 200) / sb->max_hp : 0;
    SDL_Color hpcol = sb->hp * 100 / (sb->max_hp > 0 ? sb->max_hp : 1) < 30 ?
        (SDL_Color){255, 60, 60, 255} : (SDL_Color){60, 200, 60, 255};
    draw_rect(renderer, 300, 142, hpfill, 10, hpcol.r, hpcol.g, hpcol.b, 255);

    // Success chance display
    int chance = neg_success_chance(cs, enemy_idx);
    char chance_str[32];
    snprintf(chance_str, 32, "Success: ~%d%%", chance);
    SDL_Color cc = chance > 60 ? green : chance > 30 ? yellow : red;
    draw_text(renderer, font, chance_str, cc, 720, 140);

    // Enemy dialogue box
    draw_rect(renderer,   260, 170, 760, 80, 15, 15, 50, 255);
    draw_border(renderer, 260, 170, 760, 80, 80, 80, 160, 255);

    const char* dialogue = "";
    if (cs->neg_phase == 0) {
        dialogue = neg_greeting(sb, p);
    } else if (cs->neg_phase == 1) {
        dialogue = neg_demand_text(cs, cs->itemdb);
    } else if (cs->neg_phase == 2) {
        if (cs->neg_result == 1)
            dialogue = "...Fine. I'll leave. Take care of yourself.";
        else
            dialogue = "You think I'd fall for that?! Now you'll pay!";
    }

    // Word-wrap the dialogue
    draw_text(renderer, font, dialogue, white, 276, 182);
    draw_text(renderer, font, sb->name, yellow, 276, 208);

    if (cs->neg_phase == 0 || cs->neg_phase == 1) {
        // Phase 0: show greeting, press E to continue
        // Phase 1: show demand, offer choices

        if (cs->neg_phase == 0) {
            draw_text(renderer, font, "E: Continue", gray, 260, 270);
        } else {
            // Choice panel
            draw_text(renderer, font, "How do you respond?", gray, 260, 270);

            const char* choices[3];
            int num_choices = 3;

            switch (cs->neg_demand_type) {
                case 0: // money
                    choices[0] = "1. Pay up.";
                    choices[1] = "2. Negotiate down.";
                    choices[2] = "3. Refuse.";
                    break;
                case 1: // item
                    choices[0] = "1. Hand it over.";
                    choices[1] = "2. Offer something else.";
                    choices[2] = "3. Refuse.";
                    break;
                case 2: // flattery
                    choices[0] = "1. You're incredible!";
                    choices[1] = "2. You're... okay, I guess.";
                    choices[2] = "3. I don't think so.";
                    break;
                default:
                    choices[0] = "1. Agree.";
                    choices[1] = "2. Hesitate.";
                    choices[2] = "3. Refuse.";
                    break;
            }

            for (int i = 0; i < num_choices; i++) {
                SDL_Color col = (i == cs->neg_selected) ? yellow : white;
                if (i == cs->neg_selected) {
                    draw_rect(renderer, 258, 298 + i*46, 764, 40,
                              40, 40, 80, 255);
                }
                draw_text(renderer, font, choices[i], col, 276, 306 + i*46);
            }

            // Resource display
            if (cs->neg_demand_type == 0) {
                char yen_str[64];
                snprintf(yen_str, 64, "Your yen: %d", cs->inventory->yen);
                draw_text(renderer, font, yen_str, cyan, 800, 310);
            } else if (cs->neg_demand_type == 1) {
                int qty = inventory_count(cs->inventory, cs->neg_demand_item);
                Item* item = itemdb_get(cs->itemdb, cs->neg_demand_item);
                char item_str[64];
                snprintf(item_str, 64, "You have: %s x%d",
                         item ? item->name : "?", qty);
                draw_text(renderer, font, item_str, cyan, 800, 310);
            }

            draw_text(renderer, font, "Up/Down: select  E: confirm",
                      gray, 260, 450);
        }
    } else {
        // Phase 2 — result
        if (cs->neg_result == 1) {
            draw_text(renderer, font, "Negotiation successful!", green, 260, 270);
            draw_text(renderer, font, "The enemy withdraws.", white, 260, 300);
        } else {
            draw_text(renderer, font, "Negotiation failed!", red, 260, 270);
            draw_text(renderer, font, "The enemy attacks!", white, 260, 300);
        }
        draw_text(renderer, font, "E: Continue", gray, 260, 340);
    }
}

static void handle_negotiation_input(CombatSystem* cs, SDL_Keycode key,
                                      GameState* gs) {
    if (cs->state != COMBAT_NEGOTIATE) return;

    int enemy_idx = cs->neg_enemy_idx;
    if (enemy_idx >= cs->enemy_count) return;

    CombatEnemy* ce = &cs->enemies[enemy_idx];
    StatBlock*   sb = &ce->stats;

    if (cs->neg_phase == 0) {
        // Greeting — press E to advance to demand
        if (key == SDLK_e || key == SDLK_RETURN) {
            cs->neg_phase = 1;
            cs->neg_selected = 0;
        } else if (key == SDLK_ESCAPE) {
            // Back out of negotiation
            cs->state = COMBAT_PLAYER_TURN;
        }
        return;
    }

    if (cs->neg_phase == 1) {
        // Demand phase — choose response
        switch (key) {
            case SDLK_UP:
                cs->neg_selected = (cs->neg_selected - 1 + 3) % 3; break;
            case SDLK_DOWN:
                cs->neg_selected = (cs->neg_selected + 1) % 3; break;
            case SDLK_1: cs->neg_selected = 0; break;
            case SDLK_2: cs->neg_selected = 1; break;
            case SDLK_3: cs->neg_selected = 2; break;
            case SDLK_ESCAPE:
                cs->state = COMBAT_PLAYER_TURN; return;
            case SDLK_e:
            case SDLK_RETURN: {
                int base_chance = neg_success_chance(cs, enemy_idx);
                int success     = 0;

                if (cs->neg_selected == 0) {
                    // Best response — pay/give/flatter properly
                    if (cs->neg_demand_type == 0) {
                        // Pay money
                        if (cs->inventory->yen >= cs->neg_demand_amount) {
                            cs->inventory->yen -= cs->neg_demand_amount;
                            success = (rand() % 100) < (base_chance + 20);
                        } else {
                            combat_log(cs, "Not enough yen!");
                            return;
                        }
                    } else if (cs->neg_demand_type == 1) {
                        // Give item
                        if (inventory_count(cs->inventory, cs->neg_demand_item)
                            >= cs->neg_demand_amount) {
                            inventory_remove(cs->inventory,
                                             cs->neg_demand_item,
                                             cs->neg_demand_amount);
                            success = (rand() % 100) < (base_chance + 20);
                        } else {
                            combat_log(cs, "You don't have that item!");
                            return;
                        }
                    } else {
                        // Flattery — sincere
                        success = (rand() % 100) < (base_chance + 25);
                    }
                } else if (cs->neg_selected == 1) {
                    // Middle response — half-hearted
                    success = (rand() % 100) < (base_chance - 10);
                } else {
                    // Refuse — very low chance, enemy might attack
                    success = (rand() % 100) < (base_chance - 40);
                }

                cs->neg_phase  = 2;
                cs->neg_result = success ? 1 : 2;
                break;
            }
            default: break;
        }
        return;
    }

    if (cs->neg_phase == 2) {
        // Result — press E to apply
        if (key == SDLK_e || key == SDLK_RETURN) {
            if (cs->neg_result == 1) {
                // Success — negotiated enemy gives reward, ALL enemies flee
                // The negotiated enemy gives half EXP; others give none
                sb->alive     = 0;
                sb->hp        = 0;
                ce->exp_reward /= 2;

                char msg[LOG_LINE_LEN];
                snprintf(msg, LOG_LINE_LEN, "%s withdrew!", sb->name);
                combat_log(cs, msg);

                // Chance to receive a Summon (30% base chance)
                extern SummonStock g_summon_stock;
                extern SummonDB    g_summon_db;
                extern SummonCompendium g_summon_comp;
                int yen_reward = (ce->exp_reward * 80) / 100;  // yen on no-summon
                if ((rand() % 100) < 80 && g_summon_stock.count < MAX_PROTA_SUMMONS) {
                    // Derive enemy level from EXP reward, find matching Summon
                    int enemy_level = ce->exp_reward / 15;
                    if (enemy_level < 1) enemy_level = 1;
                    int best = -1, best_diff = 999;
                    for (int pi = 0; pi < g_summon_db.count; pi++) {
                        int diff = abs(g_summon_db.entries[pi].base_level - enemy_level);
                        if (diff < best_diff) { best_diff = diff; best = pi; }
                    }
                    // Find best match not already in stock
                    int found = -1;
                    for (int pass = 0; pass < g_summon_db.count && found < 0; pass++) {
                        // Each pass: find next closest not in stock
                        int next_best = -1, next_diff = 999;
                        for (int pi = 0; pi < g_summon_db.count; pi++) {
                            int diff = abs(g_summon_db.entries[pi].base_level - enemy_level);
                            if (diff >= next_diff) continue;
                            // Check not already in stock
                            int has = 0;
                            for (int si = 0; si < g_summon_stock.count; si++)
                                if (strcmp(g_summon_stock.stock[si].name,
                                           g_summon_db.entries[pi].name) == 0)
                                    { has = 1; break; }
                            if (!has) { next_best = pi; next_diff = diff; }
                        }
                        found = next_best;
                    }
                    if (found >= 0) {
                        summon_add(&g_summon_stock, &g_summon_db.entries[found]);
                        summon_register(&g_summon_comp, &g_summon_db.entries[found]);
                        char pmsg[LOG_LINE_LEN];
                        snprintf(pmsg, LOG_LINE_LEN, "Obtained Summon: %s!",
                                 g_summon_db.entries[found].name);
                        combat_log(cs, pmsg);
                        if (cs->notify) notify_success(cs->notify, pmsg);
                        yen_reward = 0;  // got summon, no yen
                    }
                }
                // 20% chance or stock full — give yen instead
                if (yen_reward > 0 && cs->inventory) {
                    cs->inventory->yen += yen_reward;
                    char ymsg[LOG_LINE_LEN];
                    snprintf(ymsg, LOG_LINE_LEN, "Got %d yen!", yen_reward);
                    combat_log(cs, ymsg);
                }

                // All remaining enemies flee after successful negotiation
                for (int _fi = 0; _fi < cs->enemy_count; _fi++) {
                    if (cs->enemies[_fi].stats.alive) {
                        cs->enemies[_fi].stats.alive = 0;
                        cs->enemies[_fi].stats.hp    = 0;
                        cs->enemies[_fi].exp_reward  = 0;  // no EXP for fleeing enemies
                    }
                }
                combat_log(cs, "The shadows flee!");
                cs->state = COMBAT_WIN;
                gamestate_set_flag(gs, cs->win_flag, 1);
                apply_exp_rewards(cs, cs->notify);
            } else {
                // Failure — enemy attacks with boosted anger
                combat_log(cs, "The enemy is furious!");
                // Give enemy a free attack
                StatBlock* actor =
                    &cs->party->members[cs->turn_order[cs->turn_index] < PARTY_MAX ?
                    cs->turn_order[cs->turn_index] : 0];
                int dmg = statblock_phys_damage(sb, actor, 12);
                actor->hp -= dmg;
                if (actor->hp < 0) actor->hp = 0;
                char msg[LOG_LINE_LEN];
                snprintf(msg, LOG_LINE_LEN, "%s attacks %s for %d in anger!",
                         sb->name, actor->name, dmg);
                combat_log(cs, msg);
                if (actor->hp == 0) {
                    actor->alive = 0;
                    if (strcmp(actor->name, "Prota") == 0)
                        cs->state = COMBAT_LOSE;
                } else {
                    advance_turn(cs);
                    cs->state = is_party_turn(cs) ?
                                COMBAT_PLAYER_TURN : COMBAT_ENEMY_TURN;
                }
            }
        }
        return;
    }
}

// ─── Input ───────────────────────────────────────────────────────────────────

void combat_handle_input(CombatSystem* cs, SDL_Event* event,
                         GameState* gs) {
    if (!cs->active) return;
    if (event->type != SDL_KEYDOWN) return;

    SDL_Keycode key = event->key.keysym.sym;

    // Auto retarget dead enemies
    if (cs->target_selected < cs->enemy_count &&
        !cs->enemies[cs->target_selected].stats.alive) {
        for (int i = 0; i < cs->enemy_count; i++)
            if (cs->enemies[i].stats.alive) { cs->target_selected = i; break; }
    }

    // ── Negotiation screen ───────────────────────────────────────────────────
    if (cs->state == COMBAT_NEGOTIATE) {
        handle_negotiation_input(cs, key, gs);
        return;
    }

    // ── Skill learn screen ───────────────────────────────────────────────────
    if (cs->state == COMBAT_SKILL_LEARN) {
        if (cs->learn_index >= cs->learn_count) {
            // All learns processed — end combat properly
            cs->won_last_battle = 1;
            cs->active          = 0;
            cs->just_ended      = 1;
            return;
        }

        PendingLearn* pl = &cs->learn_queue[cs->learn_index];
        StatBlock* m     = &cs->party->members[pl->party_member_idx];

        if (!pl->choosing_replace) {
            // ── Root choice: Learn / Skip ────────────────────────────────
            switch (key) {
                case SDLK_e:
                case SDLK_RETURN:
                    if (m->skill_count < MAX_SKILLS_PER_COMBATANT) {
                        // Slot available — learn immediately
                        m->skill_ids[m->skill_count++] = pl->new_skill_id;
                        char msg[LOG_LINE_LEN];
                        Skill* sk = skilldb_get(cs->skilldb, pl->new_skill_id);
                        snprintf(msg, LOG_LINE_LEN, "%s learned %s!",
                                 m->name, sk ? sk->name : "a new skill");
                        combat_log(cs, msg);
                        if (cs->notify) notify_success(cs->notify, msg);
                        cs->learn_index++;
                    } else {
                        // Full loadout — enter replace mode
                        pl->choosing_replace = 1;
                        pl->replace_selected = 0;
                    }
                    break;
                case SDLK_ESCAPE:
                case SDLK_x:
                    // Skip this skill
                    cs->learn_index++;
                    break;
            }
        } else {
            // ── Replace mode: pick a slot to overwrite ───────────────────
            switch (key) {
                case SDLK_UP:
                    pl->replace_selected =
                        (pl->replace_selected - 1 + m->skill_count)
                        % m->skill_count;
                    break;
                case SDLK_DOWN:
                    pl->replace_selected =
                        (pl->replace_selected + 1) % m->skill_count;
                    break;
                case SDLK_e:
                case SDLK_RETURN: {
                    // Replace the chosen slot
                    Skill* old_sk = skilldb_get(cs->skilldb,
                                    m->skill_ids[pl->replace_selected]);
                    Skill* new_sk = skilldb_get(cs->skilldb, pl->new_skill_id);
                    m->skill_ids[pl->replace_selected] = pl->new_skill_id;

                    char msg[LOG_LINE_LEN];
                    snprintf(msg, LOG_LINE_LEN, "%s forgot %s, learned %s!",
                             m->name,
                             old_sk ? old_sk->name : "?",
                             new_sk ? new_sk->name : "?");
                    combat_log(cs, msg);
                    if (cs->notify) notify_success(cs->notify, msg);

                    pl->choosing_replace = 0;
                    cs->learn_index++;
                    break;
                }
                case SDLK_ESCAPE:
                case SDLK_x:
                    // Back to root choice (keep choosing_replace=0, skip)
                    pl->choosing_replace = 0;
                    cs->learn_index++;
                    break;
            }
        }

        // Advance past any remaining learns if all done
        if (cs->learn_index >= cs->learn_count) {
            cs->won_last_battle = 1;
            cs->active          = 0;
            cs->just_ended      = 1;
        }
        return;
    }

    // ── Dismiss victory / loss / flee screens ────────────────────────────────
    if (cs->state == COMBAT_WIN  ||
        cs->state == COMBAT_LOSE ||
        cs->state == COMBAT_FLEE) {
        if (key == SDLK_e || key == SDLK_RETURN) {
            if (cs->state == COMBAT_WIN && cs->learn_count > 0) {
                // Transition to skill learn screen — keep active=1
                cs->state       = COMBAT_SKILL_LEARN;
                cs->learn_index = 0;
                // active stays 1 so game loop keeps routing to combat
            } else {
                cs->won_last_battle = (cs->state == COMBAT_WIN ||
                                       cs->state == COMBAT_FLEE);
                cs->active     = 0;
                cs->just_ended = 1;
            }
        }
        return;
    }

    // ── Team attack ───────────────────────────────────────────────────────
    // ── Standoff ──────────────────────────────────────────────────────────────
    if (cs->state == COMBAT_TEAM_ATTACK) {
        int is_boss_ao = cs->gs && gamestate_get_flag(cs->gs, "in_boss_combat");
        int ao_count = is_boss_ao ? 2 : 3;
        if (key == SDLK_UP)
            cs->menu_selected = (cs->menu_selected - 1 + ao_count) % ao_count;
        else if (key == SDLK_DOWN)
            cs->menu_selected = (cs->menu_selected + 1) % ao_count;
        else if (key == SDLK_e || key == SDLK_RETURN) {
            int sel = cs->menu_selected;
            if (is_boss_ao && sel == 1) sel = 2;
            if (sel == 0) {
                // ── Team Attack ────────────────────────────────────────
                char msg[LOG_LINE_LEN];
                int total_dmg = 0;
                for (int i = 0; i < cs->party->count; i++) {
                    if (!cs->party->members[i].alive) continue;
                    StatBlock* m = &cs->party->members[i];
                    for (int j = 0; j < cs->enemy_count; j++) {
                        if (!cs->enemies[j].stats.alive) continue;
                        int dmg = statblock_phys_damage(m, &cs->enemies[j].stats, 8);
                        cs->enemies[j].stats.hp -= dmg;
                        if (cs->enemies[j].stats.hp <= 0) {
                            cs->enemies[j].stats.hp    = 0;
                            cs->enemies[j].stats.alive = 0;
                            roll_drops(cs, &cs->enemies[j]);
                        }
                        total_dmg += dmg;
                    }
                }
                snprintf(msg, LOG_LINE_LEN, "Team Attack! %d total damage!", total_dmg);
                combat_log(cs, msg);
                audio_play_sfx(SFX_TEAM_ATTACK);
                for (int i = 0; i < cs->enemy_count; i++)
                    cs->enemies[i].stats.knocked_down = 0;
                cs->team_attack_available = 0;
                cs->menu_selected = 0;
                if (all_enemies_dead(cs)) {
                    cs->state = COMBAT_WIN;
                    combat_log(cs, "Victory! Press E.");
                    gamestate_set_flag(gs, cs->win_flag, 1);
                    apply_exp_rewards(cs, cs->notify);
                } else {
                    advance_turn(cs);
                    cs->state = is_party_turn(cs) ?
                                COMBAT_PLAYER_TURN : COMBAT_ENEMY_TURN;
                }
            } else if (sel == 1) {
                // ── Talk — negotiate with first alive enemy ────────────────
                // Clear knocked_down on all enemies first so they stand up
                for (int i = 0; i < cs->enemy_count; i++)
                    cs->enemies[i].stats.knocked_down = 0;
                int target = -1;
                for (int i = 0; i < cs->enemy_count; i++)
                    if (cs->enemies[i].stats.alive) { target = i; break; }
                if (target >= 0) {
                    start_negotiation(cs, target);
                    cs->team_attack_available = 0;
                    cs->menu_selected = 0;
                }
            } else {
                // ── Skip — enemies stand back up ──────────────────────────
                for (int i = 0; i < cs->enemy_count; i++)
                    cs->enemies[i].stats.knocked_down = 0;
                cs->team_attack_available = 0;
                cs->menu_selected = 0;
                combat_log(cs, "The party steps back.");
                advance_turn(cs);
                cs->state = is_party_turn(cs) ?
                            COMBAT_PLAYER_TURN : COMBAT_ENEMY_TURN;
            }
        }
        return;
    }

    // ── Player turn root menu ────────────────────────────────────────────────
    if (cs->state == COMBAT_PLAYER_TURN) {
        StatBlock* actor = &cs->party->members[current_turn_idx(cs)];

        // Recalculate num_opts here for input handling
        int alive_count_i = 0;
        for (int ei = 0; ei < cs->enemy_count; ei++)
            if (cs->enemies[ei].stats.alive) alive_count_i++;
        // Talk available when 1 enemy alive and NOT knocked down
        // (Standoff Talk handles the all-downed case separately)
        int standing_i = 0;
        for (int ei = 0; ei < cs->enemy_count; ei++)
            if (cs->enemies[ei].stats.alive && !cs->enemies[ei].stats.knocked_down)
                standing_i++;
        int is_boss = cs->gs && gamestate_get_flag(cs->gs, "in_boss_combat");
        int can_negotiate_i = (alive_count_i == 1 && standing_i == 1 && !is_boss);
        int is_prota_i = (strcmp(actor->name, "Prota") == 0);
        // Prota gets a Summon option; Talk adds one more
        int num_opts = 4 + (is_prota_i ? 1 : 0) + (can_negotiate_i ? 1 : 0);
        // Option indices:
        // 0=Attack 1=Skills 2=Items 3=Flee
        // 4=Summon (Prota only)
        // 4 or 5=Talk (if can_negotiate)
        int summon_opt_i = is_prota_i ? 4 : -1;
        int talk_opt_i    = can_negotiate_i ? (is_prota_i ? 5 : 4) : -1;

        switch (key) {
            case SDLK_UP:
                cs->menu_selected = (cs->menu_selected - 1 + num_opts) % num_opts;
                audio_play_sfx(SFX_MENU_MOVE); break;
            case SDLK_DOWN:
                cs->menu_selected = (cs->menu_selected + 1) % num_opts;
                audio_play_sfx(SFX_MENU_MOVE); break;
            case SDLK_LEFT:
                cs->target_selected =
                    (cs->target_selected - 1 + cs->enemy_count)
                    % cs->enemy_count; break;
            case SDLK_RIGHT:
                cs->target_selected =
                    (cs->target_selected + 1) % cs->enemy_count; break;
            case SDLK_e:
            case SDLK_RETURN:
                if (cs->menu_selected == 0) {
                    Skill* sk = skilldb_get(cs->skilldb, actor->skill_ids[0]);
                    if (sk) {
                        execute_skill(cs, actor, sk,
                            &cs->enemies[cs->target_selected].stats,
                            1, cs->target_selected);
                        if (all_enemies_dead(cs)) {
                            cs->state = COMBAT_WIN;
                            combat_log(cs, "Victory! Press E.");
                            gamestate_set_flag(gs, cs->win_flag, 1);
                            apply_exp_rewards(cs, cs->notify);
                        } else if (all_enemies_knocked_down(cs)) {
                            cs->state = COMBAT_TEAM_ATTACK;
                            cs->team_attack_available = 1;
                            combat_log(cs, "Team Attack? (E)");
                        } else if (actor->extra_turn) {
                            actor->extra_turn = 0;
                        } else {
                            if (roll_extra_turn(cs)) {
                                cs->menu_selected = 0;
                                cs->state = COMBAT_PLAYER_TURN;
                                combat_log(cs, "Wild Card! Act again.");
                            } else {
                                advance_turn(cs);
                                cs->state = is_party_turn(cs) ?
                                    COMBAT_PLAYER_TURN : COMBAT_ENEMY_TURN;
                            }
                        }
                    }
                } else if (cs->menu_selected == 1) {
                    // Despair and Forget prevent skill use
                    if (actor->status == STATUS_DESPAIR ||
                        actor->status == STATUS_FORGET) {
                        combat_log(cs, "Can't use skills in this state!");
                    } else {
                        cs->state = COMBAT_SKILL_MENU;
                        cs->skill_selected = 0;
                    }
                } else if (cs->menu_selected == 2) {
                    cs->state = COMBAT_ITEM_MENU;
                    cs->item_selected = 0;
                } else if (talk_opt_i >= 0 && cs->menu_selected == talk_opt_i) {
                    // Talk — find the single living enemy
                    for (int ei = 0; ei < cs->enemy_count; ei++) {
                        if (cs->enemies[ei].stats.alive) {
                            start_negotiation(cs, ei);
                            break;
                        }
                    }
                } else if (summon_opt_i >= 0 && cs->menu_selected == summon_opt_i) {
                    // Open summon selection submenu
                    extern SummonStock g_summon_stock;
                    if (g_summon_stock.count <= 1) {
                        combat_log(cs, "No other Summons to switch to.");
                    } else {
                        cs->state = COMBAT_SUMMON_MENU;
                        cs->summon_selected = g_summon_stock.active_idx;
                    }
                    return;
                } else if (cs->menu_selected == 3) {
                    // Flee (always index 3)
                    int flee_chance = 50;
                    if (cs->bonds)
                        flee_chance += bond_get_perk_value(
                            cs->bonds, PERK_FLEE_BONUS);
                    if ((rand() % 100) < flee_chance) {
                        cs->state = COMBAT_FLEE;
                        combat_log(cs, "Escaped! Press E.");
                    } else {
                        combat_log(cs, "Couldn't flee!");
                        advance_turn(cs);
                        cs->state = is_party_turn(cs) ?
                                    COMBAT_PLAYER_TURN : COMBAT_ENEMY_TURN;
                    }
                }
                break;
        }

    // ── Summon menu ──────────────────────────────────────────────────────────
    } else if (cs->state == COMBAT_SUMMON_MENU) {
        extern SummonStock g_summon_stock;
        switch (key) {
            case SDLK_UP:
                cs->summon_selected =
                    (cs->summon_selected - 1 + g_summon_stock.count)
                    % g_summon_stock.count;
                audio_play_sfx(SFX_MENU_MOVE); break;
            case SDLK_DOWN:
                cs->summon_selected =
                    (cs->summon_selected + 1) % g_summon_stock.count;
                audio_play_sfx(SFX_MENU_MOVE); break;
            case SDLK_e: case SDLK_RETURN:
                if (cs->summon_selected != g_summon_stock.active_idx) {
                    summon_switch(&g_summon_stock, cs->summon_selected);
                    StatBlock* prota = &cs->party->members[0];
                    summon_apply_to_statblock(&g_summon_stock, prota);
                    char pmsg[LOG_LINE_LEN];
                    snprintf(pmsg, LOG_LINE_LEN, "Prota switches to %s!",
                             g_summon_stock.stock[cs->summon_selected].name);
                    combat_log(cs, pmsg);
                    audio_play_sfx(SFX_MENU_CONFIRM);
                }
                // Free action — return to player turn without advancing
                cs->state = COMBAT_PLAYER_TURN;
                cs->menu_selected = 0;
                break;
            case SDLK_ESCAPE:
                cs->state = COMBAT_PLAYER_TURN;
                cs->menu_selected = 0;
                break;
        }
        return;

    // ── Skill menu ───────────────────────────────────────────────────────────
    // ── Skill menu ───────────────────────────────────────────────────────────
    } else if (cs->state == COMBAT_SKILL_MENU) {
        StatBlock* actor = &cs->party->members[current_turn_idx(cs)];
        extern SummonStock g_summon_stock;
        int is_prota_sm = (strcmp(actor->name, "Prota") == 0);

        // DEBUG — remove once confirmed working
        fprintf(stderr, "SKILLMENU key=%d (q=%d w=%d) actor='%s' prota=%d pcount=%d active=%d\n",
                (int)key, (int)SDLK_q, (int)SDLK_w, actor->name,
                is_prota_sm, g_summon_stock.count, g_summon_stock.active_idx);

        switch (key) {
            case SDLK_UP:
                if (actor->skill_count > 0)
                    cs->skill_selected =
                        (cs->skill_selected - 1 + actor->skill_count)
                        % actor->skill_count;
                break;
            case SDLK_DOWN:
                if (actor->skill_count > 0)
                    cs->skill_selected =
                        (cs->skill_selected + 1) % actor->skill_count;
                break;

            // ── Summon cycling within the skill menu (Prota only) ──────────
            case SDLK_q:
            case SDLK_w: {
                if (!is_prota_sm) {
                    combat_log(cs, "Only Prota can switch Summons.");
                    break;
                }
                if (g_summon_stock.count <= 1) {
                    combat_log(cs, "No other Summons to switch to.");
                    break;
                }
                int dir = (key == SDLK_q) ? -1 : 1;
                int ni = (g_summon_stock.active_idx + dir + g_summon_stock.count)
                         % g_summon_stock.count;
                summon_switch(&g_summon_stock, ni);
                summon_apply_to_statblock(&g_summon_stock, actor);
                if (cs->skill_selected >= actor->skill_count)
                    cs->skill_selected = 0;
                audio_play_sfx(SFX_MENU_MOVE);
                char pmsg[LOG_LINE_LEN];
                snprintf(pmsg, LOG_LINE_LEN, "Summon: %s",
                         g_summon_stock.stock[ni].name);
                combat_log(cs, pmsg);
                break;
            }

            case SDLK_LEFT:
                if (cs->enemy_count > 0)
                    cs->target_selected =
                        (cs->target_selected - 1 + cs->enemy_count)
                        % cs->enemy_count;
                break;
            case SDLK_RIGHT:
                if (cs->enemy_count > 0)
                    cs->target_selected =
                        (cs->target_selected + 1) % cs->enemy_count;
                break;
            case SDLK_ESCAPE:
                cs->state = COMBAT_PLAYER_TURN; break;
            case SDLK_e:
            case SDLK_RETURN: {
                Skill* sk = skilldb_get(cs->skilldb,
                                        actor->skill_ids[cs->skill_selected]);
                if (!sk) break;

                int sp_cost = sk->sp_cost;
                if (sk->inflicts != STATUS_NONE && cs->bonds &&
                    bond_has_perk(cs->bonds, PERK_STATUS_NO_SP))
                    sp_cost = 0;
                if (actor->handoff_sp_discount && sp_cost > 0)
                    sp_cost = sp_cost / 2;

                if (actor->sp < sp_cost) {
                    combat_log(cs, "Not enough SP!"); break;
                }
                actor->sp -= sp_cost;

                if (sk->target == TARGET_ALL_ENEMIES) {
                    for (int i = 0; i < cs->enemy_count; i++) {
                        if (!cs->enemies[i].stats.alive) continue;
                        execute_skill(cs, actor, sk,
                                     &cs->enemies[i].stats, 1, i);
                    }
                } else if (sk->target == TARGET_ONE_ALLY) {
                    execute_skill(cs, actor, sk,
                        &cs->party->members[
                            cs->ally_target_selected % cs->party->count],
                        1, -1);
                } else if (sk->target == TARGET_ALL_ALLIES) {
                    for (int i = 0; i < cs->party->count; i++) {
                        if (!cs->party->members[i].alive) continue;
                        execute_skill(cs, actor, sk,
                                     &cs->party->members[i], 1, -1);
                    }
                } else {
                    execute_skill(cs, actor, sk,
                        &cs->enemies[cs->target_selected].stats,
                        1, cs->target_selected);
                }

                if (all_enemies_dead(cs)) {
                    cs->state = COMBAT_WIN;
                    combat_log(cs, "Victory! Press E.");
                    gamestate_set_flag(gs, cs->win_flag, 1);
                    apply_exp_rewards(cs, cs->notify);
                } else if (all_enemies_knocked_down(cs)) {
                    cs->state = COMBAT_TEAM_ATTACK;
                    cs->team_attack_available = 1;
                    combat_log(cs, "Team Attack? (E)");
                } else if (actor->extra_turn) {
                    cs->state = COMBAT_PLAYER_TURN;
                    combat_log(cs, "Extra Turn! Take another action.");
                } else {
                    if (roll_extra_turn(cs)) {
                        cs->menu_selected = 0;
                        cs->state = COMBAT_PLAYER_TURN;
                        combat_log(cs, "Wild Card! Act again.");
                    } else {
                        advance_turn(cs);
                        cs->menu_selected = 0;
                        cs->state = is_party_turn(cs) ?
                                    COMBAT_PLAYER_TURN : COMBAT_ENEMY_TURN;
                    }
                }
                break;
            }
        }

    // ── Item menu ─────────────────────────────────────────────────────────────
    } else if (cs->state == COMBAT_ITEM_MENU) {
        if (!cs->inventory || cs->inventory->slot_count == 0) {
            combat_log(cs, "No items!");
            cs->state = COMBAT_PLAYER_TURN;
            return;
        }

        switch (key) {
            case SDLK_UP:
                cs->item_selected =
                    (cs->item_selected - 1 + cs->inventory->slot_count)
                    % cs->inventory->slot_count; break;
            case SDLK_DOWN:
                cs->item_selected =
                    (cs->item_selected + 1) % cs->inventory->slot_count; break;
            case SDLK_LEFT:
                cs->ally_target_selected =
                    (cs->ally_target_selected - 1 + cs->party->count)
                    % cs->party->count; break;
            case SDLK_RIGHT:
                cs->ally_target_selected =
                    (cs->ally_target_selected + 1) % cs->party->count; break;
            case SDLK_ESCAPE:
                cs->state = COMBAT_PLAYER_TURN; break;
            case SDLK_e:
            case SDLK_RETURN: {
                InventorySlot* slot = &cs->inventory->slots[cs->item_selected];
                Item* item = itemdb_get(cs->itemdb, slot->item_id);
                if (!item) break;

                char msg[LOG_LINE_LEN];

                if (item->type == ITEM_BATTLE) {
                    if (item->power > 0) {
                        for (int i = 0; i < cs->enemy_count; i++) {
                            if (!cs->enemies[i].stats.alive) continue;
                            int dmg = item->power;
                            Affinity aff = statblock_get_affinity(
                                &cs->enemies[i].stats, item->element);
                            int hit_weak = (aff == AFF_WEAK);
                            if (hit_weak)          dmg = dmg * 3 / 2;
                            if (aff == AFF_RESIST)  dmg = dmg / 2;
                            if (aff == AFF_NULL || aff == AFF_ABSORB) dmg = 0;
                            cs->enemies[i].stats.hp -= dmg;
                            if (cs->enemies[i].stats.hp <= 0) {
                                cs->enemies[i].stats.hp    = 0;
                                cs->enemies[i].stats.alive = 0;
                                roll_drops(cs, &cs->enemies[i]);
                            } else if (hit_weak && dmg > 0 &&
                                       !cs->enemies[i].stats.knocked_down) {
                                cs->enemies[i].stats.knocked_down = 1;
                                audio_play_sfx(SFX_WEAKNESS_HIT);
                            }
                            if (cs->gs)
                                gamestate_discover_affinity(cs->gs,
                                    cs->enemies[i].stats.name, item->element);
                        }
                        snprintf(msg, LOG_LINE_LEN, "Used %s!", item->name);
                        combat_log(cs, msg);
                        inventory_remove(cs->inventory, item->id, 1);
                    } else if (item->id == 32) {
                        cs->state = COMBAT_FLEE;
                        combat_log(cs, "Smoke Bomb! Escaped!");
                        inventory_remove(cs->inventory, item->id, 1);
                        return;
                    }
                } else {
                    StatBlock* target =
                        &cs->party->members[
                            cs->ally_target_selected % cs->party->count];
                    inventory_use_item(cs->inventory, cs->itemdb,
                                       item->id, target, cs->party,
                                       cs->bonds);
                    inventory_remove(cs->inventory, item->id, 1);
                    snprintf(msg, LOG_LINE_LEN,
                             "Used %s on %s!", item->name, target->name);
                    combat_log(cs, msg);
                }

                if (all_enemies_dead(cs)) {
                    cs->state = COMBAT_WIN;
                    combat_log(cs, "Victory! Press E.");
                    gamestate_set_flag(gs, cs->win_flag, 1);
                    apply_exp_rewards(cs, cs->notify);
                } else if (all_enemies_knocked_down(cs)) {
                    cs->state = COMBAT_TEAM_ATTACK;
                    cs->team_attack_available = 1;
                    cs->menu_selected = 0;
                    combat_log(cs, "Standoff!");
                } else {
                    if (roll_extra_turn(cs)) {
                        cs->menu_selected = 0;
                        cs->state = COMBAT_PLAYER_TURN;
                        combat_log(cs, "Wild Card! Act again.");
                    } else {
                        advance_turn(cs);
                        cs->menu_selected = 0;
                        cs->state = is_party_turn(cs) ?
                                    COMBAT_PLAYER_TURN : COMBAT_ENEMY_TURN;
                    }
                }
                break;
            }
        }
    }
}

// ─── Update ──────────────────────────────────────────────────────────────────

void combat_update(CombatSystem* cs, GameState* gs) {
    if (!cs->active) return;
    if (cs->state == COMBAT_SKILL_LEARN) return;

    // Process player turn start — handle status effects before showing menu
    if (cs->state == COMBAT_PLAYER_TURN) {
        int idx = current_turn_idx(cs);
        if (idx < PARTY_MAX) {
            // Skip dead party members — should not happen but guard anyway
            if (!cs->party->members[idx].alive) {
                advance_turn(cs);
                cs->state = is_party_turn(cs) ?
                            COMBAT_PLAYER_TURN : COMBAT_ENEMY_TURN;
                return;
            }
            StatBlock* actor = &cs->party->members[idx];
            if (actor->status != STATUS_NONE) {
                TurnStartResult tsr = process_turn_start(cs, actor);

                // Check if burn killed the actor
                if (actor->hp == 0) {
                    actor->alive = 0;
                    if (strcmp(actor->name, "Prota") == 0) {
                        cs->state = COMBAT_LOSE;
                        combat_log(cs, "Prota has fallen. Game Over.");
                        return;
                    }
                    if (party_all_dead(cs->party)) {
                        cs->state = COMBAT_LOSE;
                        combat_log(cs, "The party has fallen...");
                        return;
                    }
                    advance_turn(cs);
                    cs->state = is_party_turn(cs) ?
                                COMBAT_PLAYER_TURN : COMBAT_ENEMY_TURN;
                    return;
                }

                if (tsr == TURN_SKIP) {
                    advance_turn(cs);
                    cs->state = is_party_turn(cs) ?
                                COMBAT_PLAYER_TURN : COMBAT_ENEMY_TURN;
                    return;
                }

                if (tsr == TURN_CONFUSE) {
                    // Confused party member attacks a random ally
                    int target_idx = -1;
                    for (int attempts = 0; attempts < 10; attempts++) {
                        int t = rand() % cs->party->count;
                        if (cs->party->members[t].alive && t != idx) {
                            target_idx = t; break;
                        }
                    }
                    if (target_idx >= 0) {
                        char msg[LOG_LINE_LEN];
                        int dmg = statblock_phys_damage(actor,
                                      &cs->party->members[target_idx], 10);
                        cs->party->members[target_idx].hp -= dmg;
                        if (cs->party->members[target_idx].hp < 0)
                            cs->party->members[target_idx].hp = 0;
                        snprintf(msg, LOG_LINE_LEN,
                                 "%s attacks ally %s for %d!",
                                 actor->name,
                                 cs->party->members[target_idx].name, dmg);
                        combat_log(cs, msg);
                        if (cs->party->members[target_idx].hp == 0) {
                            cs->party->members[target_idx].alive = 0;
                            if (strcmp(cs->party->members[target_idx].name,
                                       "Prota") == 0) {
                                cs->state = COMBAT_LOSE;
                                return;
                            }
                        }
                    }
                    advance_turn(cs);
                    cs->state = is_party_turn(cs) ?
                                COMBAT_PLAYER_TURN : COMBAT_ENEMY_TURN;
                    return;
                }

                // TURN_NORMAL with active status — enforce Despair/Forget
                // restrictions via cs->state staying COMBAT_PLAYER_TURN
                // The input handler checks status before allowing skill menu
            }
        }
        return; // Let input handler take over for normal player turns
    }

    if (cs->state != COMBAT_ENEMY_TURN) return;
    int idx = current_turn_idx(cs);
    if (idx >= PARTY_MAX)
        enemy_take_turn(cs, idx - PARTY_MAX, gs);
    else {
        advance_turn(cs);
        cs->state = is_party_turn(cs) ? COMBAT_PLAYER_TURN : COMBAT_ENEMY_TURN;
    }
}

// ─── Render ──────────────────────────────────────────────────────────────────

#define SCREEN_W  1280
#define SCREEN_H  720
#define ENEMY_Y   40
#define ENEMY_W   300
#define ENEMY_H   200
#define PARTY_Y   550
#define PARTY_W   290
#define MENU_X    20
#define MENU_Y    390
#define MENU_W    190
#define MENU_H    236
#define LOG_X     220
#define LOG_Y     380
#define LOG_W     840
#define LOG_H     185
#define SKILL_X   220
#define SKILL_Y   280
#define SKILL_W   500
#define SKILL_H   220

// ── Skill learn screen render ─────────────────────────────────────────────────

static void render_skill_learn(CombatSystem* cs, SDL_Renderer* renderer,
                                TTF_Font* font) {
    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color yellow = {255, 220, 100, 255};
    SDL_Color gray   = {150, 150, 180, 255};
    SDL_Color cyan   = { 80, 200, 220, 255};
    SDL_Color green  = {100, 220, 100, 255};
    SDL_Color red    = {220,  60,  60, 255};

    if (cs->learn_index >= cs->learn_count) return;

    PendingLearn* pl = &cs->learn_queue[cs->learn_index];
    StatBlock* m     = &cs->party->members[pl->party_member_idx];
    Skill* new_sk    = skilldb_get(cs->skilldb, pl->new_skill_id);

    // Dark background
    draw_rect(renderer, 0, 0, SCREEN_W, SCREEN_H, 5, 5, 20, 255);

    // Main panel
    draw_rect(renderer,   280, 80, 720, 560, 10, 10, 35, 255);
    draw_border(renderer, 280, 80, 720, 560, 100, 80, 200, 255);

    // Header
    char header[80];
    snprintf(header, 80, "%s can learn a new skill!", m->name);
    draw_text(renderer, font, header, yellow, 300, 92);

    // New skill box
    draw_rect(renderer,   300, 130, 680, 80, 20, 20, 60, 255);
    draw_border(renderer, 300, 130, 680, 80, 80, 80, 200, 255);

    if (new_sk) {
        char new_line[80];
        snprintf(new_line, 80, "NEW: %-16s  SP:%-3d  %s",
                 new_sk->name, new_sk->sp_cost,
                 element_name(new_sk->element));
        draw_text(renderer, font, new_line, green, 316, 142);
        draw_text(renderer, font, new_sk->description, gray, 316, 168);
    }

    // Current skills list
    draw_text(renderer, font, "Current Skills:", gray, 300, 228);
    draw_rect(renderer, 300, 248, 680, 1, 60, 60, 120, 255);

    for (int i = 0; i < m->skill_count; i++) {
        Skill* sk = skilldb_get(cs->skilldb, m->skill_ids[i]);
        if (!sk) continue;

        int sy = 258 + i * 34;
        int is_sel = pl->choosing_replace && i == pl->replace_selected;

        if (is_sel)
            draw_rect(renderer, 300, sy - 2, 680, 30, 50, 30, 10, 255);

        SDL_Color col = is_sel ? yellow : white;
        char line[80];
        snprintf(line, 80, "%s %-16s  SP:%-3d  %s",
                 is_sel ? ">" : " ",
                 sk->name, sk->sp_cost, element_name(sk->element));
        draw_text(renderer, font, line, col, 310, sy);
    }

    // Slot count indicator
    char slots[32];
    snprintf(slots, 32, "Skills: %d / %d",
             m->skill_count, MAX_SKILLS_PER_COMBATANT);
    draw_text(renderer, font, slots, gray, 300, 534);

    // Controls
    if (!pl->choosing_replace) {
        if (m->skill_count < MAX_SKILLS_PER_COMBATANT) {
            draw_text(renderer, font,
                      "E: Learn    X / Esc: Skip",
                      cyan, 300, 558);
        } else {
            draw_text(renderer, font,
                      "E: Replace a skill    X / Esc: Skip",
                      cyan, 300, 558);
        }
    } else {
        draw_text(renderer, font,
                  "Up/Down: select slot    E: Replace    Esc: Cancel",
                  cyan, 300, 558);
    }

    // Queue progress
    if (cs->learn_count > 1) {
        char progress[32];
        snprintf(progress, 32, "%d / %d",
                 cs->learn_index + 1, cs->learn_count);
        draw_text(renderer, font, progress, gray, 940, 92);
    }
}

void combat_render(SDL_Renderer* renderer, CombatSystem* cs,
                   TTF_Font* font) {
    if (!cs->active) return;

    // Skill learn screen takes over entirely
    if (cs->state == COMBAT_SKILL_LEARN) {
        render_skill_learn(cs, renderer, font);
        SDL_RenderPresent(renderer);
        return;
    }

    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color yellow = {255, 220, 100, 255};
    SDL_Color red    = {220,  60,  60, 255};
    SDL_Color gray   = {150, 150, 180, 255};
    SDL_Color cyan   = { 80, 200, 220, 255};
    SDL_Color green  = {100, 220, 100, 255};

    draw_rect(renderer, 0, 0, SCREEN_W, SCREEN_H, 5, 5, 20, 255);

    // Enemy panels
    int enemy_spacing = SCREEN_W / (cs->enemy_count + 1);
    for (int i = 0; i < cs->enemy_count; i++) {
        CombatEnemy* ce = &cs->enemies[i];
        if (!ce->stats.alive) continue;

        int ex = enemy_spacing * (i + 1) - ENEMY_W / 2;
        int ey = ENEMY_Y;

        int targeting = (cs->state == COMBAT_PLAYER_TURN ||
                          cs->state == COMBAT_SKILL_MENU);
        if (targeting && i == cs->target_selected)
            draw_rect(renderer, ex-4, ey-4, ENEMY_W+8, ENEMY_H+8, 80,60,10,255);

        draw_rect(renderer,   ex, ey, ENEMY_W, ENEMY_H, 20, 10, 30, 255);
        draw_border(renderer, ex, ey, ENEMY_W, ENEMY_H, 140, 60, 180, 255);

        char nametag[80];
        if (ce->stats.status != STATUS_NONE)
            snprintf(nametag, 80, "%s [%s]",
                     ce->stats.name, status_name(ce->stats.status));
        else
            strncpy(nametag, ce->stats.name, 79);

        SDL_Color ecol = ce->stats.knocked_down ? yellow : red;
        draw_text(renderer, font, nametag, ecol, ex+8, ey+6);

        char ehp[32];
        snprintf(ehp, 32, "HP %d/%d", ce->stats.hp, ce->stats.max_hp);
        draw_text(renderer, font, ehp, white, ex+8, ey+30);
        draw_hp_bar(renderer, ex+8, ey+54, ENEMY_W-16,
                    ce->stats.hp, ce->stats.max_hp);

        if (cs->gs) {
            const char* es[] = {
                "Ph","Gu","Fi","Ic","El","Wi","Ps","Nu","Bl","Cu"
            };
            int ex2 = ex+8, ey2 = ey+75;
            for (int el = 0; el < ELEMENT_COUNT; el++) {
                if (!gamestate_is_discovered(cs->gs, ce->stats.name, el))
                    continue;
                Affinity aff = statblock_get_affinity(&ce->stats, el);
                SDL_Color ac;
                const char* label;
                switch (aff) {
                    case AFF_WEAK:   ac=(SDL_Color){255,100,100,255};label="Wk";break;
                    case AFF_RESIST: ac=(SDL_Color){100,100,255,255};label="Rs";break;
                    case AFF_NULL:   ac=(SDL_Color){180,180,180,255};label="Nu";break;
                    case AFF_REPEL:  ac=(SDL_Color){255,200, 50,255};label="Rp";break;
                    case AFF_ABSORB: ac=(SDL_Color){ 50,200, 50,255};label="Ab";break;
                    default:         ac=(SDL_Color){150,150,150,255};label="-";break;
                }
                char tag[12];
                snprintf(tag, 12, "%s:%-2s  ", es[el], label);
                draw_text(renderer, font, tag, ac, ex2, ey2);
                ex2 += 72;
                if (ex2 > ex + ENEMY_W - 10) { ex2 = ex+8; ey2 += 22; }
            }
        }

        if (ce->stats.knocked_down)
            draw_text(renderer, font, "DOWN", yellow, ex+8, ey+ENEMY_H-28);
    }

    // Party panels
    for (int i = 0; i < cs->party->count; i++) {
        StatBlock* m = &cs->party->members[i];
        int px = 10 + i * (PARTY_W + 8);
        int py = PARTY_Y;

        int is_active = (i == current_turn_idx(cs) &&
                         cs->state == COMBAT_PLAYER_TURN);
        int is_item_target = 0;
        if (cs->state == COMBAT_ITEM_MENU &&
            cs->inventory && cs->inventory->slot_count > 0) {
            InventorySlot* slot = &cs->inventory->slots[cs->item_selected];
            Item* item = itemdb_get(cs->itemdb, slot->item_id);
            if (item && item->type != ITEM_BATTLE)
                is_item_target =
                    (i == cs->ally_target_selected % cs->party->count);
        }
        int is_skill_ally = 0;
        if (cs->state == COMBAT_SKILL_MENU &&
            cs->party->members[current_turn_idx(cs)].skill_count > 0) {
            int sid = cs->party->members[current_turn_idx(cs)]
                      .skill_ids[cs->skill_selected];
            Skill* sk = skilldb_get(cs->skilldb, sid);
            if (sk && sk->target == TARGET_ONE_ALLY)
                is_skill_ally =
                    (i == cs->ally_target_selected % cs->party->count);
        }

        draw_rect(renderer,   px, py, PARTY_W, 160, 10, 20, 40, 255);
        draw_border(renderer, px, py, PARTY_W, 160,
            is_active ? 255 : (is_item_target||is_skill_ally) ? 100 : 60,
            is_active ? 220 : (is_item_target||is_skill_ally) ? 220 : 60,
            is_active ? 100 : (is_item_target||is_skill_ally) ? 255 : 100,
            255);

        char nametag[80];
        if (m->status != STATUS_NONE)
            snprintf(nametag, 80, "%s [%s]", m->name, status_name(m->status));
        else
            strncpy(nametag, m->name, 79);

        draw_text(renderer, font, nametag,
                  m->alive ? yellow : gray, px+8, py+6);

        if (!m->alive) {
            draw_text(renderer, font, "DOWN", red, px+8, py+30);
            continue;
        }

        char hpstr[32], spstr[32];
        snprintf(hpstr, 32, "HP %d/%d", m->hp, m->max_hp);
        snprintf(spstr, 32, "SP %d/%d", m->sp, m->max_sp);
        draw_text(renderer, font, hpstr, white, px+8, py+28);
        draw_hp_bar(renderer, px+8, py+52, PARTY_W-16, m->hp, m->max_hp);
        draw_text(renderer, font, spstr, cyan,  px+8, py+66);
        draw_sp_bar(renderer, px+8, py+88, PARTY_W-16, m->sp, m->max_sp);

        const char* es2[] = {
            "Ph","Gu","Fi","Ic","El","Wi","Ps","Nu","Bl","Cu"
        };
        int ax = px+8, ay = py+106;
        for (int el = 0; el < ELEMENT_COUNT; el++) {
            Affinity aff = m->affinities[el];
            if (aff == AFF_NORMAL) continue;
            SDL_Color ac;
            const char* label;
            switch (aff) {
                case AFF_WEAK:   ac=(SDL_Color){255,100,100,255};label="Wk";break;
                case AFF_RESIST: ac=(SDL_Color){100,100,255,255};label="Rs";break;
                case AFF_NULL:   ac=(SDL_Color){180,180,180,255};label="Nu";break;
                case AFF_REPEL:  ac=(SDL_Color){255,200, 50,255};label="Rp";break;
                case AFF_ABSORB: ac=(SDL_Color){ 50,200, 50,255};label="Ab";break;
                default:         ac=gray;label="-";break;
            }
            char tag[12];
            snprintf(tag, 12, "%s:%s", es2[el], label);
            draw_text(renderer, font, tag, ac, ax, ay);
            ax += 58;
            if (ax > px + PARTY_W - 10) { ax = px+8; ay += 18; }
        }
    }

    // Battle log
    draw_rect(renderer,   LOG_X, LOG_Y, LOG_W, LOG_H, 5, 5, 20, 220);
    draw_border(renderer, LOG_X, LOG_Y, LOG_W, LOG_H, 80, 80, 160, 255);
    for (int i = 0; i < cs->log_count; i++)
        draw_text(renderer, font, cs->log[i], white,
                  LOG_X+10, LOG_Y+8+i*28);

    // Root menu
    if (cs->state == COMBAT_PLAYER_TURN) {
        int tidx = current_turn_idx(cs);
        if (tidx >= PARTY_MAX) tidx = 0;  // guard against enemy index
        StatBlock* actor = &cs->party->members[tidx];
        char header[64];
        snprintf(header, 64, "%s's turn", actor->name);
        draw_text(renderer, font, header, yellow, MENU_X, MENU_Y-28);
        draw_rect(renderer,   MENU_X, MENU_Y, MENU_W, MENU_H, 10,10,30,255);
        draw_border(renderer, MENU_X, MENU_Y, MENU_W, MENU_H, 80,80,160,255);
        // Check if negotiation is available (exactly 1 enemy alive)
        int alive_count = 0;
        for (int ei = 0; ei < cs->enemy_count; ei++)
            if (cs->enemies[ei].stats.alive) alive_count++;
        int standing_r = 0;
        for (int ei = 0; ei < cs->enemy_count; ei++)
            if (cs->enemies[ei].stats.alive && !cs->enemies[ei].stats.knocked_down)
                standing_r++;
        int can_negotiate = (alive_count == 1 && standing_r == 1);

        // Build option list — must match input handler exactly
        StatBlock* render_actor = &cs->party->members[tidx];
        int is_prota_r = (strcmp(render_actor->name, "Prota") == 0);
        extern SummonStock g_summon_stock;
        static char p_buf[32];
        if (is_prota_r && g_summon_stock.count > 0)
            snprintf(p_buf, 32, "Summon (%s)",
                     g_summon_stock.stock[g_summon_stock.active_idx].name);
        else strncpy(p_buf, "Summon", 31);

        // Indices: 0=Attack 1=Skills 2=Items 3=Flee [4=Summon if prota] [4or5=Talk]
        const char* opt_labels[6] = {
            "Attack", "Skills", "Items", "Flee",
            is_prota_r ? p_buf : (can_negotiate ? "Talk" : NULL),
            is_prota_r && can_negotiate ? "Talk" : NULL
        };
        int opt_count = 4 + (is_prota_r ? 1 : 0) + (can_negotiate ? 1 : 0);

        for (int i = 0; i < opt_count; i++) {
            if (!opt_labels[i]) continue;
            char label[48];
            snprintf(label, 48, "%s %s",
                     i == cs->menu_selected ? ">" : " ", opt_labels[i]);
            SDL_Color opt_col = i == cs->menu_selected ? yellow : white;
            if (is_prota_r && i == 4 && g_summon_stock.count <= 1)
                opt_col = i == cs->menu_selected ? yellow : (SDL_Color){100,100,120,255};
            draw_text(renderer, font, label, opt_col,
                      MENU_X+10, MENU_Y+10+i*36);
        }
        draw_text(renderer, font, "L/R: target  U/D: menu",
                  gray, MENU_X, MENU_Y+MENU_H+6);
    }

    // Summon menu (Prota only, free action)
    if (cs->state == COMBAT_SUMMON_MENU) {
        extern SummonStock g_summon_stock;
        draw_rect(renderer,   260, 200, 400, 300, 10,10,30,255);
        draw_border(renderer, 260, 200, 400, 300, 120,60,200,255);
        draw_text(renderer, font, "Switch Summon  (Esc: back)",
                  gray, 275, 208);
        draw_rect(renderer, 260, 228, 400, 1, 80,60,160,255);
        for (int i = 0; i < g_summon_stock.count; i++) {
            Summon* p = &g_summon_stock.stock[i];
            int py = 238 + i * 44;
            int is_sel    = (i == cs->summon_selected);
            int is_active = (i == g_summon_stock.active_idx);
            if (is_sel)
                draw_rect(renderer, 268, py-2, 384, 40, 40,30,80,255);
            char label[80];
            snprintf(label, 80, "%s %s | %s  Lv.%d",
                     is_sel ? ">" : " ",
                     p->name, arcana_name(p->arcana), p->level);
            SDL_Color col = is_sel ? yellow : white;
            draw_text(renderer, font, label, col, 275, py);
            if (is_active)
                draw_text(renderer, font, "[Active]", green, 520, py);
            // Affinity summary
            const char* es[]={"Ph","Gu","Fi","Ic","El","Wi","Ps","Nu","Bl","Cu"};
            const char* as[]={"","Wk","Rs","Nu","Rp","Ab"};
            char aff[64]="";
            for (int e=0;e<ELEMENT_COUNT;e++) {
                if (p->affinities[e]!=AFF_NORMAL) {
                    char b[12]; snprintf(b,12,"%s:%s ",es[e],as[p->affinities[e]]);
                    strncat(aff,b,sizeof(aff)-strlen(aff)-1);
                }
            }
            if (aff[0]) draw_text(renderer, font, aff, gray, 280, py+22);
        }
    }

    // Skill menu
    if (cs->state == COMBAT_SKILL_MENU) {
        StatBlock* actor = &cs->party->members[current_turn_idx(cs)];
        draw_rect(renderer,   SKILL_X, SKILL_Y, SKILL_W, SKILL_H, 10,10,30,255);
        draw_border(renderer, SKILL_X, SKILL_Y, SKILL_W, SKILL_H, 80,80,160,255);
        draw_text(renderer, font, "Skills  (Esc: back)",
                  gray, SKILL_X+10, SKILL_Y+6);
        for (int i = 0; i < actor->skill_count; i++) {
            Skill* sk = skilldb_get(cs->skilldb, actor->skill_ids[i]);
            if (!sk) continue;
            int dc = sk->sp_cost;
            if (sk->inflicts != STATUS_NONE && cs->bonds &&
                bond_has_perk(cs->bonds, PERK_STATUS_NO_SP))
                dc = 0;
            char label[80];
            snprintf(label, 80, "%s %-14s  SP:%-3d  %s",
                     i == cs->skill_selected ? ">" : " ",
                     sk->name, dc, element_name(sk->element));
            draw_text(renderer, font, label,
                      i == cs->skill_selected ? yellow : white,
                      SKILL_X+10, SKILL_Y+32+i*30);
        }
    }

    // Item menu
    if (cs->state == COMBAT_ITEM_MENU) {
        draw_rect(renderer,   SKILL_X, SKILL_Y, SKILL_W, SKILL_H, 10,10,30,255);
        draw_border(renderer, SKILL_X, SKILL_Y, SKILL_W, SKILL_H, 80,80,160,255);
        draw_text(renderer, font, "Items  (Esc: back)  L/R: target ally",
                  gray, SKILL_X+10, SKILL_Y+6);
        if (!cs->inventory || cs->inventory->slot_count == 0) {
            draw_text(renderer, font, "No items.", white, SKILL_X+10, SKILL_Y+40);
        } else {
            for (int i = 0; i < cs->inventory->slot_count; i++) {
                InventorySlot* slot = &cs->inventory->slots[i];
                Item* item = itemdb_get(cs->itemdb, slot->item_id);
                if (!item) continue;
                char label[80];
                snprintf(label, 80, "%s %-20s x%d",
                         i == cs->item_selected ? ">" : " ",
                         item->name, slot->quantity);
                draw_text(renderer, font, label,
                          i == cs->item_selected ? yellow : white,
                          SKILL_X+10, SKILL_Y+32+i*30);
                if (i == cs->item_selected)
                    draw_text(renderer, font, item->description, gray,
                              SKILL_X+10, SKILL_Y+SKILL_H-28);
            }
        }
    }

    // Negotiation overlay
    render_negotiation(renderer, cs, font);

    // Team attack
    if (cs->state == COMBAT_TEAM_ATTACK) {
        draw_rect(renderer,   300, 240, 680, 200, 20, 10, 40, 245);
        draw_border(renderer, 300, 240, 680, 200, 200, 100, 255, 255);
        draw_text(renderer, font, "STANDOFF!", yellow, 580, 255);

        const char* hu_opts[] = { "Team Attack", "Talk", "Skip" };
        for (int i = 0; i < 3; i++) {
            int oy = 290 + i * 44;
            if (i == cs->menu_selected) {
                draw_rect(renderer, 308, oy - 4, 664, 36, 60, 30, 100, 255);
            }
            SDL_Color hc = i == cs->menu_selected ? yellow : white;
            char hlabel[32];
            snprintf(hlabel, 32, "%s %s",
                     i == cs->menu_selected ? ">" : " ", hu_opts[i]);
            draw_text(renderer, font, hlabel, hc, 320, oy);
        }
        draw_text(renderer, font, "Up/Down: select  E: confirm",
                  gray, 320, 418);
    }

    // End screens
    if (cs->state == COMBAT_WIN) {
        draw_rect(renderer,   280, 250, 720, 180, 10, 40, 10, 240);
        draw_border(renderer, 280, 250, 720, 180, 60, 200, 60, 255);
        draw_text(renderer, font, "Victory!", green, 540, 270);
        int total_exp = 0;
        for (int i = 0; i < cs->enemy_count; i++)
            total_exp += cs->enemies[i].exp_reward;
        if (cs->bonds) {
            int bonus = bond_get_exp_bonus(cs->bonds);
            if (bonus > 0) total_exp += total_exp * bonus / 100;
        }
        // Calculate yen earned
        int total_yen = 0;
        for (int i = 0; i < cs->enemy_count; i++)
            total_yen += (cs->enemies[i].exp_reward * 60) / 100;

        char expstr[64];
        snprintf(expstr, 64, "EXP gained: %d", total_exp);
        draw_text(renderer, font, expstr, white, 380, 310);

        char yenstr[48];
        snprintf(yenstr, 48, "Yen: +%d", total_yen);
        SDL_Color gold = {255, 200, 50, 255};
        draw_text(renderer, font, yenstr, gold, 620, 310);

        for (int i = 0; i < cs->party->count; i++) {
            StatBlock* m = &cs->party->members[i];
            if (!m->alive) continue;
            char pstr[80];
            snprintf(pstr, 80, "%s  Lv.%d  EXP: %d/%d",
                     m->name, m->level, m->exp, m->level*100);
            draw_text(renderer, font, pstr, white, 260, 340+i*28);
        }
        const char* hint = cs->learn_count > 0
                           ? "Press E to continue (skill learn pending!)"
                           : "Press E to continue.";
        draw_text(renderer, font, hint,
                  cs->learn_count > 0 ? yellow : gray, 380, 410);
    }
    if (cs->state == COMBAT_LOSE) {
        draw_rect(renderer, 0, 0, SCREEN_W, SCREEN_H, 0, 0, 0, 255);
        draw_rect(renderer,   280, 250, 720, 200, 40, 10, 10, 240);
        draw_border(renderer, 280, 250, 720, 200, 200, 60, 60, 255);
        draw_text(renderer, font, "GAME OVER", red, 500, 270);
        draw_text(renderer, font, "Press E to exit.", gray, 460, 370);
    }
    if (cs->state == COMBAT_FLEE) {
        draw_rect(renderer,   280, 250, 720, 140, 10, 10, 40, 240);
        draw_border(renderer, 280, 250, 720, 140, 80, 80, 200, 255);
        draw_text(renderer, font, "Escaped!", white, 540, 270);
        draw_text(renderer, font, "Press E to continue.", gray, 460, 310);
    }
}