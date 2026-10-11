#include "dungeon_loader.h"
#include "dungeon_builder.h"
#include "enemy_registry.h"
#include "dungeon_db.h"
#include "stats.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ── Minimal JSON parser (same style as the other loaders) ─────────────────────
typedef struct { const char* src; int pos; int len; } PParser;

static void p_skip_ws(PParser* p) {
    while (p->pos < p->len && isspace((unsigned char)p->src[p->pos])) p->pos++;
}
static int p_peek(PParser* p) {
    p_skip_ws(p);
    return (p->pos >= p->len) ? 0 : p->src[p->pos];
}
static int p_consume(PParser* p, char ch) {
    p_skip_ws(p);
    if (p->pos >= p->len || p->src[p->pos] != ch) return 0;
    p->pos++; return 1;
}
static int p_parse_string(PParser* p, char* buf, int buf_len) {
    p_skip_ws(p);
    if (!p_consume(p, '"')) return 0;
    int i = 0;
    while (p->pos < p->len && p->src[p->pos] != '"') {
        if (p->src[p->pos] == '\\') {
            p->pos++;
            if (p->pos >= p->len) break;
            char ec = p->src[p->pos];
            if (ec == 'n')       { if (i < buf_len-1) buf[i++] = '\n'; }
            else if (ec == '"')  { if (i < buf_len-1) buf[i++] = '"';  }
            else if (ec == '\\') { if (i < buf_len-1) buf[i++] = '\\'; }
            else                 { if (i < buf_len-1) buf[i++] = ec;   }
        } else {
            if (i < buf_len-1) buf[i++] = p->src[p->pos];
        }
        p->pos++;
    }
    buf[i] = '\0';
    p_consume(p, '"');
    return 1;
}
static int p_parse_int(PParser* p) {
    p_skip_ws(p);
    int neg = 0;
    if (p->pos < p->len && p->src[p->pos] == '-') { neg = 1; p->pos++; }
    int v = 0;
    while (p->pos < p->len && isdigit((unsigned char)p->src[p->pos]))
        v = v * 10 + (p->src[p->pos++] - '0');
    return neg ? -v : v;
}
static void p_skip_value(PParser* p) {
    p_skip_ws(p);
    if (p->pos >= p->len) return;
    char ch = p->src[p->pos];
    if (ch == '"') { char t[256]; p_parse_string(p, t, 256); }
    else if (ch == '{' || ch == '[') {
        char open = ch, close = (ch=='{') ? '}' : ']';
        p->pos++; int depth = 1;
        while (p->pos < p->len && depth > 0) {
            char c2 = p->src[p->pos];
            if (c2 == '"') { char t[256]; p_parse_string(p, t, 256); continue; }
            if (c2 == open) depth++;
            else if (c2 == close) depth--;
            p->pos++;
        }
    } else {
        while (p->pos < p->len) {
            char c2 = p->src[p->pos];
            if (c2==','||c2=='}'||c2==']'||isspace((unsigned char)c2)) break;
            p->pos++;
        }
    }
}

// ── String → enum maps ────────────────────────────────────────────────────────
static ExitDir dir_from_str(const char* s) {
    if (strcmp(s, "north") == 0) return EXIT_NORTH;
    if (strcmp(s, "south") == 0) return EXIT_SOUTH;
    if (strcmp(s, "east") == 0)  return EXIT_EAST;
    if (strcmp(s, "west") == 0)  return EXIT_WEST;
    return EXIT_NORTH;
}
static int element_from_str(const char* s) {
    if (strcmp(s, "physical") == 0) return ELEM_PHYSICAL;
    if (strcmp(s, "gun") == 0)      return ELEM_GUN;
    if (strcmp(s, "fire") == 0)     return ELEM_FIRE;
    if (strcmp(s, "ice") == 0)      return ELEM_ICE;
    if (strcmp(s, "electric") == 0) return ELEM_ELECTRIC;
    if (strcmp(s, "wind") == 0)     return ELEM_WIND;
    if (strcmp(s, "psychic") == 0)  return ELEM_PSYCHIC;
    if (strcmp(s, "nuclear") == 0)  return ELEM_NUCLEAR;
    if (strcmp(s, "bless") == 0)    return ELEM_BLESS;
    if (strcmp(s, "curse") == 0)    return ELEM_CURSE;
    return -1;
}
static int affinity_from_str(const char* s) {
    if (strcmp(s, "weak") == 0)   return AFF_WEAK;
    if (strcmp(s, "resist") == 0) return AFF_RESIST;
    if (strcmp(s, "null") == 0)   return AFF_NULL;
    if (strcmp(s, "repel") == 0)  return AFF_REPEL;
    if (strcmp(s, "absorb") == 0) return AFF_ABSORB;
    return AFF_NORMAL;
}

