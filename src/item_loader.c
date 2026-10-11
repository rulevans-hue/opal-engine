#include "item_loader.h"
#include "stats.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ── Minimal JSON parser ───────────────────────────────────────────────────────
typedef struct { const char* src; int pos; int len; } IParser;

static void i_skip_ws(IParser* p) {
    while (p->pos < p->len && isspace((unsigned char)p->src[p->pos])) p->pos++;
}
static int i_peek(IParser* p) {
    i_skip_ws(p);
    return (p->pos >= p->len) ? 0 : p->src[p->pos];
}
static int i_consume(IParser* p, char ch) {
    i_skip_ws(p);
    if (p->pos >= p->len || p->src[p->pos] != ch) return 0;
    p->pos++; return 1;
}
static int i_parse_string(IParser* p, char* buf, int buf_len) {
    i_skip_ws(p);
    if (!i_consume(p, '"')) return 0;
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
    i_consume(p, '"');
    return 1;
}
static int i_parse_int(IParser* p) {
    i_skip_ws(p);
    int neg = 0;
    if (p->pos < p->len && p->src[p->pos] == '-') { neg = 1; p->pos++; }
    int v = 0;
    while (p->pos < p->len && isdigit((unsigned char)p->src[p->pos]))
        v = v * 10 + (p->src[p->pos++] - '0');
    return neg ? -v : v;
}
static void i_skip_value(IParser* p) {
    i_skip_ws(p);
    if (p->pos >= p->len) return;
    char ch = p->src[p->pos];
    if (ch == '"') { char t[256]; i_parse_string(p, t, 256); }
    else if (ch == '{' || ch == '[') {
        char open = ch, close = (ch=='{') ? '}' : ']';
        p->pos++; int depth = 1;
        while (p->pos < p->len && depth > 0) {
            char c2 = p->src[p->pos];
            if (c2 == '"') { char t[256]; i_parse_string(p, t, 256); continue; }
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
static ItemType item_type_from_str(const char* s) {
    if (strcmp(s, "healing") == 0)          return ITEM_HEALING;
    if (strcmp(s, "sp") == 0)               return ITEM_SP;
    if (strcmp(s, "revival") == 0)          return ITEM_REVIVAL;
    if (strcmp(s, "status_cure") == 0)      return ITEM_STATUS_CURE;
    if (strcmp(s, "battle") == 0)           return ITEM_BATTLE;
    if (strcmp(s, "full_heal") == 0)        return ITEM_FULL_HEAL;
    if (strcmp(s, "full_sp") == 0)          return ITEM_FULL_SP;
    if (strcmp(s, "full_heal_all") == 0)    return ITEM_FULL_HEAL_ALL;
    if (strcmp(s, "full_heal_sp_all") == 0) return ITEM_FULL_HEAL_SP_ALL;
    return ITEM_HEALING;
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
    return ELEM_PHYSICAL;
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

static void parse_item(IParser* p, ItemDB* db) {
    if (db->count >= MAX_ITEMS) { i_skip_value(p); return; }
    Item it;
    memset(&it, 0, sizeof(Item));
    char type_s[32] = "healing", elem_s[32] = "physical", cures_s[32] = "none";

    i_consume(p, '{');
    while (i_peek(p) != '}' && i_peek(p) != 0) {
        char key[64];
        if (!i_parse_string(p, key, 64)) break;
        i_consume(p, ':');
        if      (strcmp(key, "id") == 0)          it.id = i_parse_int(p);
        else if (strcmp(key, "name") == 0)        i_parse_string(p, it.name, 64);
        else if (strcmp(key, "description") == 0) i_parse_string(p, it.description, 128);
        else if (strcmp(key, "type") == 0)        i_parse_string(p, type_s, 32);
        else if (strcmp(key, "power") == 0)       it.power = i_parse_int(p);
        else if (strcmp(key, "cures") == 0)       i_parse_string(p, cures_s, 32);
        else if (strcmp(key, "element") == 0)     i_parse_string(p, elem_s, 32);
        else if (strcmp(key, "target_all") == 0)  it.target_all = i_parse_int(p);
        else if (strcmp(key, "price") == 0)       it.price = i_parse_int(p);
        else i_skip_value(p);
        i_consume(p, ',');
    }
    i_consume(p, '}');

    it.type    = item_type_from_str(type_s);
    it.element = (Element)element_from_str(elem_s);
    it.cures   = status_from_str(cures_s);
    db->items[db->count++] = it;
}

int item_load_json(ItemDB* db, const char* filepath) {
    FILE* f = fopen(filepath, "r");
    if (!f) { printf("item_loader: %s not found\n", filepath); return 0; }
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    char* buf = malloc(size + 1);
    if (!buf) { fclose(f); return 0; }
    fread(buf, 1, size, f); buf[size] = '\0'; fclose(f);

    db->count = 0;
    IParser p = { buf, 0, (int)size };
    i_consume(&p, '{');
    while (i_peek(&p) != '}' && i_peek(&p) != 0) {
        char key[64];
        i_parse_string(&p, key, 64);
        i_consume(&p, ':');
        if (strcmp(key, "items") == 0) {
            i_consume(&p, '[');
            while (i_peek(&p) != ']' && i_peek(&p) != 0) {
                parse_item(&p, db);
                i_consume(&p, ',');
            }
            i_consume(&p, ']');
        } else {
            i_skip_value(&p);
        }
        i_consume(&p, ',');
    }
    free(buf);
    printf("item_loader: loaded %d items from %s\n", db->count, filepath);
    return db->count > 0;
}