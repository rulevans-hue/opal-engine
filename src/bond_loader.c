#include "bond_loader.h"
#include "social_stats.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ── Minimal JSON parser (same style as npc.c / quest.c) ───────────────────────
typedef struct { const char* src; int pos; int len; } CParser;

static void c_skip_ws(CParser* p) {
    while (p->pos < p->len && isspace((unsigned char)p->src[p->pos])) p->pos++;
}
static int c_peek(CParser* p) {
    c_skip_ws(p);
    return (p->pos >= p->len) ? 0 : p->src[p->pos];
}
static int c_consume(CParser* p, char ch) {
    c_skip_ws(p);
    if (p->pos >= p->len || p->src[p->pos] != ch) return 0;
    p->pos++; return 1;
}
static int c_parse_string(CParser* p, char* buf, int buf_len) {
    c_skip_ws(p);
    if (!c_consume(p, '"')) return 0;
    int i = 0;
    while (p->pos < p->len && p->src[p->pos] != '"') {
        if (p->src[p->pos] == '\\') {
            p->pos++;
            if (p->pos >= p->len) break;
            char e = p->src[p->pos];
            if (e == 'n')       { if (i < buf_len-1) buf[i++] = '\n'; }
            else if (e == '"')  { if (i < buf_len-1) buf[i++] = '"';  }
            else if (e == '\\') { if (i < buf_len-1) buf[i++] = '\\'; }
            else                { if (i < buf_len-1) buf[i++] = e;    }
        } else {
            if (i < buf_len-1) buf[i++] = p->src[p->pos];
        }
        p->pos++;
    }
    buf[i] = '\0';
    c_consume(p, '"');
    return 1;
}
static int c_parse_int(CParser* p) {
    c_skip_ws(p);
    int neg = 0;
    if (p->pos < p->len && p->src[p->pos] == '-') { neg = 1; p->pos++; }
    int v = 0;
    while (p->pos < p->len && isdigit((unsigned char)p->src[p->pos]))
        v = v * 10 + (p->src[p->pos++] - '0');
    return neg ? -v : v;
}
static void c_skip_value(CParser* p) {
    c_skip_ws(p);
    if (p->pos >= p->len) return;
    char ch = p->src[p->pos];
    if (ch == '"') { char t[256]; c_parse_string(p, t, 256); }
    else if (ch == '{' || ch == '[') {
        char open = ch, close = (ch=='{') ? '}' : ']';
        p->pos++; int depth = 1;
        while (p->pos < p->len && depth > 0) {
            char c2 = p->src[p->pos];
            if (c2 == '"') { char t[256]; c_parse_string(p, t, 256); continue; }
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
static PerkType perk_from_str(const char* s) {
    if (strcmp(s, "shop_discount")     == 0) return PERK_SHOP_DISCOUNT;
    if (strcmp(s, "exp_bonus")         == 0) return PERK_EXP_BONUS;
    if (strcmp(s, "flee_bonus")        == 0) return PERK_FLEE_BONUS;
    if (strcmp(s, "followup_attack")   == 0) return PERK_FOLLOWUP_ATTACK;
    if (strcmp(s, "steal_chance")      == 0) return PERK_STEAL_CHANCE;
    if (strcmp(s, "status_duration")   == 0) return PERK_STATUS_DURATION;
    if (strcmp(s, "technical_bonus")   == 0) return PERK_TECHNICAL_BONUS;
    if (strcmp(s, "status_no_sp")      == 0) return PERK_STATUS_NO_SP;
    if (strcmp(s, "status_always_hit") == 0) return PERK_STATUS_ALWAYS_HIT;
    if (strcmp(s, "gun_ignore_resist") == 0) return PERK_GUN_IGNORE_RESIST;
    if (strcmp(s, "extra_turn_chance") == 0) return PERK_EXTRA_TURN_CHANCE;
    if (strcmp(s, "unlock_items")      == 0) return PERK_UNLOCK_ITEMS;
    if (strcmp(s, "item_potency")      == 0) return PERK_ITEM_POTENCY;
    return PERK_NONE;
}

static int stat_from_str(const char* s) {
    if (strcmp(s, "knowledge")   == 0) return SOCIAL_KNOWLEDGE;
    if (strcmp(s, "charm")       == 0) return SOCIAL_CHARM;
    if (strcmp(s, "guts")        == 0) return SOCIAL_GUTS;
    if (strcmp(s, "kindness")    == 0) return SOCIAL_KINDNESS;
    if (strcmp(s, "proficiency") == 0) return SOCIAL_PROFICIENCY;
    return -1;  // "none" or unknown
}

// ── Parse one rank object ─────────────────────────────────────────────────────
static void parse_rank(CParser* p, BondRank* r) {
    memset(r, 0, sizeof(BondRank));
    r->req_stat_type = -1;
    if (!c_consume(p, '{')) return;
    char perk_str[64] = "none";
    char stat_str[64] = "none";
    while (c_peek(p) != '}' && c_peek(p) != 0) {
        char key[64];
        if (!c_parse_string(p, key, 64)) break;
        c_consume(p, ':');
        if      (strcmp(key, "points_required") == 0) r->points_required = c_parse_int(p);
        else if (strcmp(key, "perk") == 0)            c_parse_string(p, perk_str, 64);
        else if (strcmp(key, "perk_value") == 0)      r->perk.value = c_parse_int(p);
        else if (strcmp(key, "perk_desc") == 0)       c_parse_string(p, r->perk.description, 128);
        else if (strcmp(key, "rank_up_message") == 0) c_parse_string(p, r->rank_up_message, 128);
        else if (strcmp(key, "req_stat") == 0)        c_parse_string(p, stat_str, 64);
        else if (strcmp(key, "req_stat_rank") == 0)   r->req_stat_rank = c_parse_int(p);
        else c_skip_value(p);
        c_consume(p, ',');
    }
    c_consume(p, '}');
    r->perk.type     = perk_from_str(perk_str);
    r->req_stat_type = stat_from_str(stat_str);
}

// ── Parse one bond object ────────────────────────────────────────────────
static int parse_bond(CParser* p, Bond* c) {
    memset(c, 0, sizeof(Bond));
    if (!c_consume(p, '{')) return 0;
    c->current_rank = 0;
    c->points       = 0;
    c->max_rank     = 10;
    c->points_per_hangout = 20;
    c->active       = 1;

    while (c_peek(p) != '}' && c_peek(p) != 0) {
        char key[64];
        if (!c_parse_string(p, key, 64)) break;
        c_consume(p, ':');
        if      (strcmp(key, "name") == 0)               c_parse_string(p, c->name, BOND_NAME_LEN);
        else if (strcmp(key, "arcana") == 0)             c_parse_string(p, c->arcana, BOND_NAME_LEN);
        else if (strcmp(key, "description") == 0)        c_parse_string(p, c->description, 256);
        else if (strcmp(key, "npc_name_match") == 0)     c_parse_string(p, c->npc_name_match, BOND_NAME_LEN);
        else if (strcmp(key, "max_rank") == 0)           c->max_rank = c_parse_int(p);
        else if (strcmp(key, "points_per_hangout") == 0) c->points_per_hangout = c_parse_int(p);
        else if (strcmp(key, "ranks") == 0) {
            c_consume(p, '[');
            int rank_idx = 0;
            while (c_peek(p) != ']' && c_peek(p) != 0) {
                if (rank_idx < MAX_BOND_RANKS) {
                    parse_rank(p, &c->ranks[rank_idx]);
                    rank_idx++;
                } else {
                    BondRank throwaway;
                    parse_rank(p, &throwaway);
                }
                c_consume(p, ',');
            }
            c_consume(p, ']');
        }
        else c_skip_value(p);
        c_consume(p, ',');
    }
    c_consume(p, '}');
    return (c->name[0] != '\0');
}

int bond_load_json(BondDB* db, const char* filepath) {
    FILE* f = fopen(filepath, "r");
    if (!f) { printf("bond_loader: %s not found\n", filepath); return 0; }
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    char* buf = malloc(size + 1);
    if (!buf) { fclose(f); return 0; }
    fread(buf, 1, size, f); buf[size] = '\0'; fclose(f);

    db->count = 0;
    CParser p = { buf, 0, (int)size };
    c_consume(&p, '{');
    while (c_peek(&p) != '}' && c_peek(&p) != 0) {
        char key[64];
        c_parse_string(&p, key, 64);
        c_consume(&p, ':');
        if (strcmp(key, "bonds") == 0) {
            c_consume(&p, '[');
            while (c_peek(&p) != ']' && c_peek(&p) != 0) {
                if (db->count < MAX_BONDS) {
                    if (parse_bond(&p, &db->bonds[db->count]))
                        db->count++;
                } else {
                    Bond throwaway;
                    parse_bond(&p, &throwaway);
                }
                c_consume(&p, ',');
            }
            c_consume(&p, ']');
        } else {
            c_skip_value(&p);
        }
        c_consume(&p, ',');
    }
    free(buf);
    printf("bond_loader: loaded %d bonds from %s\n", db->count, filepath);
    return db->count > 0;
}