// Map enemy enum string -> EnemyID. Mirrors enemy_loader's table.
static int enemy_id_from_str(const char* s) {
    if (strcmp(s, "ENEMY_SHADOW_T1") == 0)      return ENEMY_SHADOW_T1;
    if (strcmp(s, "ENEMY_FLAME_DANCER") == 0)   return ENEMY_FLAME_DANCER;
    if (strcmp(s, "ENEMY_ICE_WRAITH") == 0)     return ENEMY_ICE_WRAITH;
    if (strcmp(s, "ENEMY_SHADOW_T2") == 0)      return ENEMY_SHADOW_T2;
    if (strcmp(s, "ENEMY_DARK_SPIRIT_T1") == 0) return ENEMY_DARK_SPIRIT_T1;
    if (strcmp(s, "ENEMY_STORM_HOUND") == 0)    return ENEMY_STORM_HOUND;
    if (strcmp(s, "ENEMY_CURSED_ARMOR") == 0)   return ENEMY_CURSED_ARMOR;
    if (strcmp(s, "ENEMY_SHADOW_T3") == 0)      return ENEMY_SHADOW_T3;
    if (strcmp(s, "ENEMY_DARK_SPIRIT_T2") == 0) return ENEMY_DARK_SPIRIT_T2;
    if (strcmp(s, "ENEMY_ALLURING_WISP") == 0)  return ENEMY_ALLURING_WISP;
    if (strcmp(s, "ENEMY_THORNED_SHADE") == 0)  return ENEMY_THORNED_SHADE;
    if (strcmp(s, "ENEMY_TWISTED_DRYAD") == 0)  return ENEMY_TWISTED_DRYAD;
    return ENEMY_NONE;
}

// ── Per-floor room name -> builder index table ────────────────────────────────
#define MAX_ROOM_NAMES MAX_DUNGEON_ROOMS
typedef struct {
    char names[MAX_ROOM_NAMES][64];
    int  indices[MAX_ROOM_NAMES];
    int  count;
} NameMap;

static void namemap_add(NameMap* nm, const char* name, int idx) {
    if (nm->count >= MAX_ROOM_NAMES) return;
    strncpy(nm->names[nm->count], name, 63);
    nm->names[nm->count][63] = '\0';
    nm->indices[nm->count] = idx;
    nm->count++;
}
static int namemap_get(NameMap* nm, const char* name) {
    for (int i = 0; i < nm->count; i++)
        if (strcmp(nm->names[i], name) == 0) return nm->indices[i];
    return -1;
}

