#include "stats.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>

void statblock_init(StatBlock* sb, const char* name) {
    memset(sb, 0, sizeof(StatBlock));
    strncpy(sb->name, name, 63);
    sb->level = 1;
    sb->alive = 1;
    statblock_clear_affinities(sb);
    sb->can_hand_off    = 0;
    sb->handoff_level       = 0;
    sb->handoff_sp_discount = 0;

    // No equipment by default
    for (int i = 0; i < EQUIP_SLOT_COUNT; i++)
        sb->equipped[i] = -1;
}

void statblock_clear_affinities(StatBlock* sb) {
    for (int i = 0; i < ELEMENT_COUNT; i++)
        sb->affinities[i] = AFF_NORMAL;
}

void statblock_set_affinity(StatBlock* sb, Element elem, Affinity aff) {
    sb->affinities[elem] = aff;
}

Affinity statblock_get_affinity(StatBlock* sb, Element elem) {
    return sb->affinities[elem];
}

int statblock_add_exp(StatBlock* sb, int exp) {
    sb->exp += exp;
    int levels_gained = 0;
    int threshold = sb->level * 100;
    while (sb->exp >= threshold) {
        sb->exp -= threshold;
        sb->level++;
        levels_gained++;
        int hp_gain = 10 + sb->en;
        int sp_gain = 5  + sb->ma / 2;
        sb->max_hp += hp_gain;
        sb->max_sp += sp_gain;
        sb->hp     += hp_gain;
        sb->sp     += sp_gain;
        if (sb->hp > sb->max_hp) sb->hp = sb->max_hp;
        if (sb->sp > sb->max_sp) sb->sp = sb->max_sp;
        sb->st++;
        sb->ma++;
        sb->en++;
        sb->ag++;
        sb->lu++;
        threshold = sb->level * 100;
    }
    return levels_gained;
}

int statblock_phys_damage(StatBlock* attacker, StatBlock* defender, int power) {
    int base = (attacker->st * power) / 10;
    int def  = defender->en / 4;
    int dmg  = base - def;
    int var  = dmg * 15 / 100;
    dmg += (rand() % (var * 2 + 1)) - var;
    if (dmg < 1) dmg = 1;
    dmg = (dmg * statblock_handoff_multiplier_pct(attacker)) / 100;
    return dmg;
}

int statblock_handoff_multiplier_pct(StatBlock* sb) {
    switch (sb->handoff_level) {
        case 1: return 150;
        case 2: return 200;
        case 3: return 250;
        default: return 100;
    }
}

int statblock_magic_damage(StatBlock* attacker, StatBlock* defender,
                           int power, Element elem) {
    int base = (attacker->ma * power) / 10;
    int def  = defender->en / 6;
    int dmg  = base - def;

    Affinity aff = statblock_get_affinity(defender, elem);
    switch (aff) {
        case AFF_WEAK:   dmg = dmg * 3 / 2; break;
        case AFF_RESIST: dmg = dmg / 2;     break;
        case AFF_NULL:   dmg = 0;           break;
        case AFF_REPEL:  dmg = -dmg;        break;
        case AFF_ABSORB: dmg = -dmg / 2;   break;
        default: break;
    }

    int var = abs(dmg) * 15 / 100;
    if (dmg > 0) dmg += (rand() % (var * 2 + 1)) - var;
    if (dmg < 1 && aff == AFF_NORMAL) dmg = 1;
    dmg = (dmg * statblock_handoff_multiplier_pct(attacker)) / 100;
    return dmg;
}

int statblock_process_status(StatBlock* sb) {
    if (sb->status == STATUS_NONE) return 0;
    sb->status_turns--;
    if (sb->status_turns <= 0) {
        statblock_clear_status(sb);
        return 0;
    }
    return 1;
}

void statblock_clear_status(StatBlock* sb) {
    sb->status       = STATUS_NONE;
    sb->status_turns = 0;
}

void statblock_apply_status(StatBlock* sb, StatusEffect status, int turns) {
    if (sb->status != STATUS_NONE) return;
    sb->status       = status;
    sb->status_turns = turns;
}

const char* element_name(Element e) {
    switch (e) {
        case ELEM_PHYSICAL: return "Physical";
        case ELEM_GUN:      return "Gun";
        case ELEM_FIRE:     return "Fire";
        case ELEM_ICE:      return "Ice";
        case ELEM_ELECTRIC: return "Electric";
        case ELEM_WIND:     return "Wind";
        case ELEM_PSYCHIC:  return "Psychic";
        case ELEM_NUCLEAR:  return "Nuclear";
        case ELEM_BLESS:    return "Bless";
        case ELEM_CURSE:    return "Curse";
        default:            return "Unknown";
    }
}

const char* affinity_name(Affinity a) {
    switch (a) {
        case AFF_NORMAL:  return "-";
        case AFF_WEAK:    return "Wk";
        case AFF_RESIST:  return "Rs";
        case AFF_NULL:    return "Nu";
        case AFF_REPEL:   return "Rp";
        case AFF_ABSORB:  return "Ab";
        default:          return "?";
    }
}

const char* status_name(StatusEffect s) {
    switch (s) {
        case STATUS_BURN:      return "Burn";
        case STATUS_FREEZE:    return "Freeze";
        case STATUS_SHOCK:     return "Shock";
        case STATUS_DIZZY:     return "Dizzy";
        case STATUS_SLEEP:     return "Sleep";
        case STATUS_CONFUSE:   return "Confuse";
        case STATUS_FEAR:      return "Fear";
        case STATUS_DESPAIR:   return "Despair";
        case STATUS_BRAINWASH: return "Brainwash";
        case STATUS_FORGET:    return "Forget";
        default:               return "";
    }
}