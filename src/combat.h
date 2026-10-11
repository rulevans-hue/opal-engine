#ifndef COMBAT_H
#define COMBAT_H

#include "bond.h"
#ifdef OPAL_GDEXTENSION
#include "sdl_compat.h"
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#endif
#include "stats.h"
#include "skills.h"
#include "party.h"
#include "gamestate.h"
#include "inventory.h"
#include "notify.h"

#define MAX_ENEMIES      4
#define MAX_LOOT_ENTRIES 6
#define LOG_LINES        6
#define LOG_LINE_LEN     128

typedef enum {
    COMBAT_INACTIVE,
    COMBAT_PLAYER_TURN,
    COMBAT_SKILL_MENU,
    COMBAT_ITEM_MENU,
    COMBAT_TARGET_SELECT,
    COMBAT_ENEMY_TURN,
    COMBAT_TEAM_ATTACK,
    COMBAT_WIN,
    COMBAT_LOSE,
    COMBAT_FLEE,
    COMBAT_SKILL_LEARN,  // post-victory skill learn screen
    COMBAT_NEGOTIATE,
    COMBAT_SUMMON_MENU     // enemy negotiation screen
} CombatState;

typedef struct {
    StatBlock stats;
    char      win_flag[64];
    int       exp_reward;
    int       loot_ids[MAX_LOOT_ENTRIES];
    int       loot_chances[MAX_LOOT_ENTRIES];
    int       loot_count;
} CombatEnemy;

// ── Pending skill learn ───────────────────────────────────────────────────────
#define MAX_PENDING_LEARNS 4

typedef struct {
    int party_member_idx;  // index into party->members
    int new_skill_id;      // skill being offered
    int replace_selected;  // which existing slot is highlighted when replacing
    int choosing_replace;  // 1 = player is picking a slot to replace
} PendingLearn;

typedef struct {
    Party*      party;
    SkillDB*    skilldb;
    CombatEnemy enemies[MAX_ENEMIES];
    int         enemy_count;
    int         turn_order[PARTY_MAX + MAX_ENEMIES];
    int         turn_count;
    int         turn_index;
    CombatState state;
    int         active;
    int         just_ended;
    int         won_last_battle;
    int         menu_selected;
    int         skill_selected;
    int         item_selected;
    int         summon_selected;
    int         target_selected;
    int         ally_target_selected;
    int         team_attack_available;
    char        log[LOG_LINES][LOG_LINE_LEN];
    int         log_count;
    char        win_flag[64];
    GameState*  gs;
    Inventory*  inventory;
    ItemDB*     itemdb;
    NotifySystem* notify;
    BondDB*  bonds;

    // Skill learn queue — filled during apply_exp_rewards, shown post-victory
    PendingLearn learn_queue[MAX_PENDING_LEARNS];
    int          learn_count;
    int          learn_index;

    // Negotiation state
    int          neg_enemy_idx;      // which enemy we are talking to
    int          neg_phase;          // 0=greeting, 1=demand, 2=result
    int          neg_selected;       // current choice highlight
    int          neg_result;         // 0=pending, 1=success, 2=failure
    int          neg_demand_type;    // 0=money, 1=item, 2=flattery
    int          neg_demand_amount;  // yen or item qty demanded
    int          neg_demand_item;    // item id if demand_type==1
    int          handoff_chain_count;  // number of passes in current chain (0-3)
    
} CombatSystem;

void combat_init(CombatSystem* cs, Party* party, SkillDB* skilldb,
                 GameState* gs, Inventory* inventory, ItemDB* itemdb,
                 NotifySystem* notify, BondDB* bonds);
void combat_start(CombatSystem* cs, CombatEnemy* enemies, int count,
                  const char* win_flag);

// Hand-off — pass Extra Turn to an ally.
// Returns 1 on success, 0 on failure (no extra_turn, target not alive, etc.)
int combat_handoff_available(CombatSystem* cs, int from_idx);
int combat_hand_off(CombatSystem* cs, int from_idx, int to_idx);
void combat_handle_input(CombatSystem* cs, SDL_Event* event,
                         GameState* gs);
void combat_update(CombatSystem* cs, GameState* gs);
void combat_render(SDL_Renderer* renderer, CombatSystem* cs,
                   TTF_Font* font);
void combat_log(CombatSystem* cs, const char* msg);

CombatEnemy make_combat_enemy(const char* name,
                               int hp, int sp,
                               int st, int ma, int en, int ag, int lu,
                               int exp_reward,
                               const char* win_flag);

#endif