// ── Boss parsing ──────────────────────────────────────────────────────────────
// Parses a boss object and returns a fully-built BossConfig.
static BossConfig parse_boss(PParser* p, EnemyRegistry* reg) {
    char name[64] = "Boss";
    int hp=100, sp=10, st=10, ma=10, en=10, ag=10, lu=10, exp=100;
    int pre_node = -1, post_node = -1;
    int skills[MAX_SKILLS_PER_COMBATANT]; int skill_n = 0;
    int aff_elem[ELEMENT_COUNT]; int aff_val[ELEMENT_COUNT]; int aff_n = 0;
    int phase2_hp_pct = 50;
    int phase2_immune = -1;
    int phase2_skills[8]; int phase2_skill_n = 0;
    int loot_item[16]; int loot_chance[16]; int loot_n = 0;
    int treasure_equip = -1;
    int treasure_item = -1, treasure_item_qty = 0;

    p_consume(p, '{');
    while (p_peek(p) != '}' && p_peek(p) != 0) {
        char key[64];
        if (!p_parse_string(p, key, 64)) break;
        p_consume(p, ':');
        if      (strcmp(key, "name") == 0) p_parse_string(p, name, 64);
        else if (strcmp(key, "hp") == 0)   hp = p_parse_int(p);
        else if (strcmp(key, "sp") == 0)   sp = p_parse_int(p);
        else if (strcmp(key, "st") == 0)   st = p_parse_int(p);
        else if (strcmp(key, "ma") == 0)   ma = p_parse_int(p);
        else if (strcmp(key, "en") == 0)   en = p_parse_int(p);
        else if (strcmp(key, "ag") == 0)   ag = p_parse_int(p);
        else if (strcmp(key, "lu") == 0)   lu = p_parse_int(p);
        else if (strcmp(key, "exp") == 0)  exp = p_parse_int(p);
        else if (strcmp(key, "pre_battle_node") == 0)  pre_node = p_parse_int(p);
        else if (strcmp(key, "post_battle_node") == 0) post_node = p_parse_int(p);
        else if (strcmp(key, "phase2_hp_pct") == 0)    phase2_hp_pct = p_parse_int(p);
        else if (strcmp(key, "phase2_immune") == 0) {
            char el[64]; p_parse_string(p, el, 64);
            phase2_immune = element_from_str(el);
        }
        else if (strcmp(key, "treasure_equip_id") == 0) treasure_equip = p_parse_int(p);
        else if (strcmp(key, "treasure_item_id") == 0)  treasure_item = p_parse_int(p);
        else if (strcmp(key, "treasure_item_qty") == 0) treasure_item_qty = p_parse_int(p);
        else if (strcmp(key, "skills") == 0) {
            p_consume(p, '[');
            while (p_peek(p) != ']' && p_peek(p) != 0) {
                int sid = p_parse_int(p);
                if (skill_n < MAX_SKILLS_PER_COMBATANT) skills[skill_n++] = sid;
                p_consume(p, ',');
            }
            p_consume(p, ']');
        }
        else if (strcmp(key, "phase2_skills") == 0) {
            p_consume(p, '[');
            while (p_peek(p) != ']' && p_peek(p) != 0) {
                int sid = p_parse_int(p);
                if (phase2_skill_n < 8) phase2_skills[phase2_skill_n++] = sid;
                p_consume(p, ',');
            }
            p_consume(p, ']');
        }
        else if (strcmp(key, "affinities") == 0) {
            p_consume(p, '[');
            while (p_peek(p) != ']' && p_peek(p) != 0) {
                p_consume(p, '{');
                char el[64]="", af[64]="";
                while (p_peek(p) != '}' && p_peek(p) != 0) {
                    char k2[64];
                    if (!p_parse_string(p, k2, 64)) break;
                    p_consume(p, ':');
                    if      (strcmp(k2, "element") == 0)  p_parse_string(p, el, 64);
                    else if (strcmp(k2, "affinity") == 0) p_parse_string(p, af, 64);
                    else p_skip_value(p);
                    p_consume(p, ',');
                }
                p_consume(p, '}');
                int ei = element_from_str(el);
                if (ei >= 0 && aff_n < ELEMENT_COUNT) {
                    aff_elem[aff_n] = ei; aff_val[aff_n] = affinity_from_str(af); aff_n++;
                }
                p_consume(p, ',');
            }
            p_consume(p, ']');
        }
        else if (strcmp(key, "loot") == 0) {
            p_consume(p, '[');
            while (p_peek(p) != ']' && p_peek(p) != 0) {
                p_consume(p, '{');
                int item=-1, chance=0;
                while (p_peek(p) != '}' && p_peek(p) != 0) {
                    char k2[64];
                    if (!p_parse_string(p, k2, 64)) break;
                    p_consume(p, ':');
                    if      (strcmp(k2, "item_id") == 0) item = p_parse_int(p);
                    else if (strcmp(k2, "chance") == 0)  chance = p_parse_int(p);
                    else p_skip_value(p);
                    p_consume(p, ',');
                }
                p_consume(p, '}');
                if (loot_n < 16) { loot_item[loot_n]=item; loot_chance[loot_n]=chance; loot_n++; }
                p_consume(p, ',');
            }
            p_consume(p, ']');
        }
        else p_skip_value(p);
        p_consume(p, ',');
    }
    p_consume(p, '}');

    // Build the BossConfig via the builder helpers (ENEMY_NONE = custom stats).
    BossConfig boss = pb_make_boss(reg, ENEMY_NONE, hp, sp, st, ma, en, ag, lu,
                                   exp, pre_node, post_node);
    strncpy(boss.enemy.stats.name, name, 63);
    boss.enemy.stats.name[63] = '\0';
    for (int i = 0; i < aff_n; i++)
        statblock_set_affinity(&boss.enemy.stats, (Element)aff_elem[i], (Affinity)aff_val[i]);
    boss.enemy.stats.skill_count = skill_n;
    for (int i = 0; i < skill_n; i++)
        boss.enemy.stats.skill_ids[i] = skills[i];
    pb_boss_set_phase2_hp_pct(&boss, phase2_hp_pct);
    if (phase2_immune >= 0) pb_boss_set_phase2_immune(&boss, phase2_immune);
    for (int i = 0; i < phase2_skill_n; i++)
        pb_boss_add_phase2_skill(&boss, phase2_skills[i]);
    for (int i = 0; i < loot_n; i++)
        pb_boss_add_loot(&boss, loot_item[i], loot_chance[i]);
    if (treasure_equip >= 0)      pb_boss_set_treasure_equip(&boss, treasure_equip);
    else if (treasure_item >= 0)  pb_boss_set_treasure_item(&boss, treasure_item, treasure_item_qty);

    return boss;
}

