#include "npc.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ── Minimal JSON parser (same style as dialogue_loader.c) ─────────────────────
typedef struct { const char* src; int pos; int len; } NParser;

static void n_skip_ws(NParser* p) {
    while (p->pos < p->len && isspace((unsigned char)p->src[p->pos])) p->pos++;
}
static int n_peek(NParser* p) {
    n_skip_ws(p);
    return (p->pos >= p->len) ? 0 : p->src[p->pos];
}
static int n_consume(NParser* p, char c) {
    n_skip_ws(p);
    if (p->pos >= p->len || p->src[p->pos] != c) return 0;
    p->pos++; return 1;
}
static int n_parse_string(NParser* p, char* buf, int buf_len) {
    n_skip_ws(p);
    if (!n_consume(p, '"')) return 0;
    int i = 0;
    while (p->pos < p->len && p->src[p->pos] != '"') {
        if (p->src[p->pos] == '\\') {
            p->pos++;
            if (p->pos >= p->len) break;
            char e = p->src[p->pos];
            if (e == 'n')      { if (i < buf_len-1) buf[i++] = '\n'; }
            else if (e == '"') { if (i < buf_len-1) buf[i++] = '"';  }
            else if (e == '\\'){ if (i < buf_len-1) buf[i++] = '\\'; }
            else               { if (i < buf_len-1) buf[i++] = e;    }
        } else {
            if (i < buf_len-1) buf[i++] = p->src[p->pos];
        }
        p->pos++;
    }
    buf[i] = '\0';
    n_consume(p, '"');
    return 1;
}
static float n_parse_number(NParser* p) {
    n_skip_ws(p);
    char tmp[64]; int i = 0;
    while (p->pos < p->len && i < 63) {
        char c = p->src[p->pos];
        if (isdigit((unsigned char)c) || c=='-' || c=='+' || c=='.' ) {
            tmp[i++] = c; p->pos++;
        } else break;
    }
    tmp[i] = '\0';
    return (float)atof(tmp);
}
static void n_skip_value(NParser* p) {
    n_skip_ws(p);
    if (p->pos >= p->len) return;
    char c = p->src[p->pos];
    if (c == '"') { char t[256]; n_parse_string(p, t, 256); }
    else if (c == '{' || c == '[') {
        char open = c, close = (c=='{') ? '}' : ']';
        p->pos++; int depth = 1;
        while (p->pos < p->len && depth > 0) {
            char ch = p->src[p->pos];
            if (ch == '"') { char t[256]; n_parse_string(p, t, 256); continue; }
            if (ch == open) depth++;
            else if (ch == close) depth--;
            p->pos++;
        }
    } else {
        while (p->pos < p->len) {
            char ch = p->src[p->pos];
            if (ch==','||ch=='}'||ch==']'||isspace((unsigned char)ch)) break;
            p->pos++;
        }
    }
}

// ── Parse one NPC object ──────────────────────────────────────────────────────
static int n_parse_npc(NParser* p, NPCDefinition* out) {
    if (!n_consume(p, '{')) return 0;
    memset(out, 0, sizeof(NPCDefinition));
    out->active = 1;

    while (n_peek(p) != '}' && n_peek(p) != 0) {
        char key[64];
        if (!n_parse_string(p, key, 64)) break;
        n_consume(p, ':');

        if      (strcmp(key, "id") == 0)            n_parse_string(p, out->id, NPC_ID_LEN);
        else if (strcmp(key, "display_name") == 0)  n_parse_string(p, out->display_name, NPC_NAME_LEN);
        else if (strcmp(key, "x") == 0)             out->x = n_parse_number(p);
        else if (strcmp(key, "y") == 0)             out->y = n_parse_number(p);
        else if (strcmp(key, "z") == 0)             out->z = n_parse_number(p);
        else if (strcmp(key, "convo_first") == 0)   n_parse_string(p, out->convo_first, CONVO_KEY_LEN);
        else if (strcmp(key, "convo_repeat") == 0)  n_parse_string(p, out->convo_repeat, CONVO_KEY_LEN);
        else if (strcmp(key, "met_flag") == 0)      n_parse_string(p, out->met_flag, FLAG_NAME_LEN);
        else if (strcmp(key, "bond_name") == 0)n_parse_string(p, out->bond_name, NPC_NAME_LEN);
        else n_skip_value(p);

        n_consume(p, ',');
    }
    n_consume(p, '}');
    return 1;
}

