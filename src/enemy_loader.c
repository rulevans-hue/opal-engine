#include "enemy_loader.h"
#include "stats.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ── Minimal JSON parser (same style as bond_loader.c) ────────────────────
typedef struct { const char* src; int pos; int len; } EParser;

static void e_skip_ws(EParser* p) {
    while (p->pos < p->len && isspace((unsigned char)p->src[p->pos])) p->pos++;
}
static int e_peek(EParser* p) {
    e_skip_ws(p);
    return (p->pos >= p->len) ? 0 : p->src[p->pos];
}
static int e_consume(EParser* p, char ch) {
    e_skip_ws(p);
    if (p->pos >= p->len || p->src[p->pos] != ch) return 0;
    p->pos++; return 1;
}
static int e_parse_string(EParser* p, char* buf, int buf_len) {
    e_skip_ws(p);
    if (!e_consume(p, '"')) return 0;
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
    e_consume(p, '"');
    return 1;
}
static int e_parse_int(EParser* p) {
    e_skip_ws(p);
    int neg = 0;
    if (p->pos < p->len && p->src[p->pos] == '-') { neg = 1; p->pos++; }
    int v = 0;
    while (p->pos < p->len && isdigit((unsigned char)p->src[p->pos]))
        v = v * 10 + (p->src[p->pos++] - '0');
    return neg ? -v : v;
}
static void e_skip_value(EParser* p) {
    e_skip_ws(p);
    if (p->pos >= p->len) return;
    char ch = p->src[p->pos];
    if (ch == '"') { char t[256]; e_parse_string(p, t, 256); }
    else if (ch == '{' || ch == '[') {
        char open = ch, close = (ch=='{') ? '}' : ']';
        p->pos++; int depth = 1;
        while (p->pos < p->len && depth > 0) {
            char c2 = p->src[p->pos];
            if (c2 == '"') { char t[256]; e_parse_string(p, t, 256); continue; }
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
    if (strcmp(s, "normal") == 0) return AFF_NORMAL;
    if (strcmp(s, "weak") == 0)   return AFF_WEAK;
    if (strcmp(s, "resist") == 0) return AFF_RESIST;
    if (strcmp(s, "null") == 0)   return AFF_NULL;
    if (strcmp(s, "repel") == 0)  return AFF_REPEL;
    if (strcmp(s, "absorb") == 0) return AFF_ABSORB;
    return AFF_NORMAL;
}

// Maps the JSON "id" string to an EnemyID enum slot.
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
    return -1;
}

// ── Parse one enemy object into a template slot ───────────────────────────────
static void parse_enemy(EParser* p, EnemyRegistry* reg) {
    char id_str[64] = "";
    char name[64]   = "Enemy";
    int hp=1, sp=0, st=1, ma=1, en=1, ag=1, lu=1, exp=0;

    // temp storage for affinities/skills/loot until we know the slot
    int aff_elem[ELEMENT_COUNT]; int aff_val[ELEMENT_COUNT]; int aff_n = 0;
    int skills[MAX_SKILLS_PER_COMBATANT]; int skill_n = 0;
    int loot_item[16]; int loot_chance[16]; int loot_n = 0;

    e_consume(p, '{');
    while (e_peek(p) != '}' && e_peek(p) != 0) {
        char key[64];
        if (!e_parse_string(p, key, 64)) break;
        e_consume(p, ':');
        if      (strcmp(key, "id") == 0)   e_parse_string(p, id_str, 64);
        else if (strcmp(key, "name") == 0) e_parse_string(p, name, 64);
        else if (strcmp(key, "hp") == 0)   hp = e_parse_int(p);
        else if (strcmp(key, "sp") == 0)   sp = e_parse_int(p);
        else if (strcmp(key, "st") == 0)   st = e_parse_int(p);
        else if (strcmp(key, "ma") == 0)   ma = e_parse_int(p);
        else if (strcmp(key, "en") == 0)   en = e_parse_int(p);
        else if (strcmp(key, "ag") == 0)   ag = e_parse_int(p);
        else if (strcmp(key, "lu") == 0)   lu = e_parse_int(p);
        else if (strcmp(key, "exp") == 0)  exp = e_parse_int(p);
        else if (strcmp(key, "affinities") == 0) {
            e_consume(p, '[');
            while (e_peek(p) != ']' && e_peek(p) != 0) {
                e_consume(p, '{');
                char el[64] = "", af[64] = "";
                while (e_peek(p) != '}' && e_peek(p) != 0) {
                    char k2[64];
                    if (!e_parse_string(p, k2, 64)) break;
                    e_consume(p, ':');
                    if      (strcmp(k2, "element") == 0)  e_parse_string(p, el, 64);
                    else if (strcmp(k2, "affinity") == 0) e_parse_string(p, af, 64);
                    else e_skip_value(p);
                    e_consume(p, ',');
                }
                e_consume(p, '}');
                int ei = element_from_str(el);
                if (ei >= 0 && aff_n < ELEMENT_COUNT) {
                    aff_elem[aff_n] = ei;
                    aff_val[aff_n]  = affinity_from_str(af);
                    aff_n++;
                }
                e_consume(p, ',');
            }
            e_consume(p, ']');
        }
        else if (strcmp(key, "skills") == 0) {
            e_consume(p, '[');
            while (e_peek(p) != ']' && e_peek(p) != 0) {
                int sid = e_parse_int(p);
                if (skill_n < MAX_SKILLS_PER_COMBATANT) skills[skill_n++] = sid;
                e_consume(p, ',');
            }
            e_consume(p, ']');
        }
        else if (strcmp(key, "loot") == 0) {
            e_consume(p, '[');
            while (e_peek(p) != ']' && e_peek(p) != 0) {
                e_consume(p, '{');
                int item = -1, chance = 0;
                while (e_peek(p) != '}' && e_peek(p) != 0) {
                    char k2[64];
                    if (!e_parse_string(p, k2, 64)) break;
                    e_consume(p, ':');
                    if      (strcmp(k2, "item_id") == 0) item = e_parse_int(p);
                    else if (strcmp(k2, "chance") == 0)  chance = e_parse_int(p);
                    else e_skip_value(p);
                    e_consume(p, ',');
                }
                e_consume(p, '}');
                if (loot_n < 16) { loot_item[loot_n] = item; loot_chance[loot_n] = chance; loot_n++; }
                e_consume(p, ',');
            }
            e_consume(p, ']');
        }
        else e_skip_value(p);
        e_consume(p, ',');
    }
    e_consume(p, '}');

    int slot = enemy_id_from_str(id_str);
    if (slot < 0 || slot >= ENEMY_REGISTRY_MAX) {
        printf("enemy_loader: unknown enemy id '%s' (skipped)\n", id_str);
        return;
    }

    EnemyTemplate* t = &reg->templates[slot];
    *t = make_enemy_template(name, hp, sp, st, ma, en, ag, lu, exp);
    for (int i = 0; i < aff_n; i++)
        enemy_template_set_affinity(t, (Element)aff_elem[i], (Affinity)aff_val[i]);
    t->skill_count = skill_n;
    for (int i = 0; i < skill_n; i++)
        t->skill_ids[i] = skills[i];
    for (int i = 0; i < loot_n; i++)
        enemy_template_add_loot(t, loot_item[i], loot_chance[i]);
}

int enemy_load_json(EnemyRegistry* reg, const char* filepath) {
    FILE* f = fopen(filepath, "r");
    if (!f) { printf("enemy_loader: %s not found\n", filepath); return 0; }
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    char* buf = malloc(size + 1);
    if (!buf) { fclose(f); return 0; }
    fread(buf, 1, size, f); buf[size] = '\0'; fclose(f);

    int loaded = 0;
    EParser p = { buf, 0, (int)size };
    e_consume(&p, '{');
    while (e_peek(&p) != '}' && e_peek(&p) != 0) {
        char key[64];
        e_parse_string(&p, key, 64);
        e_consume(&p, ':');
        if (strcmp(key, "enemies") == 0) {
            e_consume(&p, '[');
            while (e_peek(&p) != ']' && e_peek(&p) != 0) {
                parse_enemy(&p, reg);
                loaded++;
                e_consume(&p, ',');
            }
            e_consume(&p, ']');
        } else {
            e_skip_value(&p);
        }
        e_consume(&p, ',');
    }
    free(buf);
    reg->count = ENEMY_COUNT;
    printf("enemy_loader: loaded %d enemies from %s\n", loaded, filepath);
    return loaded > 0;
}