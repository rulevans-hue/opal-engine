#ifndef STATS_H
#define STATS_H

#define ELEMENT_COUNT            10
#define MAX_SKILLS_PER_COMBATANT  8
#define EQUIP_SLOT_COUNT          3   // weapon, armor, accessory

typedef enum {
    ELEM_PHYSICAL = 0,
    ELEM_GUN,
    ELEM_FIRE,
    ELEM_ICE,
    ELEM_ELECTRIC,
    ELEM_WIND,
    ELEM_PSYCHIC,
    ELEM_NUCLEAR,
    ELEM_BLESS,
    ELEM_CURSE
} Element;

typedef enum {
    AFF_NORMAL = 0,
    AFF_WEAK,
    AFF_RESIST,
    AFF_NULL,
    AFF_REPEL,
    AFF_ABSORB
} Affinity;

typedef enum {
    STATUS_NONE = 0,
    STATUS_BURN,
    STATUS_FREEZE,
    STATUS_SHOCK,
    STATUS_DIZZY,
    STATUS_SLEEP,
    STATUS_CONFUSE,
    STATUS_FEAR,
    STATUS_DESPAIR,
    STATUS_BRAINWASH,
    STATUS_FORGET
} StatusEffect;

typedef struct {
    char name[64];
    int  level;
    int  exp;

    int  hp,     max_hp;
    int  sp,     max_sp;
    int  st;     // strength
    int  ma;     // magic
    int  en;     // endurance
    int  ag;     // agility
    int  lu;     // luck

    Affinity     affinities[ELEMENT_COUNT];
    StatusEffect status;
    int          status_turns;
    int          knocked_down;
    int          extra_turn;
    int          can_hand_off;
    int          handoff_level;        // 0=none, 1/2/3 = stacked passes
    int          handoff_sp_discount;  // 1 if handoff SP discount applies this turn
    int          alive;

    int          skill_ids[MAX_SKILLS_PER_COMBATANT];
    int          skill_count;

    // Equipment: index = EquipSlot, value = EquipmentPiece id, -1 = none
    int          equipped[EQUIP_SLOT_COUNT];
} StatBlock;

void statblock_init(StatBlock* sb, const char* name);

void statblock_clear_affinities(StatBlock* sb);
void statblock_set_affinity(StatBlock* sb, Element elem, Affinity aff);
Affinity statblock_get_affinity(StatBlock* sb, Element elem);

int statblock_add_exp(StatBlock* sb, int exp);  // returns 1 if leveled up

int statblock_phys_damage(StatBlock* attacker, StatBlock* defender, int power);
// Returns handoff multiplier × 100 (e.g. 150 = 1.5x). Always 100 if handoff_level==0.
int statblock_handoff_multiplier_pct(StatBlock* sb);
int statblock_magic_damage(StatBlock* attacker, StatBlock* defender,
                           int power, Element elem);

int  statblock_process_status(StatBlock* sb);
void statblock_clear_status(StatBlock* sb);
void statblock_apply_status(StatBlock* sb, StatusEffect status, int turns);

const char* element_name(Element e);
const char* affinity_name(Affinity a);
const char* status_name(StatusEffect s);

#endif