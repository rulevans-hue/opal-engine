#include "characters.h"
#include <string.h>

// ─── Skill Learn Tables ──────────────────────────────────────────────────────
// Each entry: { level, skill_id }
// Skills are offered one at a time as the character levels up.
// IDs match the skilldb defined in skills.c:
//   0=Attack  1=Cleave  2=Rampage  3=Shot    4=Burst
//   5=Ember     6=Blaze 7=Inferno  8=Ember Wave
//   9=Frost   10=Freeze 11=Glacier 12=Frost Wave
//  13=Spark   14=Thunderbolt  15=Spark Wave
//  16=Gust  17=Cyclone 18=Gust Wave
//  19=Psi   20=Psydyne
//  21=Fission  22=Meltdown
//  23=Gleam 24=Kougaon  25=Banish
//  26=Hex  27=Eigaon   28=Doom
//  29=Mend   30=Greater Mend  31=Mass Mend

// ── Prota ─────────────────────────────────────────────────────────────────────
// Starts: Attack, Ember, Spark, Mend
static const SkillLearnTable prota_learns = {
    {
        {  3, 3  },   // Shot
        {  5, 6  },   // Blaze
        {  7, 14 },   // Thunderbolt
        {  9, 30 },   // Greater Mend
        { 11, 2  },   // Rampage
        { 13, 8  },   // Ember Wave
        { 15, 15 },   // Spark Wave
        { 17, 7  },   // Inferno
        { 20, 4  },   // Burst
        { 23, 19 },   // Psi
        { 26, 31 },   // Mass Mend
        { 30, 20 },   // Psydyne
    },
    12
};

// ── Rook ─────────────────────────────────────────────────────────────────────
// Starts: Attack, Cleave, Spark, Spark Wave
static const SkillLearnTable rook_learns = {
    {
        {  3, 2  },   // Rampage
        {  5, 14 },   // Thunderbolt
        {  7, 1  },   // Cleave (if replaced, this upgrades it)
        {  9, 4  },   // Burst
        { 11, 21 },   // Fission
        { 14, 22 },   // Meltdown
        { 17, 15 },   // Spark Wave (already known — upgrade slot)
        { 20, 16 },   // Gust
        { 24, 18 },   // Gust Wave
        { 28, 17 },   // Cyclone
    },
    10
};

// ── Wren ───────────────────────────────────────────────────────────────────────
// Starts: Attack, Ember, Inferno, Mend, Mass Mend
static const SkillLearnTable wren_learns = {
    {
        {  3, 6  },   // Blaze
        {  5, 8  },   // Ember Wave
        {  7, 30 },   // Greater Mend
        {  9, 23 },   // Gleam
        { 12, 24 },   // Kougaon
        { 15, 19 },   // Psi
        { 18, 20 },   // Psydyne
        { 21, 25 },   // Banish
        { 25, 31 },   // Mass Mend (upgrade)
        { 29, 7  },   // Inferno (upgrade)
    },
    10
};

// ── Sable ────────────────────────────────────────────────────────────────────
// Starts: Attack, Cleave, Frost, Glacier
static const SkillLearnTable sable_learns = {
    {
        {  3, 10 },   // Freeze
        {  5, 12 },   // Frost Wave
        {  7, 16 },   // Gust
        {  9, 2  },   // Rampage
        { 11, 17 },   // Cyclone
        { 14, 18 },   // Gust Wave
        { 17, 21 },   // Fission
        { 20, 11 },   // Glacier (upgrade)
        { 23, 22 },   // Meltdown
        { 27, 3  },   // Shot
        { 30, 4  },   // Burst
    },
    11
};

// ─── Lookup ──────────────────────────────────────────────────────────────────

const SkillLearnTable* characters_get_learn_table(const char* name) {
    if (strcmp(name, "Prota")  == 0) return &prota_learns;
    if (strcmp(name, "Rook")  == 0) return &rook_learns;
    if (strcmp(name, "Wren")    == 0) return &wren_learns;
    if (strcmp(name, "Sable") == 0) return &sable_learns;
    return NULL;
}

