#ifndef QUEST_H
#define QUEST_H

#include "gamestate.h"
#include "inventory.h"
#include "bond.h"
#include "notify.h"

#define MAX_QUESTS         32
#define QUEST_ID_LEN       64
#define QUEST_TEXT_LEN     256
#define QUEST_FLAG_LEN     64
#define MAX_QUEST_REWARDS  4

typedef enum {
    QUEST_REWARD_NONE = 0,
    QUEST_REWARD_YEN,
    QUEST_REWARD_ITEM,
    QUEST_REWARD_BOND
} QuestRewardType;

typedef struct {
    QuestRewardType type;
    int  amount;                   // yen amount, item qty, or bond points
    int  item_id;                  // used when type == QUEST_REWARD_ITEM
    char bond[QUEST_ID_LEN];  // used when type == QUEST_REWARD_BOND
} QuestReward;

typedef struct {
    char id[QUEST_ID_LEN];
    char title[QUEST_TEXT_LEN];
    char description[QUEST_TEXT_LEN];
    char unlock_flag[QUEST_FLAG_LEN];
    char complete_flag[QUEST_FLAG_LEN];

    QuestReward rewards[MAX_QUEST_REWARDS];
    int         reward_count;

    // runtime state
    int unlocked;
    int completed;
    int reward_given;
    int active;
} Quest;

typedef struct {
    Quest quests[MAX_QUESTS];
    int   count;
} QuestRegistry;

void quest_registry_init(QuestRegistry* reg);
int  quest_registry_load_json(QuestRegistry* reg, const char* filepath);

// Per-frame tick: checks unlock/complete flags, grants rewards.
// Returns the number of quests that just completed this tick (for notify).
int  quest_tick(QuestRegistry* reg, GameState* gs, Inventory* inv,
                BondDB* cdb, NotifySystem* ns);

Quest* quest_find_by_id(QuestRegistry* reg, const char* id);

#endif