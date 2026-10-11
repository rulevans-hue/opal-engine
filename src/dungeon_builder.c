#include "dungeon_builder.h"
#include <string.h>
#include <stdio.h>

// ── Internal map builders ─────────────────────────────────────────────────────
// These are shared with dungeon.c via a forward declaration.
// All dungeon map geometry lives here — dungeon_builder owns the visual layout.

extern void dungeon_map_init(Map* m);

static void map_normal(Map* m) {
    dungeon_map_init(m);
    // 4x4 pillars in a 2x2 grid — open center and edges
    int pcols[] = {12, 60};
    int prows[] = {10, 32};
    for (int pi = 0; pi < 2; pi++)
        for (int pj = 0; pj < 2; pj++)
            for (int dr = 0; dr < 4; dr++)
                for (int dc = 0; dc < 4; dc++)
                    m->tiles[prows[pj]+dr][pcols[pi]+dc] = TILE_WALL;
}

static void map_safe(Map* m) {
    dungeon_map_init(m);
    // Fully open — safe rooms distinguished by background tint in render
}

static void map_chest(Map* m) {
    dungeon_map_init(m);
    // Small U-shaped alcove behind the chest position
    for (int c = 33; c <= 46; c++) m->tiles[16][c] = TILE_WALL;
    for (int r = 16; r <= 26; r++) m->tiles[r][33]  = TILE_WALL;
    for (int r = 16; r <= 26; r++) m->tiles[r][46]  = TILE_WALL;
}

static void map_boss(Map* m) {
    dungeon_map_init(m);
    // Grand throne room — pillars only, NO border walls
    // Border walls block edge transitions so they must not be used here
    int pcols[] = {15, 62};
    int prows[] = {10, 30};
    for (int pi = 0; pi < 2; pi++)
        for (int pj = 0; pj < 2; pj++)
            for (int dr = 0; dr < 3; dr++)
                for (int dc = 0; dc < 2; dc++)
                    m->tiles[prows[pj]+dr][pcols[pi]+dc] = TILE_WALL;
}

// ── Room constructors ─────────────────────────────────────────────────────────

static DungeonRoom make_room_base(int id, DungeonRoomType type,
                                   const char* name, const char* desc) {
    DungeonRoom r;
    memset(&r, 0, sizeof(DungeonRoom));
    r.id   = id;
    r.type = type;
    strncpy(r.name,        name, 63);
    strncpy(r.description, desc ? desc : "", 127);
    for (int i = 0; i < 4; i++) r.exits[i] = -1;
    entitylist_init(&r.entities);
    r.chest.item_id  = -1;
    r.chest.equip_id = -1;
    return r;
}

// ── Lifecycle ─────────────────────────────────────────────────────────────────

void pb_init(DungeonBuilder* pb, Dungeon* dungeon, EnemyRegistry* reg) {
    memset(pb, 0, sizeof(DungeonBuilder));
    memset(dungeon, 0, sizeof(Dungeon));
    pb->dungeon        = dungeon;
    pb->reg           = reg;
    pb->current_floor = -1;
    pb->building      = 0;

    dungeon->alert_level        = 0;
    dungeon->active             = 0;
    dungeon->current_floor      = 0;
    dungeon->floor_count        = 0;
    dungeon->team_meeting_node  = -1;
    dungeon->team_meeting_flag[0] = '\0';
}

void pb_set_meta(DungeonBuilder* pb,
                 const char* name, const char* ruler,
                 const char* clear_flag, const char* entry_flag,
                 int deadline_day) {
    strncpy(pb->dungeon->name,       name,       63);
    strncpy(pb->dungeon->ruler,      ruler,      63);
    strncpy(pb->dungeon->clear_flag, clear_flag, 63);
    strncpy(pb->dungeon->entry_flag, entry_flag, 63);
    pb->dungeon->deadline_day = deadline_day;
}

void pb_begin_floor(DungeonBuilder* pb, int floor_number, const char* name) {
    if (pb->dungeon->floor_count >= MAX_DUNGEON_FLOORS) {
        printf("pb_begin_floor: too many floors\n");
        return;
    }
    pb->current_floor = pb->dungeon->floor_count;
    pb->dungeon->floor_count++;
    pb->building = 1;

    DungeonFloor* f = &pb->dungeon->floors[pb->current_floor];
    memset(f, 0, sizeof(DungeonFloor));
    f->floor_number   = floor_number;
    f->room_count     = 0;
    f->current_room   = 0;
    f->stairs_room_id = -1;
    f->safe_room_count = 0;
    strncpy(f->name, name, 63);
}

void pb_end_floor(DungeonBuilder* pb) {
    pb->building = 0;
}

void pb_set_stairs(DungeonBuilder* pb, int room_id) {
    if (pb->current_floor < 0) return;
    pb->dungeon->floors[pb->current_floor].stairs_room_id = room_id;
}