int characters_skill_at_level(const char* name, int level) {
    const SkillLearnTable* table = characters_get_learn_table(name);
    if (!table) return -1;
    for (int i = 0; i < table->count; i++)
        if (table->entries[i].level == level)
            return table->entries[i].skill_id;
    return -1;
}

// ─── Character builders ──────────────────────────────────────────────────────

StatBlock make_protagonist() {
    StatBlock sb;
    statblock_init(&sb, "Prota");
    sb.level  = 1;
    sb.max_hp = 80;  sb.hp = 80;
    sb.max_sp = 60;  sb.sp = 60;
    sb.st = 10; sb.ma = 10; sb.en = 8;
    sb.ag = 12; sb.lu = 10;
    statblock_clear_affinities(&sb);
    sb.skill_ids[0] = 0;   // Attack
    sb.skill_ids[1] = 5;   // Ember
    sb.skill_ids[2] = 13;  // Spark
    sb.skill_ids[3] = 29;  // Mend
    sb.skill_count  = 4;
    return sb;
}

StatBlock make_rook() {
    StatBlock sb;
    statblock_init(&sb, "Rook");
    sb.level  = 1;
    sb.max_hp = 100; sb.hp = 100;
    sb.max_sp = 40;  sb.sp = 40;
    sb.st = 14; sb.ma = 6;  sb.en = 10;
    sb.ag = 8;  sb.lu = 8;
    statblock_clear_affinities(&sb);
    statblock_set_affinity(&sb, ELEM_WIND,  AFF_WEAK);
    statblock_set_affinity(&sb, ELEM_BLESS, AFF_RESIST);
    sb.skill_ids[0] = 0;   // Attack
    sb.skill_ids[1] = 1;   // Cleave
    sb.skill_ids[2] = 13;  // Spark
    sb.skill_ids[3] = 15;  // Spark Wave
    sb.skill_count  = 4;
    return sb;
}

StatBlock make_wren() {
    StatBlock sb;
    statblock_init(&sb, "Wren");
    sb.level  = 1;
    sb.max_hp = 70;  sb.hp = 70;
    sb.max_sp = 80;  sb.sp = 80;
    sb.st = 6;  sb.ma = 14; sb.en = 6;
    sb.ag = 10; sb.lu = 10;
    statblock_clear_affinities(&sb);
    statblock_set_affinity(&sb, ELEM_FIRE, AFF_NULL);
    statblock_set_affinity(&sb, ELEM_ICE,  AFF_WEAK);
    sb.skill_ids[0] = 0;   // Attack
    sb.skill_ids[1] = 5;   // Ember
    sb.skill_ids[2] = 7;   // Inferno
    sb.skill_ids[3] = 29;  // Mend
    sb.skill_ids[4] = 31;  // Mass Mend
    sb.skill_count  = 5;
    return sb;
}

StatBlock make_sable() {
    StatBlock sb;
    statblock_init(&sb, "Sable");
    sb.level  = 1;
    sb.max_hp = 85;  sb.hp = 85;
    sb.max_sp = 50;  sb.sp = 50;
    sb.st = 12; sb.ma = 10; sb.en = 8;
    sb.ag = 11; sb.lu = 9;
    statblock_clear_affinities(&sb);
    statblock_set_affinity(&sb, ELEM_ICE,      AFF_NULL);
    statblock_set_affinity(&sb, ELEM_ELECTRIC, AFF_WEAK);
    sb.skill_ids[0] = 0;   // Attack
    sb.skill_ids[1] = 1;   // Cleave
    sb.skill_ids[2] = 9;   // Frost
    sb.skill_ids[3] = 11;  // Glacier
    sb.skill_count  = 4;
    return sb;
}

void build_starter_party(Party* p) {
    party_init(p);
    party_add_member(p, make_protagonist());
    party_add_member(p, make_rook());
    party_add_member(p, make_wren());
    party_add_member(p, make_sable());
}