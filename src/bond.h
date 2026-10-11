#ifndef BOND_H
#define BOND_H

#ifdef OPAL_GDEXTENSION
#include "sdl_compat.h"
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#endif
#include "social_stats.h"
#include "notify.h"
#include "time_system.h"

#define MAX_BONDS      12
#define MAX_BOND_RANKS 10
#define MAX_PERKS           10
#define BOND_NAME_LEN  64

// Perk types — what each rank unlock does
typedef enum {
    PERK_NONE = 0,
    PERK_SHOP_DISCOUNT,         // % discount at shops
    PERK_EXP_BONUS,             // % bonus EXP from battles
    PERK_FLEE_BONUS,            // improved flee rate
    PERK_FOLLOWUP_ATTACK,       // chance of follow-up after knockdown
    PERK_STEAL_CHANCE,          // chance to steal items on physical
    PERK_STATUS_DURATION,       // status effects last longer
    PERK_TECHNICAL_BONUS,       // technical damage multiplier
    PERK_STATUS_NO_SP,          // status skills cost no SP
    PERK_STATUS_ALWAYS_HIT,     // status skills never miss
    PERK_GUN_IGNORE_RESIST,     // gun attacks ignore resistances
    PERK_EXTRA_TURN_CHANCE,     // chance to act again after action
    PERK_UNLOCK_ITEMS,          // unlocks new shop stock (tier 1/2/3)
    PERK_ITEM_POTENCY,          // items restore more HP/SP
} PerkType;

typedef struct {
    PerkType type;
    int      value;     // e.g. 10 for 10% discount, 1/2/3 for unlock tier
    char     description[128];
} Perk;

typedef struct {
    int  points_required;   // relationship points to reach this rank
    Perk perk;
    char rank_up_message[128];

    // Social stat requirement to reach this rank
    int            req_stat_type;   // -1 if none
    int            req_stat_rank;
} BondRank;

typedef struct {
    char         name[BOND_NAME_LEN];
    char         arcana[BOND_NAME_LEN];
    char         description[256];
    int          current_rank;      // 0 = not started, 1-10
    int          points;            // current relationship points
    int          max_rank;          // usually 10
    BondRank ranks[MAX_BOND_RANKS];
    char         npc_name_match[BOND_NAME_LEN];
    int          points_per_hangout; // base points gained per hangout
    int          active;            // is this bond available?
    int rankup_in_progress;   // transient: a rank-up scene is playing
} Bond;

typedef struct {
    Bond bonds[MAX_BONDS];
    int       count;
} BondDB;

// Active perk query functions
int  bond_get_shop_discount(BondDB* db);
int  bond_get_exp_bonus(BondDB* db);
int  bond_has_perk(BondDB* db, PerkType perk);
int  bond_get_perk_value(BondDB* db, PerkType perk);
void bond_check_rankup(Bond* c, NotifySystem* ns);
void bond_award_points_by_name(BondDB* db, const char* name,
                                     int points, NotifySystem* ns);
void bond_apply_rankup(Bond* c, NotifySystem* ns);
void bond_resolve_pending_rankups(BondDB* db, NotifySystem* ns);

void bonddb_init(BondDB* db);
void bond_hangout(Bond* c, SocialStats* ss,
                       NotifySystem* ns, TimeSystem* ts);
int  bond_can_hangout(Bond* c, SocialStats* ss);

Bond* bond_find_by_name(BondDB* db, const char* name);

// UI
typedef enum {
    BOND_MENU_CLOSED,
    BOND_MENU_LIST,
    BOND_MENU_DETAIL
} BondMenuState;

typedef struct {
    BondMenuState state;
    int                selected;
    int                pending;  // open detail after dialogue ends
} BondMenu;

void bondmenu_init(BondMenu* cm);
void bondmenu_open(BondMenu* cm);
void bondmenu_close(BondMenu* cm);
void bondmenu_handle_input(BondMenu* cm, SDL_Event* event,
                                 BondDB* db, SocialStats* ss,
                                 NotifySystem* ns, TimeSystem* ts);
void bondmenu_render(BondMenu* cm, SDL_Renderer* renderer,
                           TTF_Font* font, BondDB* db,
                           SocialStats* ss);

#endif