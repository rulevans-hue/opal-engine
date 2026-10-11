#include "equipment_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ── Minimal JSON parser ───────────────────────────────────────────────────────
typedef struct { const char* src; int pos; int len; } QParser;

static void q_skip_ws(QParser* p) {
    while (p->pos < p->len && isspace((unsigned char)p->src[p->pos])) p->pos++;
}
static int q_peek(QParser* p) {
    q_skip_ws(p);
    return (p->pos >= p->len) ? 0 : p->src[p->pos];
}
static int q_consume(QParser* p, char ch) {
    q_skip_ws(p);
    if (p->pos >= p->len || p->src[p->pos] != ch) return 0;
    p->pos++; return 1;
}
static int q_parse_string(QParser* p, char* buf, int buf_len) {
    q_skip_ws(p);
    if (!q_consume(p, '"')) return 0;
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
    q_consume(p, '"');
    return 1;
}
static int q_parse_int(QParser* p) {
    q_skip_ws(p);
    int neg = 0;
    if (p->pos < p->len && p->src[p->pos] == '-') { neg = 1; p->pos++; }
    int v = 0;
    while (p->pos < p->len && isdigit((unsigned char)p->src[p->pos]))
        v = v * 10 + (p->src[p->pos++] - '0');
    return neg ? -v : v;
}
static void q_skip_value(QParser* p) {
    q_skip_ws(p);
    if (p->pos >= p->len) return;
    char ch = p->src[p->pos];
    if (ch == '"') { char t[256]; q_parse_string(p, t, 256); }
    else if (ch == '{' || ch == '[') {
        char open = ch, close = (ch=='{') ? '}' : ']';
        p->pos++; int depth = 1;
        while (p->pos < p->len && depth > 0) {
            char c2 = p->src[p->pos];
            if (c2 == '"') { char t[256]; q_parse_string(p, t, 256); continue; }
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
static EquipSlot slot_from_str(const char* s) {
    if (strcmp(s, "weapon") == 0)    return EQUIP_WEAPON;
    if (strcmp(s, "armor") == 0)     return EQUIP_ARMOR;
    if (strcmp(s, "accessory") == 0) return EQUIP_ACCESSORY;
    return EQUIP_WEAPON;
}
static StatModType mod_from_str(const char* s) {
    if (strcmp(s, "st") == 0)     return MOD_ST;
    if (strcmp(s, "ma") == 0)     return MOD_MA;
    if (strcmp(s, "en") == 0)     return MOD_EN;
    if (strcmp(s, "ag") == 0)     return MOD_AG;
    if (strcmp(s, "lu") == 0)     return MOD_LU;
    if (strcmp(s, "max_hp") == 0) return MOD_MAX_HP;
    if (strcmp(s, "max_sp") == 0) return MOD_MAX_SP;
    return MOD_ST;
}

static void parse_piece(QParser* p, EquipDB* db) {
    if (db->count >= MAX_EQUIPMENT) { q_skip_value(p); return; }
    EquipmentPiece pc;
    memset(&pc, 0, sizeof(EquipmentPiece));
    char slot_s[32] = "weapon";

    q_consume(p, '{');
    while (q_peek(p) != '}' && q_peek(p) != 0) {
        char key[64];
        if (!q_parse_string(p, key, 64)) break;
        q_consume(p, ':');
        if      (strcmp(key, "id") == 0)          pc.id = q_parse_int(p);
        else if (strcmp(key, "name") == 0)        q_parse_string(p, pc.name, 64);
        else if (strcmp(key, "description") == 0) q_parse_string(p, pc.description, 128);
        else if (strcmp(key, "slot") == 0)        q_parse_string(p, slot_s, 32);
        else if (strcmp(key, "price") == 0)       pc.price = q_parse_int(p);
        else if (strcmp(key, "mods") == 0) {
            q_consume(p, '[');
            while (q_peek(p) != ']' && q_peek(p) != 0) {
                q_consume(p, '{');
                char mtype[32] = "st"; int mval = 0;
                while (q_peek(p) != '}' && q_peek(p) != 0) {
                    char k2[64];
                    if (!q_parse_string(p, k2, 64)) break;
                    q_consume(p, ':');
                    if      (strcmp(k2, "type") == 0)  q_parse_string(p, mtype, 32);
                    else if (strcmp(k2, "value") == 0) mval = q_parse_int(p);
                    else q_skip_value(p);
                    q_consume(p, ',');
                }
                q_consume(p, '}');
                if (pc.mod_count < MAX_STAT_MODS) {
                    pc.mods[pc.mod_count].type  = mod_from_str(mtype);
                    pc.mods[pc.mod_count].value = mval;
                    pc.mod_count++;
                }
                q_consume(p, ',');
            }
            q_consume(p, ']');
        }
        else q_skip_value(p);
        q_consume(p, ',');
    }
    q_consume(p, '}');

    pc.slot = slot_from_str(slot_s);
    db->pieces[db->count++] = pc;
}

int equipment_load_json(EquipDB* db, const char* filepath) {
    FILE* f = fopen(filepath, "r");
    if (!f) { printf("equipment_loader: %s not found\n", filepath); return 0; }
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    char* buf = malloc(size + 1);
    if (!buf) { fclose(f); return 0; }
    fread(buf, 1, size, f); buf[size] = '\0'; fclose(f);

    db->count = 0;
    QParser p = { buf, 0, (int)size };
    q_consume(&p, '{');
    while (q_peek(&p) != '}' && q_peek(&p) != 0) {
        char key[64];
        q_parse_string(&p, key, 64);
        q_consume(&p, ':');
        if (strcmp(key, "equipment") == 0) {
            q_consume(&p, '[');
            while (q_peek(&p) != ']' && q_peek(&p) != 0) {
                parse_piece(&p, db);
                q_consume(&p, ',');
            }
            q_consume(&p, ']');
        } else {
            q_skip_value(&p);
        }
        q_consume(&p, ',');
    }
    free(buf);
    printf("equipment_loader: loaded %d pieces from %s\n", db->count, filepath);
    return db->count > 0;
}