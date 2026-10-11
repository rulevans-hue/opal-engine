#include "skills.h"
#include <string.h>
#include <stdio.h>
#include "skill_loader.h"

void skilldb_init(SkillDB* db) {
    db->count = 0;

    // Try data-driven load first; fall back to hardcoded below if it fails.
    if (skill_load_json(db, "data/skills.json"))
        return;

    // Helper macro to reduce repetition
    #define ADD_SKILL(ID, NAME, DESC, ELEM, SP, PWR, TGT, INFL, CHANCE, HEAL) \
    { \
        Skill s = {0}; \
        s.id = ID; \
        strncpy(s.name, NAME, 63); \
        strncpy(s.description, DESC, 127); \
        s.element = ELEM; \
        s.sp_cost = SP; \
        s.power   = PWR; \
        s.target  = TGT; \
        s.inflicts = INFL; \
        s.status_chance = CHANCE; \
        s.is_heal = HEAL; \
        skilldb_add(db, s); \
    }

    // Physical
    ADD_SKILL(0,  "Attack",      "A basic physical strike.",
              ELEM_PHYSICAL, 0,  10, TARGET_ONE_ENEMY,  STATUS_NONE, 0.0f, 0)
    ADD_SKILL(1,  "Cleave",      "A heavy physical slash.",
              ELEM_PHYSICAL, 0,  18, TARGET_ONE_ENEMY,  STATUS_NONE, 0.0f, 0)
    ADD_SKILL(2,  "Rampage",     "Strike all enemies.",
              ELEM_PHYSICAL, 8,  12, TARGET_ALL_ENEMIES,STATUS_NONE, 0.0f, 0)

    // Gun
    ADD_SKILL(3,  "Shot",        "A single gunshot.",
              ELEM_GUN,      3,  10, TARGET_ONE_ENEMY,  STATUS_NONE, 0.0f, 0)
    ADD_SKILL(4,  "Burst",       "Fire at all enemies.",
              ELEM_GUN,      8,   8, TARGET_ALL_ENEMIES,STATUS_NONE, 0.0f, 0)

    // Fire
    ADD_SKILL(5,  "Ember",         "A small fire attack.",
              ELEM_FIRE,     4,  10, TARGET_ONE_ENEMY,  STATUS_BURN, 0.2f, 0)
    ADD_SKILL(6,  "Blaze",     "A medium fire attack.",
              ELEM_FIRE,     8,  18, TARGET_ONE_ENEMY,  STATUS_BURN, 0.3f, 0)
    ADD_SKILL(7,  "Inferno",     "A heavy fire attack.",
              ELEM_FIRE,    16,  28, TARGET_ONE_ENEMY,  STATUS_BURN, 0.4f, 0)
    ADD_SKILL(8,  "Ember Wave",      "Fire hits all enemies.",
              ELEM_FIRE,    12,  10, TARGET_ALL_ENEMIES,STATUS_BURN, 0.2f, 0)

    // Ice
    ADD_SKILL(9,  "Frost",        "A small ice attack.",
              ELEM_ICE,      4,  10, TARGET_ONE_ENEMY,  STATUS_FREEZE,0.2f,0)
    ADD_SKILL(10, "Freeze",    "A medium ice attack.",
              ELEM_ICE,      8,  18, TARGET_ONE_ENEMY,  STATUS_FREEZE,0.3f,0)
    ADD_SKILL(11, "Glacier",    "A heavy ice attack.",
              ELEM_ICE,     16,  28, TARGET_ONE_ENEMY,  STATUS_FREEZE,0.4f,0)
    ADD_SKILL(12, "Frost Wave",      "Ice hits all enemies.",
              ELEM_ICE,     12,  10, TARGET_ALL_ENEMIES,STATUS_FREEZE,0.2f,0)

    // Electric
    ADD_SKILL(13, "Spark",         "A small electric attack.",
              ELEM_ELECTRIC, 4,  10, TARGET_ONE_ENEMY,  STATUS_SHOCK, 0.2f,0)
    ADD_SKILL(14, "Thunderbolt",     "A heavy electric attack.",
              ELEM_ELECTRIC,16,  28, TARGET_ONE_ENEMY,  STATUS_SHOCK, 0.4f,0)
    ADD_SKILL(15, "Spark Wave",       "Electric hits all enemies.",
              ELEM_ELECTRIC,12,  10, TARGET_ALL_ENEMIES,STATUS_SHOCK, 0.2f,0)

    // Wind
    ADD_SKILL(16, "Gust",        "A small wind attack.",
              ELEM_WIND,     4,  10, TARGET_ONE_ENEMY,  STATUS_DIZZY, 0.2f,0)
    ADD_SKILL(17, "Cyclone",    "A heavy wind attack.",
              ELEM_WIND,    16,  28, TARGET_ONE_ENEMY,  STATUS_DIZZY, 0.4f,0)
    ADD_SKILL(18, "Gust Wave",      "Wind hits all enemies.",
              ELEM_WIND,    12,  10, TARGET_ALL_ENEMIES,STATUS_DIZZY, 0.2f,0)

    // Psychic
    ADD_SKILL(19, "Psi",         "A small psychic attack.",
              ELEM_PSYCHIC,  4,  10, TARGET_ONE_ENEMY,  STATUS_CONFUSE,0.2f,0)
    ADD_SKILL(20, "Psydyne",     "A heavy psychic attack.",
              ELEM_PSYCHIC, 16,  28, TARGET_ONE_ENEMY,  STATUS_CONFUSE,0.4f,0)

    // Nuclear
    ADD_SKILL(21, "Fission",        "A small nuclear attack.",
              ELEM_NUCLEAR,  4,  10, TARGET_ONE_ENEMY,  STATUS_DIZZY, 0.2f,0)
    ADD_SKILL(22, "Meltdown",    "A heavy nuclear attack.",
              ELEM_NUCLEAR, 16,  28, TARGET_ONE_ENEMY,  STATUS_DIZZY, 0.4f,0)

    // Bless
    ADD_SKILL(23, "Gleam",       "A small bless attack.",
              ELEM_BLESS,    4,  10, TARGET_ONE_ENEMY,  STATUS_FEAR,  0.2f,0)
    ADD_SKILL(24, "Kougaon",     "A heavy bless attack.",
              ELEM_BLESS,   16,  28, TARGET_ONE_ENEMY,  STATUS_FEAR,  0.4f,0)
    ADD_SKILL(25, "Banish",        "Instant kill (bless). Low chance.",
              ELEM_BLESS,    8,   0, TARGET_ONE_ENEMY,  STATUS_NONE,  0.0f,0)

    // Curse
    ADD_SKILL(26, "Hex",        "A small curse attack.",
              ELEM_CURSE,    4,  10, TARGET_ONE_ENEMY,  STATUS_DESPAIR,0.2f,0)
    ADD_SKILL(27, "Eigaon",      "A heavy curse attack.",
              ELEM_CURSE,   16,  28, TARGET_ONE_ENEMY,  STATUS_DESPAIR,0.4f,0)
    ADD_SKILL(28, "Doom",        "Instant kill (curse). Low chance.",
              ELEM_CURSE,    8,   0, TARGET_ONE_ENEMY,  STATUS_NONE,  0.0f,0)

    // Healing
    ADD_SKILL(29, "Mend",         "Restore a little HP.",
              ELEM_BLESS,    3,  15, TARGET_ONE_ALLY,   STATUS_NONE,  0.0f,1)
    ADD_SKILL(30, "Greater Mend",     "Restore moderate HP.",
              ELEM_BLESS,    8,  30, TARGET_ONE_ALLY,   STATUS_NONE,  0.0f,1)
    ADD_SKILL(31, "Mass Mend",   "Restore HP to all allies.",
              ELEM_BLESS,   16,  25, TARGET_ALL_ALLIES, STATUS_NONE,  0.0f,1)

    #undef ADD_SKILL
}

Skill* skilldb_get(SkillDB* db, int id) {
    for (int i = 0; i < db->count; i++)
        if (db->skills[i].id == id) return &db->skills[i];
    return NULL;
}

int skilldb_add(SkillDB* db, Skill skill) {
    if (db->count >= MAX_SKILLS) return -1;
    db->skills[db->count] = skill;
    return db->count++;
}