// ── Room parsing ──────────────────────────────────────────────────────────────
// Parses one room object and adds it via the appropriate pb_add_* call.
// Records the room's string id -> builder index in the NameMap.
static void parse_room(PParser* p, DungeonBuilder* pb, EnemyRegistry* reg, NameMap* nm) {
    char id[64] = "", type[32] = "normal", name[64] = "Room", win_flag[64] = "";
    // enemies
    int e_ids[4]; int e_counts[4]; int e_n = 0;
    // chest
    int has_chest = 0, chest_item = -1, chest_qty = 1, chest_equip = -1;
    char chest_sets_flag[64] = "";
    // boss
    int has_boss = 0;
    BossConfig boss;
    memset(&boss, 0, sizeof(BossConfig));

    p_consume(p, '{');
    while (p_peek(p) != '}' && p_peek(p) != 0) {
        char key[64];
        if (!p_parse_string(p, key, 64)) break;
        p_consume(p, ':');
        if      (strcmp(key, "id") == 0)       p_parse_string(p, id, 64);
        else if (strcmp(key, "type") == 0)     p_parse_string(p, type, 32);
        else if (strcmp(key, "name") == 0)     p_parse_string(p, name, 64);
        else if (strcmp(key, "win_flag") == 0) p_parse_string(p, win_flag, 64);
        else if (strcmp(key, "enemies") == 0) {
            p_consume(p, '[');
            while (p_peek(p) != ']' && p_peek(p) != 0) {
                p_consume(p, '{');
                char eid[64]=""; int cnt=1;
                while (p_peek(p) != '}' && p_peek(p) != 0) {
                    char k2[64];
                    if (!p_parse_string(p, k2, 64)) break;
                    p_consume(p, ':');
                    if      (strcmp(k2, "id") == 0)    p_parse_string(p, eid, 64);
                    else if (strcmp(k2, "count") == 0) cnt = p_parse_int(p);
                    else p_skip_value(p);
                    p_consume(p, ',');
                }
                p_consume(p, '}');
                if (e_n < 4) { e_ids[e_n] = enemy_id_from_str(eid); e_counts[e_n] = cnt; e_n++; }
                p_consume(p, ',');
            }
            p_consume(p, ']');
        }
        else if (strcmp(key, "chest") == 0) {
            has_chest = 1;
            p_consume(p, '{');
            while (p_peek(p) != '}' && p_peek(p) != 0) {
                char k2[64];
                if (!p_parse_string(p, k2, 64)) break;
                p_consume(p, ':');
                if      (strcmp(k2, "item_id") == 0)   chest_item = p_parse_int(p);
                else if (strcmp(k2, "item_qty") == 0)  chest_qty = p_parse_int(p);
                else if (strcmp(k2, "equip_id") == 0)  chest_equip = p_parse_int(p);
                else if (strcmp(k2, "sets_flag") == 0) p_parse_string(p, chest_sets_flag, 64);
                else p_skip_value(p);
                p_consume(p, ',');
            }
            p_consume(p, '}');
        }
        else if (strcmp(key, "boss") == 0) {
            has_boss = 1;
            boss = parse_boss(p, reg);
        }
        else p_skip_value(p);
        p_consume(p, ',');
    }
    p_consume(p, '}');

    // Dispatch to the right builder call based on type.
    int idx = -1;
    if (strcmp(type, "safe") == 0) {
        idx = pb_add_safe_room(pb, name);
    } else if (strcmp(type, "chest") == 0) {
        if (chest_equip >= 0) idx = pb_add_chest_room_equip(pb, name, chest_equip);
        else                  idx = pb_add_chest_room(pb, name, chest_item, chest_qty);
        if (idx >= 0 && chest_sets_flag[0])
            pb_set_chest_flag(pb, idx, chest_sets_flag);
    } else if (strcmp(type, "boss") == 0) {
        idx = pb_add_boss_room(pb, name, &boss);
    } else { // normal
        int e1 = (e_n > 0) ? e_ids[0] : ENEMY_NONE;
        int c1 = (e_n > 0) ? e_counts[0] : 0;
        int e2 = (e_n > 1) ? e_ids[1] : ENEMY_NONE;
        int c2 = (e_n > 1) ? e_counts[1] : 0;
        idx = pb_add_normal_room(pb, name, win_flag, e1, c1, e2, c2);
    }
    if (idx >= 0) namemap_add(nm, id, idx);
}