void pb_set_chest_flag(DungeonBuilder* pb, int room_id, const char* flag) {
    if (pb->current_floor < 0) return;
    DungeonFloor* f = &pb->dungeon->floors[pb->current_floor];
    if (room_id < 0 || room_id >= f->room_count) return;
    strncpy(f->rooms[room_id].chest.sets_flag, flag ? flag : "", 63);
    f->rooms[room_id].chest.sets_flag[63] = '\0';
}

void pb_finish(DungeonBuilder* pb) {
    // Mark the last floor as having no stairs (it's the final floor)
    if (pb->dungeon->floor_count > 0) {
        int last = pb->dungeon->floor_count - 1;
        if (pb->dungeon->floors[last].stairs_room_id < 0)
            pb->dungeon->floors[last].stairs_room_id = -1;
    }
    printf("Dungeon built: '%s' — %d floors\n",
           pb->dungeon->name, pb->dungeon->floor_count);
}

// ── Internal room adder ───────────────────────────────────────────────────────

static int add_room(DungeonBuilder* pb, DungeonRoom* r) {
    if (pb->current_floor < 0) return -1;
    DungeonFloor* f = &pb->dungeon->floors[pb->current_floor];
    if (f->room_count >= MAX_DUNGEON_ROOMS) {
        printf("pb: too many rooms on floor %d\n", pb->current_floor + 1);
        return -1;
    }
    int id = f->room_count;
    r->id = id;
    f->rooms[id] = *r;
    f->room_count++;
    return id;
}

// ── Room adders ───────────────────────────────────────────────────────────────

int pb_add_safe_room(DungeonBuilder* pb, const char* name) {
    DungeonRoom r = make_room_base(-1, ROOM_SAFE, name,
        "A safe haven. HP and SP restored.");
    map_safe(&r.map);

    DungeonFloor* f = &pb->dungeon->floors[pb->current_floor];
    int id = add_room(pb, &r);
    if (id >= 0 && f->safe_room_count < 4)
        f->safe_room_ids[f->safe_room_count++] = id;
    return id;
}

int pb_add_normal_room(DungeonBuilder* pb,
                       const char* name, const char* win_flag,
                       EnemyID enemy1_id, int enemy1_count,
                       EnemyID enemy2_id, int enemy2_count) {
    DungeonRoom r = make_room_base(-1, ROOM_NORMAL, name,
        "Shadows lurk here.");
    map_normal(&r.map);

    // Build enemy group from registry
    EnemyTemplate group[PB_MAX_ENEMIES_PER_ROOM];
    int total = 0;

    if (enemy1_id >= 0 && enemy1_count > 0 && pb->reg) {
        EnemyTemplate base = enemy_registry_get(pb->reg, (EnemyID)enemy1_id);
        for (int i = 0; i < enemy1_count && total < PB_MAX_ENEMIES_PER_ROOM; i++)
            group[total++] = base;
    }
    if (enemy2_id >= 0 && enemy2_count > 0 && pb->reg) {
        EnemyTemplate base = enemy_registry_get(pb->reg, (EnemyID)enemy2_id);
        for (int i = 0; i < enemy2_count && total < PB_MAX_ENEMIES_PER_ROOM; i++)
            group[total++] = base;
    }

    if (total > 0)
        entitylist_add_enemy(&r.entities, 560, 480, 32, 32,
                             "Shadows", win_flag, group, total);

    // Store spawn for patrol
    if (r.entities.count > 0) {
        r.entities.entities[r.entities.count-1].pre_combat_x = 560.0f;
        r.entities.entities[r.entities.count-1].pre_combat_y = 480.0f;
    }

    return add_room(pb, &r);
}

int pb_add_chest_room(DungeonBuilder* pb, const char* name,
                      int item_id, int item_qty) {
    DungeonRoom r = make_room_base(-1, ROOM_CHEST, name,
        "A glittering treasure lies within.");
    map_chest(&r.map);
    r.chest.item_id  = item_id;
    r.chest.item_qty = item_qty;
    r.chest.equip_id = -1;
    return add_room(pb, &r);
}

int pb_add_chest_room_equip(DungeonBuilder* pb, const char* name,
                             int equip_id) {
    DungeonRoom r = make_room_base(-1, ROOM_CHEST, name,
        "A powerful item rests here.");
    map_chest(&r.map);
    r.chest.item_id  = -1;
    r.chest.item_qty = 0;
    r.chest.equip_id = equip_id;
    return add_room(pb, &r);
}

int pb_add_treasure_room(DungeonBuilder* pb, const char* name, int equip_id) {
    DungeonRoom r = make_room_base(-1, ROOM_CHEST, name,
        "The dungeon treasure. This is what you came for.");
    map_chest(&r.map);
    r.chest.item_id            = -1;
    r.chest.item_qty           = 0;
    r.chest.equip_id           = equip_id;
    r.chest.is_dungeon_treasure = 1;
    return add_room(pb, &r);
}

int pb_add_boss_room(DungeonBuilder* pb, const char* name, BossConfig* boss) {
    DungeonRoom r = make_room_base(-1, ROOM_BOSS, name,
        "An immense presence fills the air. There is no turning back.");
    map_boss(&r.map);
    r.boss = *boss;
    return add_room(pb, &r);
}

