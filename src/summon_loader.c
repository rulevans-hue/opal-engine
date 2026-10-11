#include "summon_loader.h"
#include "stats.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ── Minimal JSON parser ───────────────────────────────────────────────────────
typedef struct { const char* src; int pos; int len; } RParser;

static void r_skip_ws(RParser* p) {
    while (p->pos < p->len && isspace((unsigned char)p->src[p->pos])) p->pos++;
}
static int r_peek(RParser* p) {
    r_skip_ws(p);
    return (p->pos >= p->len) ? 0 : p->src[p->pos];
}
static int r_consume(RParser* p, char ch) {
    r_skip_ws(p);
    if (p->pos >= p->len || p->src[p->pos] != ch) return 0;
    p->pos++; return 1;
}
static int r_parse_string(RParser* p, char* buf, int buf_len) {
    r_skip_ws(p);
    if (!r_consume(p, '"')) return 0;
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
    r_consume(p, '"');
    return 1;
}
static int r_parse_int(RParser* p) {
    r_skip_ws(p);
    int neg = 0;
    if (p->pos < p->len && p->src[p->pos] == '-') { neg = 1; p->pos++; }
    int v = 0;
    while (p->pos < p->len && isdigit((unsigned char)p->src[p->pos]))
        v = v * 10 + (p->src[p->pos++] - '0');
    return neg ? -v : v;
}
static void r_skip_value(RParser* p) {
    r_skip_ws(p);
    if (p->pos >= p->len) return;
    char ch = p->src[p->pos];
    if (ch == '"') { char t[256]; r_parse_string(p, t, 256); }
    else if (ch == '{' || ch == '[') {
        char open = ch, close = (ch=='{') ? '}' : ']';
        p->pos++; int depth = 1;
        while (p->pos < p->len && depth > 0) {
            char c2 = p->src[p->pos];
            if (c2 == '"') { char t[256]; r_parse_string(p, t, 256); continue; }
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
static Affinity affinity_from_str(const char* s) {
    if (strcmp(s, "weak") == 0)   return AFF_WEAK;
    if (strcmp(s, "resist") == 0) return AFF_RESIST;
    if (strcmp(s, "null") == 0)   return AFF_NULL;
    if (strcmp(s, "repel") == 0)  return AFF_REPEL;
    if (strcmp(s, "absorb") == 0) return AFF_ABSORB;
    return AFF_NORMAL;
}
static Arcana arcana_from_str(const char* s) {
    if (strcmp(s, "fool") == 0)       return ARCANA_FOOL;
    if (strcmp(s, "magician") == 0)   return ARCANA_MAGICIAN;
    if (strcmp(s, "priestess") == 0)  return ARCANA_PRIESTESS;
    if (strcmp(s, "empress") == 0)    return ARCANA_EMPRESS;
    if (strcmp(s, "emperor") == 0)    return ARCANA_EMPEROR;
    if (strcmp(s, "hierophant") == 0) return ARCANA_HIEROPHANT;
    if (strcmp(s, "lovers") == 0)     return ARCANA_LOVERS;
    if (strcmp(s, "chariot") == 0)    return ARCANA_CHARIOT;
    if (strcmp(s, "justice") == 0)    return ARCANA_JUSTICE;
    if (strcmp(s, "hermit") == 0)     return ARCANA_HERMIT;
    if (strcmp(s, "wheel") == 0)      return ARCANA_WHEEL;
    if (strcmp(s, "strength") == 0)   return ARCANA_STRENGTH;
    if (strcmp(s, "hangedman") == 0)  return ARCANA_HANGEDMAN;
    if (strcmp(s, "death") == 0)      return ARCANA_DEATH;
    if (strcmp(s, "temperance") == 0) return ARCANA_TEMPERANCE;
    if (strcmp(s, "devil") == 0)      return ARCANA_DEVIL;
    if (strcmp(s, "tower") == 0)      return ARCANA_TOWER;
    if (strcmp(s, "star") == 0)       return ARCANA_STAR;
    if (strcmp(s, "moon") == 0)       return ARCANA_MOON;
    if (strcmp(s, "sun") == 0)        return ARCANA_SUN;
    if (strcmp(s, "judgement") == 0)  return ARCANA_JUDGEMENT;
    return ARCANA_FOOL;
}

static void parse_summon(RParser* p, SummonDB* db) {
    if (db->count >= MAX_SUMMON_IN_DB) { r_skip_value(p); return; }
    Summon per;
    memset(&per, 0, sizeof(Summon));
    per.valid = 1;
    for (int i = 0; i < ELEMENT_COUNT; i++) per.affinities[i] = AFF_NORMAL;
    char arc_s[32] = "fool";

    r_consume(p, '{');
    while (r_peek(p) != '}' && r_peek(p) != 0) {
        char key[64];
        if (!r_parse_string(p, key, 64)) break;
        r_consume(p, ':');
        if      (strcmp(key, "name") == 0)       r_parse_string(p, per.name, 64);
        else if (strcmp(key, "arcana") == 0)     r_parse_string(p, arc_s, 32);
        else if (strcmp(key, "base_level") == 0) { per.base_level = r_parse_int(p); per.level = per.base_level; }
        else if (strcmp(key, "ma_mod") == 0)     per.ma_mod = r_parse_int(p);
        else if (strcmp(key, "ag_mod") == 0)     per.ag_mod = r_parse_int(p);
        else if (strcmp(key, "affinities") == 0) {
            r_consume(p, '[');
            while (r_peek(p) != ']' && r_peek(p) != 0) {
                r_consume(p, '{');
                char el[32]="", af[32]="";
                while (r_peek(p) != '}' && r_peek(p) != 0) {
                    char k2[64];
                    if (!r_parse_string(p, k2, 64)) break;
                    r_consume(p, ':');
                    if      (strcmp(k2, "element") == 0)  r_parse_string(p, el, 32);
                    else if (strcmp(k2, "affinity") == 0) r_parse_string(p, af, 32);
                    else r_skip_value(p);
                    r_consume(p, ',');
                }
                r_consume(p, '}');
                int ei = element_from_str(el);
                if (ei >= 0) per.affinities[ei] = affinity_from_str(af);
                r_consume(p, ',');
            }
            r_consume(p, ']');
        }
        else if (strcmp(key, "skills") == 0) {
            r_consume(p, '[');
            while (r_peek(p) != ']' && r_peek(p) != 0) {
                r_consume(p, '{');
                int sid = 0, lvl = 1;
                while (r_peek(p) != '}' && r_peek(p) != 0) {
                    char k2[64];
                    if (!r_parse_string(p, k2, 64)) break;
                    r_consume(p, ':');
                    if      (strcmp(k2, "skill") == 0)       sid = r_parse_int(p);
                    else if (strcmp(k2, "learn_level") == 0) lvl = r_parse_int(p);
                    else r_skip_value(p);
                    r_consume(p, ',');
                }
                r_consume(p, '}');
                if (per.skill_count < MAX_SUMMON_SKILLS) {
                    per.skill_ids[per.skill_count]    = sid;
                    per.learn_levels[per.skill_count] = lvl;
                    per.skill_count++;
                }
                r_consume(p, ',');
            }
            r_consume(p, ']');
        }
        else r_skip_value(p);
        r_consume(p, ',');
    }
    r_consume(p, '}');

    per.arcana = arcana_from_str(arc_s);
    per.exp = 0;
    db->entries[db->count++] = per;
}

int summon_load_json(SummonDB* db, const char* filepath) {
    FILE* f = fopen(filepath, "r");
    if (!f) { printf("summon_loader: %s not found\n", filepath); return 0; }
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    char* buf = malloc(size + 1);
    if (!buf) { fclose(f); return 0; }
    fread(buf, 1, size, f); buf[size] = '\0'; fclose(f);

    db->count = 0;
    RParser p = { buf, 0, (int)size };
    r_consume(&p, '{');
    while (r_peek(&p) != '}' && r_peek(&p) != 0) {
        char key[64];
        r_parse_string(&p, key, 64);
        r_consume(&p, ':');
        if (strcmp(key, "summons") == 0) {
            r_consume(&p, '[');
            while (r_peek(&p) != ']' && r_peek(&p) != 0) {
                parse_summon(&p, db);
                r_consume(&p, ',');
            }
            r_consume(&p, ']');
        } else {
            r_skip_value(&p);
        }
        r_consume(&p, ',');
    }
    free(buf);
    printf("summon_loader: loaded %d summons from %s\n", db->count, filepath);
    return db->count > 0;
}