// ── Floor parsing ─────────────────────────────────────────────────────────────
static void parse_floor(PParser* p, DungeonBuilder* pb, EnemyRegistry* reg) {
    int floor_number = 1;
    char fname[64] = "Floor";
    char stairs_room[64] = "";
    int has_stairs = 0;
    NameMap nm; nm.count = 0;

    // We must process rooms BEFORE connections/stairs (they reference room ids),
    // but JSON key order isn't guaranteed. So we capture connection text by
    // re-scanning: simplest robust approach is two-pass within the floor object.
    // Here we rely on author order (rooms first), which our generator emits.

    p_consume(p, '{');
    while (p_peek(p) != '}' && p_peek(p) != 0) {
        char key[64];
        if (!p_parse_string(p, key, 64)) break;
        p_consume(p, ':');
        if      (strcmp(key, "floor_number") == 0) floor_number = p_parse_int(p);
        else if (strcmp(key, "name") == 0)         p_parse_string(p, fname, 64);
        else if (strcmp(key, "stairs_room") == 0) {
            if (p_peek(p) == '"') { p_parse_string(p, stairs_room, 64); has_stairs = 1; }
            else p_skip_value(p);  // null
        }
        else if (strcmp(key, "rooms") == 0) {
            pb_begin_floor(pb, floor_number, fname);
            p_consume(p, '[');
            while (p_peek(p) != ']' && p_peek(p) != 0) {
                parse_room(p, pb, reg, &nm);
                p_consume(p, ',');
            }
            p_consume(p, ']');
        }
        else if (strcmp(key, "connections") == 0) {
            p_consume(p, '[');
            while (p_peek(p) != ']' && p_peek(p) != 0) {
                p_consume(p, '{');
                char from[64]="", dir[16]="", to[64]="";
                while (p_peek(p) != '}' && p_peek(p) != 0) {
                    char k2[64];
                    if (!p_parse_string(p, k2, 64)) break;
                    p_consume(p, ':');
                    if      (strcmp(k2, "from") == 0) p_parse_string(p, from, 64);
                    else if (strcmp(k2, "dir") == 0)  p_parse_string(p, dir, 16);
                    else if (strcmp(k2, "to") == 0)   p_parse_string(p, to, 64);
                    else p_skip_value(p);
                    p_consume(p, ',');
                }
                p_consume(p, '}');
                int a = namemap_get(&nm, from);
                int b = namemap_get(&nm, to);
                if (a >= 0 && b >= 0)
                    pb_connect(pb, a, dir_from_str(dir), b);
                else
                    printf("dungeon_loader: bad connection %s -> %s\n", from, to);
                p_consume(p, ',');
            }
            p_consume(p, ']');
        }
        else p_skip_value(p);
        p_consume(p, ',');
    }
    p_consume(p, '}');

    if (has_stairs) {
        int s = namemap_get(&nm, stairs_room);
        if (s >= 0) pb_set_stairs(pb, s);
    }
    pb_end_floor(pb);
}