// ── Connections ───────────────────────────────────────────────────────────────

// Returns the opposite direction
static ExitDir opposite(ExitDir d) {
    switch (d) {
        case EXIT_NORTH: return EXIT_SOUTH;
        case EXIT_SOUTH: return EXIT_NORTH;
        case EXIT_EAST:  return EXIT_WEST;
        case EXIT_WEST:  return EXIT_EAST;
        default:         return EXIT_NORTH;
    }
}

void pb_connect(DungeonBuilder* pb, int room_a, ExitDir dir, int room_b) {
    if (pb->current_floor < 0) return;
    DungeonFloor* f = &pb->dungeon->floors[pb->current_floor];
    if (room_a < 0 || room_a >= f->room_count) return;
    if (room_b < 0 || room_b >= f->room_count) return;

    f->rooms[room_a].exits[dir]           = room_b;
    f->rooms[room_b].exits[opposite(dir)] = room_a;
}

void pb_connect_one_way(DungeonBuilder* pb, int from_room,
                         ExitDir dir, int to_room) {
    if (pb->current_floor < 0) return;
    DungeonFloor* f = &pb->dungeon->floors[pb->current_floor];
    if (from_room < 0 || from_room >= f->room_count) return;
    if (to_room  < 0 || to_room  >= f->room_count) return;
    f->rooms[from_room].exits[dir] = to_room;
}

// ── Boss config helpers ───────────────────────────────────────────────────────

BossConfig pb_make_boss(EnemyRegistry* reg,
                         EnemyID id,
                         int hp, int sp,
                         int st, int ma, int en, int ag, int lu,
                         int exp_reward,
                         int pre_battle_node,
                         int post_battle_node) {
    BossConfig boss;
    memset(&boss, 0, sizeof(BossConfig));

    // Start from registry template if available, override stats
    if (reg && id >= 0 && id < ENEMY_COUNT) {
        EnemyTemplate tmpl = enemy_registry_get(reg, id);
        boss.enemy = make_combat_enemy(tmpl.name, hp, sp,
                                        st, ma, en, ag, lu,
                                        exp_reward, "defeated_boss");
        // Copy affinities and skills from template
        for (int i = 0; i < ELEMENT_COUNT; i++)
            statblock_set_affinity(&boss.enemy.stats, i, tmpl.affinities[i]);
        boss.enemy.stats.skill_count = tmpl.skill_count;
        for (int i = 0; i < tmpl.skill_count; i++)
            boss.enemy.stats.skill_ids[i] = tmpl.skill_ids[i];
        boss.enemy.loot_count = tmpl.loot_count;
        for (int i = 0; i < tmpl.loot_count; i++) {
            boss.enemy.loot_ids[i]     = tmpl.loot_ids[i];
            boss.enemy.loot_chances[i] = tmpl.loot_chances[i];
        }
    } else {
        // Blank boss
        boss.enemy = make_combat_enemy("Boss", hp, sp,
                                        st, ma, en, ag, lu,
                                        exp_reward, "defeated_boss");
    }

    boss.phase2_hp_pct      = 50;
    boss.phase2_immune_elem = -1;
    boss.phase2_skill_count = 0;
    boss.pre_battle_node    = pre_battle_node;
    boss.post_battle_node   = post_battle_node;
    boss.in_phase2          = 0;
    boss.defeated           = 0;
    boss.treasure_equip_id  = -1;
    boss.treasure_item_id   = -1;
    boss.treasure_item_qty  = 0;

    return boss;
}

void pb_boss_add_phase2_skill(BossConfig* boss, int skill_id) {
    if (boss->phase2_skill_count >= 4) return;
    boss->phase2_skill_ids[boss->phase2_skill_count++] = skill_id;
}

void pb_boss_set_phase2_immune(BossConfig* boss, int element) {
    boss->phase2_immune_elem = element;
}

void pb_boss_set_phase2_hp_pct(BossConfig* boss, int pct) {
    boss->phase2_hp_pct = pct;
}

void pb_boss_add_loot(BossConfig* boss, int item_id, int chance) {
    if (boss->enemy.loot_count >= MAX_LOOT_ENTRIES) return;
    int i = boss->enemy.loot_count++;
    boss->enemy.loot_ids[i]     = item_id;
    boss->enemy.loot_chances[i] = chance;
}

void pb_boss_set_treasure_equip(BossConfig* boss, int equip_id) {
    boss->treasure_equip_id = equip_id;
    boss->treasure_item_id  = -1;
    boss->treasure_item_qty = 0;
}

void pb_boss_set_treasure_item(BossConfig* boss, int item_id, int qty) {
    boss->treasure_item_id  = item_id;
    boss->treasure_item_qty = qty;
    boss->treasure_equip_id = -1;
}

void pb_set_team_meeting(DungeonBuilder* pb, int node, const char* flag) {
    pb->dungeon->team_meeting_node = node;
    strncpy(pb->dungeon->team_meeting_flag, flag ? flag : "", 63);
}