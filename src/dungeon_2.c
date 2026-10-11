#include "dungeon_builder.h"
#include "skills.h"

// ── Dungeon 2: The Primeval Forest of Lust ─────────────────────────────────────
// A twisted ancient forest where the ruler has warped nature into a prison
// for those they desire. Beauty conceals danger at every turn.
//
// Entry: dungeon_2_unlocked flag
// Deadline: day 40
// Boss: Shadow of Desire — Curse phase 1, Bless immune + AoE Curse phase 2
//
// Floor layout:
//   F1: Safe(0) -> East Path(1) -+-> Convergence Safe(4) -> Stairs(5)
//                  West Path(2) -+
//                  Hidden Glade(3)[chest, off East Path]
//
//   F2: Overgrown Hall(0) -> Tangled Depths(1) -> Sacred Grove Safe(2)
//                                                -> Throne Clearing(3)[boss]

void build_dungeon_2(Dungeon* p, EnemyRegistry* reg) {
    DungeonBuilder pb;
    pb_init(&pb, p, reg);

    pb_set_meta(&pb,
        "The Primeval Forest",
        "The Desirous One",
        "dungeon_2_cleared",
        "dungeon_2_unlocked",
        40);

    // ── Floor 1: The Entangled Wood ───────────────────────────────────────────
    pb_begin_floor(&pb, 1, "The Entangled Wood");

    int f1r0 = pb_add_safe_room(&pb, "Forest Entrance");

    // East path — Curse-heavy
    int f1r1 = pb_add_normal_room(&pb,
        "Eastern Thicket",
        "f2_f1r1_cleared",
        ENEMY_ALLURING_WISP, 2,
        ENEMY_NONE,          0);

    // West path — Physical/Wind heavy
    int f1r2 = pb_add_normal_room(&pb,
        "Western Briar",
        "f2_f1r2_cleared",
        ENEMY_THORNED_SHADE, 1,
        ENEMY_ALLURING_WISP, 1);

    // Hidden glade off east path — chest reward
    int f1r3 = pb_add_chest_room(&pb,
        "Hidden Glade",
        1, 4);   // Medicine DX x4

    // Convergence — both paths meet here
    int f1r4 = pb_add_safe_room(&pb, "Ancient Clearing");

    int f1r5 = pb_add_normal_room(&pb,
        "Overgrown Stairway",
        "f2_f1r5_cleared",
        ENEMY_TWISTED_DRYAD, 1,
        ENEMY_ALLURING_WISP, 1);

    // Connections — branching structure
    pb_connect(&pb, f1r0, EXIT_SOUTH, f1r1);   // south -> east path
    pb_connect(&pb, f1r0, EXIT_WEST,  f1r2);   // west  -> west path
    pb_connect(&pb, f1r1, EXIT_EAST,  f1r3);   // east path -> hidden glade
    pb_connect(&pb, f1r1, EXIT_SOUTH, f1r4);   // east path -> convergence
    pb_connect(&pb, f1r2, EXIT_SOUTH, f1r4);   // west path -> convergence
    pb_connect(&pb, f1r4, EXIT_SOUTH, f1r5);   // convergence -> stairs
    pb_set_stairs(&pb, f1r5);

    pb_end_floor(&pb);

    // ── Floor 2: The Sacred Grove ─────────────────────────────────────────────
    pb_begin_floor(&pb, 2, "The Sacred Grove");

    int f2r0 = pb_add_normal_room(&pb,
        "Overgrown Hall",
        "f2_f2r0_cleared",
        ENEMY_TWISTED_DRYAD,  1,
        ENEMY_THORNED_SHADE,  1);

    int f2r1 = pb_add_normal_room(&pb,
        "Tangled Depths",
        "f2_f2r1_cleared",
        ENEMY_ALLURING_WISP,  2,
        ENEMY_TWISTED_DRYAD,  1);

    int f2r2 = pb_add_safe_room(&pb, "Sacred Grove Sanctuary");

    // ── Shadow of Desire boss ──────────────────────────────────────────────────
    BossConfig boss = pb_make_boss(reg,
        ENEMY_NONE,                 // custom stats
        700, 150,                   // HP, SP
        16, 24, 16, 18, 14,         // ST MA EN AG LU
        1000,                       // EXP
        53,                         // pre_battle_node
        57);                        // post_battle_node

    // Give proper name
    strncpy(boss.enemy.stats.name, "Shadow of Desire", 63);

    // Phase 1 skills: Attack, Hex, Eigaon, Psi
    boss.enemy.stats.skill_ids[0] = 0;   // Attack
    boss.enemy.stats.skill_ids[1] = 26;  // Hex
    boss.enemy.stats.skill_ids[2] = 27;  // Eigaon
    boss.enemy.stats.skill_ids[3] = 19;  // Psi
    boss.enemy.stats.skill_count  = 4;

    statblock_set_affinity(&boss.enemy.stats, ELEM_FIRE,  AFF_WEAK);
    statblock_set_affinity(&boss.enemy.stats, ELEM_CURSE, AFF_NULL);
    statblock_set_affinity(&boss.enemy.stats, ELEM_BLESS, AFF_RESIST);
    statblock_set_affinity(&boss.enemy.stats, ELEM_WIND,  AFF_RESIST);

    // Phase 2: gains Bless immunity, switches to AoE curse + wind
    pb_boss_set_phase2_hp_pct(&boss, 50);
    pb_boss_set_phase2_immune(&boss, ELEM_BLESS);
    pb_boss_add_phase2_skill(&boss, 27);  // Eigaon
    pb_boss_add_phase2_skill(&boss, 18);  // Gust Wave
    pb_boss_add_phase2_skill(&boss, 28);  // Mapsi (AoE psy)
    pb_boss_add_phase2_skill(&boss, 0);   // Attack

    pb_boss_add_loot(&boss, 14, 100);   // Revival Bead (guaranteed)
    pb_boss_set_treasure_equip(&boss, 27); // Forest Crown (equip id 27)

    int f2r3 = pb_add_boss_room(&pb, "The Desirous Throne", &boss);

    pb_connect(&pb, f2r0, EXIT_SOUTH, f2r1);
    pb_connect(&pb, f2r1, EXIT_EAST,  f2r2);
    pb_connect(&pb, f2r1, EXIT_SOUTH, f2r3);

    pb_end_floor(&pb);
    pb_set_team_meeting(&pb, 49, "team_meeting_done_p2");
    pb_finish(&pb);
}