int npc_registry_load_json(NPCRegistry* reg, const char* filepath) {
    FILE* f = fopen(filepath, "r");
    if (!f) {
        printf("npc_registry: %s not found\n", filepath);
        return 0;
    }
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    char* buf = malloc(size + 1);
    if (!buf) { fclose(f); return 0; }
    fread(buf, 1, size, f); buf[size] = '\0'; fclose(f);

    NParser p = { buf, 0, (int)size };
    n_consume(&p, '{');
    while (n_peek(&p) != '}' && n_peek(&p) != 0) {
        char key[64];
        n_parse_string(&p, key, 64);
        n_consume(&p, ':');
        if (strcmp(key, "npcs") == 0) {
            n_consume(&p, '[');
            while (n_peek(&p) != ']' && n_peek(&p) != 0) {
                if (reg->count < MAX_NPCS) {
                    if (n_parse_npc(&p, &reg->npcs[reg->count]))
                        reg->count++;
                } else {
                    NPCDefinition throwaway;
                    n_parse_npc(&p, &throwaway);
                }
                n_consume(&p, ',');
            }
            n_consume(&p, ']');
        } else {
            n_skip_value(&p);
        }
        n_consume(&p, ',');
    }
    free(buf);
    printf("npc_registry: loaded %d NPCs from %s\n", reg->count, filepath);
    return 1;
}

void npc_registry_init(NPCRegistry* reg) {
    memset(reg, 0, sizeof(NPCRegistry));
    npc_registry_load_json(reg, "data/npcs.json");
}

NPCDefinition* npc_find_by_id(NPCRegistry* reg, const char* id) {
    if (!reg || !id) return NULL;
    for (int i = 0; i < reg->count; i++) {
        if (strcmp(reg->npcs[i].id, id) == 0) return &reg->npcs[i];
    }
    return NULL;
}

NPCDefinition* npc_get(NPCRegistry* reg, int index) {
    if (!reg || index < 0 || index >= reg->count) return NULL;
    return &reg->npcs[index];
}

// Resolve which conversation to play and start it. C owns this logic.
// - Non-bond NPC: first-meeting (convo_first) then convo_repeat.
// - Bond NPC: ready-to-rank-up -> "<id>_rank<N>", else "<id>_filler".
//   Falls back to filler if the rank scene isn't authored yet.
int npc_interact(NPCRegistry* reg, const char* id,
                 DialogueSystem* ds, GameState* gs, BondDB* cdb) {
    NPCDefinition* npc = npc_find_by_id(reg, id);
    if (!npc || !npc->active) return 0;

    // ── Bond NPC: resolve by relationship state ──
    if (npc->bond_name[0] && cdb) {
        Bond* c = bond_find_by_name(cdb, npc->bond_name);
        if (c) {
            // Mark as met (first-meeting flag, for any non-bond uses)
            if (npc->met_flag[0])
                gamestate_set_flag(gs, npc->met_flag, 1);

            int ready = 0;
            if (c->current_rank < c->max_rank) {
                BondRank* cur = &c->ranks[c->current_rank];
                if (c->points >= cur->points_required)
                    ready = 1;
            }

            char key[CONVO_KEY_LEN];
            if (ready) {
                int target = c->current_rank + 1;
                snprintf(key, CONVO_KEY_LEN, "%s_rank%d", id, target);
                if (dialogue_db_find(&g_dialogue_db, key)) {
                    c->rankup_in_progress = 1;
                } else {
                    snprintf(key, CONVO_KEY_LEN, "%s_filler", id);
                }
            } else {
                snprintf(key, CONVO_KEY_LEN, "%s_filler", id);
            }

            dialogue_start_key(ds, key);
            ds->hangout_pending = 1;   // bond hangout — costs a time slot on end
            return 1;
        }
        // bond_name set but not found — fall through to normal handling
    }

    // ── Non-bond NPC: first/repeat ──
    int met = (npc->met_flag[0]) ? gamestate_get_flag(gs, npc->met_flag) : 0;
    const char* key;
    if (!met && npc->convo_first[0]) {
        key = npc->convo_first;
        if (npc->met_flag[0])
            gamestate_set_flag(gs, npc->met_flag, 1);
    } else if (npc->convo_repeat[0]) {
        key = npc->convo_repeat;
    } else if (npc->convo_first[0]) {
        key = npc->convo_first;
    } else {
        return 0;
    }
    dialogue_start_key(ds, key);
    return 1;
}