// ── Top-level: find and build the dungeon with matching id ─────────────────────
int dungeon_build_from_json(Dungeon* p, EnemyRegistry* reg, const char* dungeon_id) {
    FILE* f = fopen("data/dungeons.json", "r");
    if (!f) { printf("dungeon_loader: data/dungeons.json not found\n"); return 0; }
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    char* buf = malloc(size + 1);
    if (!buf) { fclose(f); return 0; }
    fread(buf, 1, size, f); buf[size] = '\0'; fclose(f);

    PParser pp = { buf, 0, (int)size };
    int built = 0;
    p_consume(&pp, '{');
    while (p_peek(&pp) != '}' && p_peek(&pp) != 0) {
        char key[64];
        p_parse_string(&pp, key, 64);
        p_consume(&pp, ':');
        if (strcmp(key, "dungeons") == 0) {
            p_consume(&pp, '[');
            while (p_peek(&pp) != ']' && p_peek(&pp) != 0) {
                // Peek the dungeon's "id" without committing: we parse the object,
                // but only run builder calls if id matches. Simplest correct way:
                // parse meta fields, and when we hit "floors", decide.
                // We do this by parsing into a temp and matching id first.
                // --- parse this one dungeon object ---
                DungeonBuilder pb;
                char this_id[64] = "";
                char name[64]="", ruler[64]="", clear_flag[64]="", entry_flag[64]="";
                int deadline = 0, team_node = -1;
                char team_flag[64] = "";
                int started = 0;

                p_consume(&pp, '{');
                while (p_peek(&pp) != '}' && p_peek(&pp) != 0) {
                    char k[64];
                    if (!p_parse_string(&pp, k, 64)) break;
                    p_consume(&pp, ':');
                    if      (strcmp(k, "id") == 0)            p_parse_string(&pp, this_id, 64);
                    else if (strcmp(k, "name") == 0)          p_parse_string(&pp, name, 64);
                    else if (strcmp(k, "ruler") == 0)         p_parse_string(&pp, ruler, 64);
                    else if (strcmp(k, "clear_flag") == 0)    p_parse_string(&pp, clear_flag, 64);
                    else if (strcmp(k, "entry_flag") == 0)    p_parse_string(&pp, entry_flag, 64);
                    else if (strcmp(k, "deadline_day") == 0)  deadline = p_parse_int(&pp);
                    else if (strcmp(k, "team_meeting_node") == 0) team_node = p_parse_int(&pp);
                    else if (strcmp(k, "team_meeting_flag") == 0) p_parse_string(&pp, team_flag, 64);
                    else if (strcmp(k, "floors") == 0) {
                        int match = (strcmp(this_id, dungeon_id) == 0);
                        if (match) {
                            pb_init(&pb, p, reg);
                            pb_set_meta(&pb, name, ruler, clear_flag, entry_flag, deadline);
                            started = 1;
                            p_consume(&pp, '[');
                            while (p_peek(&pp) != ']' && p_peek(&pp) != 0) {
                                parse_floor(&pp, &pb, reg);
                                p_consume(&pp, ',');
                            }
                            p_consume(&pp, ']');
                        } else {
                            p_skip_value(&pp);  // skip floors of non-matching dungeon
                        }
                    }
                    else p_skip_value(&pp);
                    p_consume(&pp, ',');
                }
                p_consume(&pp, '}');

                if (started) {
                    pb_set_team_meeting(&pb, team_node, team_flag);
                    pb_finish(&pb);
                    built = 1;
                }
                p_consume(&pp, ',');
                if (built) break;  // found and built our dungeon
            }
            p_consume(&pp, ']');
        } else {
            p_skip_value(&pp);
        }
        p_consume(&pp, ',');
    }
    free(buf);
    if (!built) printf("dungeon_loader: dungeon id '%s' not found\n", dungeon_id);
    return built;
}

