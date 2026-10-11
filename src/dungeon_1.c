#include "dungeon_builder.h"
#include "skills.h"

// ── Dungeon 1: Castle of Greed ─────────────────────────────────────────────────
// Entry: hungerscape_unlocked flag, afternoon or later
// Deadline: day 19
// Boss: Shadow King — Physical/Fire phase 1, Electric immune + AoE phase 2
//
// Floor layout:
//   F1: Safe(0) -> Guard(1) -> Scorched(2) -east-> Windswept(3) -> Stairs(4)
//   F2: Antechamber(0) -east-> Alcove(1)[chest]
//                       -south-> Armory(2) -east-> Safe(3)
//                                           -south-> Coin Chamber(4) -east-> Cache(5)[chest]
//                                                                     -south-> Stairs(6)
//   F3: Royal Guard(0) -> Antechamber(1) -east-> Treasure(2)[dungeon]
//                                         -south-> Throne(3)[boss]

void build_dungeon_1(Dungeon* p, EnemyRegistry* reg) {
    DungeonBuilder pb;
    pb_init(&pb, p, reg);

    pb_set_meta(&pb,
        "Castle of Greed",
        "The Greedy King",
        "dungeon_1_cleared",
        "hungerscape_unlocked",
        19);

    // ── Floor 1: Entry Hall ───────────────────────────────────────────────────
    pb_begin_floor(&pb, 1, "Entry Hall");

    int f1r0 = pb_add_safe_room(&pb, "Dungeon Entrance");

    int f1r1 = pb_add_normal_room(&pb,
        "Guard Chamber",
        "f1r1_cleared",
        ENEMY_SHADOW_T2, 2,
        ENEMY_NONE, 0);

    int f1r2 = pb_add_normal_room(&pb,
        "Scorched Hall",
        "f1r2_cleared",
        ENEMY_FLAME_DANCER, 1,
        ENEMY_SHADOW_T2,    1);

    int f1r3 = pb_add_normal_room(&pb,
        "Windswept Gallery",
        "f1r3_cleared",
        ENEMY_STORM_HOUND, 1,
        ENEMY_SHADOW_T2,   1);

    int f1r4 = pb_add_normal_room(&pb,
        "Descending Staircase",
        "f1r4_cleared",
        ENEMY_NONE, 0,
        ENEMY_NONE, 0);

    pb_connect(&pb, f1r0, EXIT_SOUTH, f1r1);
    pb_connect(&pb, f1r1, EXIT_SOUTH, f1r2);
    pb_connect(&pb, f1r2, EXIT_EAST,  f1r3);
    pb_connect(&pb, f1r3, EXIT_SOUTH, f1r4);
    pb_set_stairs(&pb, f1r4);

    pb_end_floor(&pb);

    // ── Floor 2: Treasury ────────────────────────────────────────────────────
    pb_begin_floor(&pb, 2, "Treasury");

    int f2r0 = pb_add_normal_room(&pb,
        "Vault Antechamber",
        "f2r0_cleared",
        ENEMY_DARK_SPIRIT_T1, 1,
        ENEMY_SHADOW_T2,      1);

    int f2r1 = pb_add_chest_room(&pb,
        "Hidden Alcove",
        1, 3);   // Medicine DX x3

    int f2r2 = pb_add_normal_room(&pb,
        "Armory",
        "f2r2_cleared",
        ENEMY_CURSED_ARMOR, 1,
        ENEMY_SHADOW_T2,    1);

    int f2r3 = pb_add_safe_room(&pb, "Treasury Safe Room");

    int f2r4 = pb_add_normal_room(&pb,
        "Coin Chamber",
        "f2r4_cleared",
        ENEMY_DARK_SPIRIT_T1, 2,
        ENEMY_NONE,           0);

    int f2r5 = pb_add_chest_room_equip(&pb,
        "Weapon Cache",
        1);   // Iron Sword (equip id 1)
    pb_set_chest_flag(&pb, f2r5, "quest_mara_sword_obtained");

    int f2r6 = pb_add_normal_room(&pb,
        "Grand Staircase",
        "f2r6_cleared",
        ENEMY_NONE, 0,
        ENEMY_NONE, 0);

    pb_connect(&pb, f2r0, EXIT_EAST,  f2r1);
    pb_connect(&pb, f2r0, EXIT_SOUTH, f2r2);
    pb_connect(&pb, f2r2, EXIT_EAST,  f2r3);
    pb_connect(&pb, f2r2, EXIT_SOUTH, f2r4);
    pb_connect(&pb, f2r4, EXIT_EAST,  f2r5);
    pb_connect(&pb, f2r4, EXIT_SOUTH, f2r6);
    pb_set_stairs(&pb, f2r6);

    pb_end_floor(&pb);

    // ── Floor 3: Throne Room ─────────────────────────────────────────────────
    pb_begin_floor(&pb, 3, "Throne Room");

    // Safe room at floor 3 entrance so player can rest/save/exit
    int f3r0 = pb_add_safe_room(&pb, "Royal Antechamber");

    int f3r1 = pb_add_normal_room(&pb,
        "Royal Guard Post",
        "f3r1_cleared",
        ENEMY_SHADOW_T3, 3,
        ENEMY_NONE,      0);

    int f3r2_inner = pb_add_normal_room(&pb,
        "Throne Antechamber",
        "f3r2_cleared",
        ENEMY_CURSED_ARMOR,   1,
        ENEMY_DARK_SPIRIT_T1, 1);

    // The dungeon treasure materializes in the boss chamber — collected on defeat
    // No separate treasure room needed. The BossConfig stores the reward.

    // ── Shadow King boss ──────────────────────────────────────────────────────
    BossConfig boss = pb_make_boss(reg,
        ENEMY_NONE,                  // no registry template — custom stats
        600, 120,                    // HP, SP
        22, 20, 18, 14, 12,          // ST MA EN AG LU
        800,                         // EXP
        22,                          // pre_battle_node
        26);                         // post_battle_node

    strncpy(boss.enemy.stats.name, "Shadow King", 63);
    boss.enemy.stats.name[63] = '\0';

    // Phase 1 skills: Attack, Cleave, Ember, Eigaon
    boss.enemy.stats.skill_ids[0] = 0;   // Attack
    boss.enemy.stats.skill_ids[1] = 1;   // Cleave
    boss.enemy.stats.skill_ids[2] = 5;   // Ember
    boss.enemy.stats.skill_ids[3] = 27;  // Eigaon
    boss.enemy.stats.skill_count  = 4;

    statblock_set_affinity(&boss.enemy.stats, ELEM_BLESS,    AFF_WEAK);
    statblock_set_affinity(&boss.enemy.stats, ELEM_CURSE,    AFF_NULL);
    statblock_set_affinity(&boss.enemy.stats, ELEM_PHYSICAL, AFF_RESIST);

    // Phase 2: gains Electric immunity, switches to AoE fire + thunder
    pb_boss_set_phase2_hp_pct(&boss, 50);
    pb_boss_set_phase2_immune(&boss, ELEM_ELECTRIC);
    pb_boss_add_phase2_skill(&boss, 2);   // Rampage
    pb_boss_add_phase2_skill(&boss, 7);   // Inferno
    pb_boss_add_phase2_skill(&boss, 8);   // Ember Wave
    pb_boss_add_phase2_skill(&boss, 14);  // Thunderbolt

    // Loot
    pb_boss_add_loot(&boss, 14, 100);   // Revival Bead (guaranteed)
    pb_boss_set_treasure_equip(&boss, 26);  // Champion's Medal — given on defeat

    int f3r3 = pb_add_boss_room(&pb, "Throne of Greed", &boss);

    pb_connect(&pb, f3r0, EXIT_SOUTH, f3r1);
    pb_connect(&pb, f3r1, EXIT_SOUTH, f3r2_inner);
    pb_connect(&pb, f3r2_inner, EXIT_SOUTH, f3r3);
    // No stairs on floor 3 (final floor)

    pb_end_floor(&pb);
    pb_set_team_meeting(&pb, 42, "team_meeting_done");
    pb_finish(&pb);
}