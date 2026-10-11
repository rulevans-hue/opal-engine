#include "skill_loader.h"
#include "stats.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ── Minimal JSON parser ───────────────────────────────────────────────────────
typedef struct { const char* src; int pos; int len; } SParser;

static void s_skip_ws(SParser* p) {
    while (p->pos < p->len && isspace((unsigned char)p->src[p->pos])) p->pos++;
}
static int s_peek(SParser* p) {
    s_skip_ws(p);
    return (p->pos >= p->len) ? 0 : p->src[p->pos];
}
static int s_consume(SParser* p, char ch) {
    s_skip_ws(p);
    if (p->pos >= p->len || p->src[p->pos] != ch) return 0;
    p->pos++; return 1;
}
static int s_parse_string(SParser* p, char* buf, int buf_len) {
    s_skip_ws(p);
    if (!s_consume(p, '"')) return 0;
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
    s_consume(p, '"');
    return 1;
}
static int s_parse_int(SParser* p) {
    s_skip_ws(p);
    int neg = 0;
    if (p->pos < p->len && p->src[p->pos] == '-') { neg = 1; p->pos++; }
    int v = 0;
    while (p->pos < p->len && isdigit((unsigned char)p->src[p->pos]))
        v = v * 10 + (p->src[p->pos++] - '0');
    return neg ? -v : v;
}
// Parse a float (handles "0.4", "1", "0")
static float s_parse_float(SParser* p) {
    s_skip_ws(p);
    char buf[32]; int i = 0;
    if (p->pos < p->len && p->src[p->pos] == '-') { if (i<31) buf[i++]=p->src[p->pos]; p->pos++; }
    while (p->pos < p->len &&
           (isdigit((unsigned char)p->src[p->pos]) || p->src[p->pos]=='.')) {
        if (i < 31) buf[i++] = p->src[p->pos];
        p->pos++;
    }
    buf[i] = '\0';
    return (float)atof(buf);
}
static void s_skip_value(SParser* p) {
    s_skip_ws(p);
    if (p->pos >= p->len) return;
    char ch = p->src[p->pos];
    if (ch == '"') { char t[256]; s_parse_string(p, t, 256); }
    else if (ch == '{' || ch == '[') {
        char open = ch, close = (ch=='{') ? '}' : ']';
        p->pos++; int depth = 1;
        while (p->pos < p->len && depth > 0) {
            char c2 = p->src[p->pos];
            if (c2 == '"') { char t[256]; s_parse_string(p, t, 256); continue; }
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
    return ELEM_PHYSICAL;
}
static SkillTarget target_from_str(const char* s) {
    if (strcmp(s, "one_enemy") == 0)   return TARGET_ONE_ENEMY;
    if (strcmp(s, "all_enemies") == 0) return TARGET_ALL_ENEMIES;
    if (strcmp(s, "one_ally") == 0)    return TARGET_ONE_ALLY;
    if (strcmp(s, "all_allies") == 0)  return TARGET_ALL_ALLIES;
    if (strcmp(s, "self") == 0)        return TARGET_SELF;
    return TARGET_ONE_ENEMY;
}
static StatusEffect status_from_str(const char* s) {
    if (strcmp(s, "burn") == 0)      return STATUS_BURN;
    if (strcmp(s, "freeze") == 0)    return STATUS_FREEZE;
    if (strcmp(s, "shock") == 0)     return STATUS_SHOCK;
    if (strcmp(s, "dizzy") == 0)     return STATUS_DIZZY;
    if (strcmp(s, "sleep") == 0)     return STATUS_SLEEP;
    if (strcmp(s, "confuse") == 0)   return STATUS_CONFUSE;
    if (strcmp(s, "fear") == 0)      return STATUS_FEAR;
    if (strcmp(s, "despair") == 0)   return STATUS_DESPAIR;
    if (strcmp(s, "brainwash") == 0) return STATUS_BRAINWASH;
    if (strcmp(s, "forget") == 0)    return STATUS_FORGET;
    return STATUS_NONE;
}

static void parse_skill(SParser* p, SkillDB* db) {
    Skill sk;
    memset(&sk, 0, sizeof(Skill));
    char elem_s[32]="physical", tgt_s[32]="one_enemy", infl_s[32]="none";

    s_consume(p, '{');
    while (s_peek(p) != '}' && s_peek(p) != 0) {
        char key[64];
        if (!s_parse_string(p, key, 64)) break;
        s_consume(p, ':');
        if      (strcmp(key, "id") == 0)            sk.id = s_parse_int(p);
        else if (strcmp(key, "name") == 0)          s_parse_string(p, sk.name, 64);
        else if (strcmp(key, "description") == 0)   s_parse_string(p, sk.description, 128);
        else if (strcmp(key, "element") == 0)       s_parse_string(p, elem_s, 32);
        else if (strcmp(key, "sp_cost") == 0)       sk.sp_cost = s_parse_int(p);
        else if (strcmp(key, "power") == 0)         sk.power = s_parse_int(p);
        else if (strcmp(key, "target") == 0)        s_parse_string(p, tgt_s, 32);
        else if (strcmp(key, "inflicts") == 0)      s_parse_string(p, infl_s, 32);
        else if (strcmp(key, "status_chance") == 0) sk.status_chance = s_parse_float(p);
        else if (strcmp(key, "is_heal") == 0)       sk.is_heal = s_parse_int(p);
        else if (strcmp(key, "is_buff") == 0)       sk.is_buff = s_parse_int(p);
        else if (strcmp(key, "stat_buff") == 0)     sk.stat_buff = s_parse_int(p);
        else if (strcmp(key, "buff_amount") == 0)   sk.buff_amount = s_parse_int(p);
        else s_skip_value(p);
        s_consume(p, ',');
    }
    s_consume(p, '}');

    sk.element  = (Element)element_from_str(elem_s);
    sk.target   = target_from_str(tgt_s);
    sk.inflicts = status_from_str(infl_s);
    skilldb_add(db, sk);
}

int skill_load_json(SkillDB* db, const char* filepath) {
    FILE* f = fopen(filepath, "r");
    if (!f) { printf("skill_loader: %s not found\n", filepath); return 0; }
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    char* buf = malloc(size + 1);
    if (!buf) { fclose(f); return 0; }
    fread(buf, 1, size, f); buf[size] = '\0'; fclose(f);

    db->count = 0;
    SParser p = { buf, 0, (int)size };
    s_consume(&p, '{');
    while (s_peek(&p) != '}' && s_peek(&p) != 0) {
        char key[64];
        s_parse_string(&p, key, 64);
        s_consume(&p, ':');
        if (strcmp(key, "skills") == 0) {
            s_consume(&p, '[');
            while (s_peek(&p) != ']' && s_peek(&p) != 0) {
                parse_skill(&p, db);
                s_consume(&p, ',');
            }
            s_consume(&p, ']');
        } else {
            s_skip_value(&p);
        }
        s_consume(&p, ',');
    }
    free(buf);
    printf("skill_loader: loaded %d skills from %s\n", db->count, filepath);
    return db->count > 0;
}