// ── Data-driven dungeon registry ───────────────────────────────────────────────
// Reads data/dungeons.json and registers every dungeon's meta (no floors), so a
// new dungeon needs only a JSON object — no C edit. Floors are built later by
// dungeon_build_from_json(json_id) when the dungeon is entered.
int dungeon_load_registry(DungeonDB* db) {
    FILE* f = fopen("data/dungeons.json", "r");
    if (!f) return 0;
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    char* buf = malloc(size + 1);
    if (!buf) { fclose(f); return 0; }
    fread(buf, 1, size, f); buf[size] = '\0'; fclose(f);

    PParser pp = { buf, 0, (int)size };
    int n = 0;
    p_consume(&pp, '{');
    while (p_peek(&pp) != '}' && p_peek(&pp) != 0) {
        char key[64];
        p_parse_string(&pp, key, 64);
        p_consume(&pp, ':');
        if (strcmp(key, "dungeons") == 0) {
            p_consume(&pp, '[');
            while (p_peek(&pp) != ']' && p_peek(&pp) != 0) {
                char this_id[64]="", name[64]="", entry_flag[64]="", clear_flag[64]="";
                int deadline = 0;
                p_consume(&pp, '{');
                while (p_peek(&pp) != '}' && p_peek(&pp) != 0) {
                    char k[64];
                    if (!p_parse_string(&pp, k, 64)) break;
                    p_consume(&pp, ':');
                    if      (strcmp(k, "id") == 0)           p_parse_string(&pp, this_id, 64);
                    else if (strcmp(k, "name") == 0)         p_parse_string(&pp, name, 64);
                    else if (strcmp(k, "entry_flag") == 0)   p_parse_string(&pp, entry_flag, 64);
                    else if (strcmp(k, "clear_flag") == 0)   p_parse_string(&pp, clear_flag, 64);
                    else if (strcmp(k, "deadline_day") == 0) deadline = p_parse_int(&pp);
                    else p_skip_value(&pp);
                    p_consume(&pp, ',');
                }
                p_consume(&pp, '}');
                if (this_id[0] && db->count < MAX_DUNGEONS) {
                    DungeonEntry* e = &db->entries[db->count++];
                    memset(e, 0, sizeof(*e));
                    int num = 0;
                    for (const char* c = this_id; *c; ++c)
                        if (*c >= '0' && *c <= '9') num = num * 10 + (*c - '0');
                    e->id = (num > 0) ? num : db->count;
                    strncpy(e->json_id,    this_id,    63);
                    strncpy(e->name,       name,       63);
                    strncpy(e->entry_flag, entry_flag, 63);
                    strncpy(e->clear_flag, clear_flag, 63);
                    e->deadline_day = deadline;
                    e->builder = 0;   // build via json_id
                    n++;
                }
                p_consume(&pp, ',');
            }
            p_consume(&pp, ']');
        } else {
            p_skip_value(&pp);
        }
        p_consume(&pp, ',');
    }
    free(buf);
    return n;
}