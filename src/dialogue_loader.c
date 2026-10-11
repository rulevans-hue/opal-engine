#include "dialogue_loader.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ── Minimal JSON parser ───────────────────────────────────────────────────────
typedef struct {
    const char* src;
    int         pos;
    int         len;
} Parser;

static void skip_ws(Parser* p) {
    while (p->pos < p->len && isspace((unsigned char)p->src[p->pos]))
        p->pos++;
}

static int peek(Parser* p) {
    skip_ws(p);
    if (p->pos >= p->len) return 0;
    return p->src[p->pos];
}

static int consume(Parser* p, char c) {
    skip_ws(p);
    if (p->pos >= p->len || p->src[p->pos] != c) return 0;
    p->pos++;
    return 1;
}

static int parse_string(Parser* p, char* buf, int buf_len) {
    skip_ws(p);
    if (!consume(p, '"')) return 0;
    int i = 0;
    while (p->pos < p->len && p->src[p->pos] != '"') {
        if (p->src[p->pos] == '\\') {
            p->pos++;
            if (p->pos >= p->len) break;
            char esc = p->src[p->pos];
            if (esc == 'n')       { if (i < buf_len-1) buf[i++] = '\n'; }
            else if (esc == '"')  { if (i < buf_len-1) buf[i++] = '"';  }
            else if (esc == '\\') { if (i < buf_len-1) buf[i++] = '\\'; }
            else                  { if (i < buf_len-1) buf[i++] = esc;  }
        } else {
            if (i < buf_len-1) buf[i++] = p->src[p->pos];
        }
        p->pos++;
    }
    buf[i] = '\0';
    consume(p, '"');
    return 1;
}

static int parse_int(Parser* p, int* ok) {
    skip_ws(p);
    int neg = 0;
    if (p->pos < p->len && p->src[p->pos] == '-') { neg = 1; p->pos++; }
    if (p->pos >= p->len || !isdigit((unsigned char)p->src[p->pos])) {
        *ok = 0; return 0;
    }
    int val = 0;
    while (p->pos < p->len && isdigit((unsigned char)p->src[p->pos]))
        val = val * 10 + (p->src[p->pos++] - '0');
    *ok = 1;
    return neg ? -val : val;
}

static void skip_value(Parser* p) {
    skip_ws(p);
    if (p->pos >= p->len) return;
    char c = p->src[p->pos];
    if (c == '"') {
        char tmp[512]; parse_string(p, tmp, 512);
    } else if (c == '{') {
        p->pos++; int depth = 1;
        while (p->pos < p->len && depth > 0) {
            if      (p->src[p->pos] == '{') depth++;
            else if (p->src[p->pos] == '}') depth--;
            else if (p->src[p->pos] == '"') {
                char tmp[512]; parse_string(p, tmp, 512); continue;
            }
            p->pos++;
        }
    } else if (c == '[') {
        p->pos++; int depth = 1;
        while (p->pos < p->len && depth > 0) {
            if      (p->src[p->pos] == '[') depth++;
            else if (p->src[p->pos] == ']') depth--;
            else if (p->src[p->pos] == '"') {
                char tmp[512]; parse_string(p, tmp, 512); continue;
            }
            p->pos++;
        }
    } else {
        while (p->pos < p->len) {
            char ch = p->src[p->pos];
            if (ch == ',' || ch == '}' || ch == ']' || isspace((unsigned char)ch)) break;
            p->pos++;
        }
    }
}

// ── LEGACY: parse one node into the flat global array (unchanged) ─────────────
static int parse_node(Parser* p, DialogueSystem* ds) {
    if (!consume(p, '{')) return 0;

    int  id      = -1;
    char speaker[64] = "";
    char text[MAX_TEXT_LEN] = "";
    int  next    = -1;
    char sets_flag[FLAG_NAME_LEN] = "";
    int  flag_val = 0;
    char choice_text[4][MAX_TEXT_LEN];
    int  choice_next[4];
    int  choice_count = 0;
    memset(choice_text, 0, sizeof(choice_text));
    memset(choice_next, -1, sizeof(choice_next));

    while (peek(p) != '}' && peek(p) != 0) {
        char key[64];
        if (!parse_string(p, key, 64)) break;
        consume(p, ':');

        if (strcmp(key, "id") == 0) {
            int ok; id = parse_int(p, &ok);
        } else if (strcmp(key, "speaker") == 0) {
            parse_string(p, speaker, 64);
        } else if (strcmp(key, "text") == 0) {
            parse_string(p, text, MAX_TEXT_LEN);
        } else if (strcmp(key, "next") == 0) {
            int ok; next = parse_int(p, &ok);
        } else if (strcmp(key, "sets_flag") == 0) {
            parse_string(p, sets_flag, FLAG_NAME_LEN);
        } else if (strcmp(key, "flag_value") == 0) {
            int ok; flag_val = parse_int(p, &ok);
        } else if (strcmp(key, "choices") == 0) {
            consume(p, '[');
            while (peek(p) != ']' && peek(p) != 0 && choice_count < 4) {
                consume(p, '{');
                while (peek(p) != '}' && peek(p) != 0) {
                    char ckey[32];
                    parse_string(p, ckey, 32);
                    consume(p, ':');
                    if (strcmp(ckey, "text") == 0) {
                        parse_string(p, choice_text[choice_count], MAX_TEXT_LEN);
                    } else if (strcmp(ckey, "next") == 0) {
                        int ok;
                        choice_next[choice_count] = parse_int(p, &ok);
                    } else {
                        skip_value(p);
                    }
                    consume(p, ',');
                }
                consume(p, '}');
                choice_count++;
                consume(p, ',');
            }
            consume(p, ']');
        } else {
            skip_value(p);
        }
        consume(p, ',');
    }
    consume(p, '}');

    if (id < 0 || id >= MAX_NODES) return 1;

    DialogueNode* node = &ds->nodes[id];
    memset(node, 0, sizeof(DialogueNode));
    strncpy(node->speaker,    speaker, 63);
    strncpy(node->text,       text,    MAX_TEXT_LEN-1);
    strncpy(node->sets_flag,  sets_flag, FLAG_NAME_LEN-1);
    node->sets_flag_value = flag_val;
    node->next            = next;
    node->choice_count    = choice_count;
    for (int i = 0; i < choice_count; i++) {
        strncpy(node->choice_text[i], choice_text[i], MAX_TEXT_LEN-1);
        node->choice_next[i] = choice_next[i];
    }
    if (id >= ds->node_count) ds->node_count = id + 1;
    return 1;
}

// Parse an "effect" object into *eff. All fields optional.
static void parse_effect(Parser* p, DialogueChoiceEffect* eff) {
    memset(eff, 0, sizeof(DialogueChoiceEffect));
    if (!consume(p, '{')) return;
    while (peek(p) != '}' && peek(p) != 0) {
        char k[64];
        parse_string(p, k, 64);
        consume(p, ':');
        if (strcmp(k, "set_flag") == 0) {
            parse_string(p, eff->set_flag, FLAG_NAME_LEN);
        } else if (strcmp(k, "set_flag_value") == 0) {
            int ok; eff->set_flag_value = parse_int(p, &ok);
        } else if (strcmp(k, "award_bond") == 0) {
            parse_string(p, eff->award_bond, 64);
        } else if (strcmp(k, "award_points") == 0) {
            int ok; eff->award_points = parse_int(p, &ok);
        } else if (strcmp(k, "goto_conversation") == 0) {
            parse_string(p, eff->goto_conversation, CONVO_KEY_LEN);
        } else if (strcmp(k, "open_shop") == 0) {
            int ok; eff->open_shop = parse_int(p, &ok);
        } else if (strcmp(k, "start_combat") == 0) {
            int ok; eff->start_combat = parse_int(p, &ok);
        } else {
            skip_value(p);
        }
        consume(p, ',');
    }
    consume(p, '}');
}

// ── NEW: parse one node into *out (LOCAL index; no global id) ─────────────────
// Used for keyed conversations. Array position = local index; "next" is local.
static int parse_node_local(Parser* p, DialogueNode* out) {
    if (!consume(p, '{')) return 0;
    memset(out, 0, sizeof(DialogueNode));
    out->next = -1;
    for (int i = 0; i < MAX_CHOICES; i++) out->choice_next[i] = -1;

    while (peek(p) != '}' && peek(p) != 0) {
        char key[64];
        if (!parse_string(p, key, 64)) break;
        consume(p, ':');

        if (strcmp(key, "speaker") == 0) {
            parse_string(p, out->speaker, 64);
        } else if (strcmp(key, "text") == 0) {
            parse_string(p, out->text, MAX_TEXT_LEN);
        } else if (strcmp(key, "next") == 0) {
            int ok; out->next = parse_int(p, &ok);
        } else if (strcmp(key, "sets_flag") == 0) {
            parse_string(p, out->sets_flag, FLAG_NAME_LEN);
        } else if (strcmp(key, "flag_value") == 0) {
            int ok; out->sets_flag_value = parse_int(p, &ok);
        } else if (strcmp(key, "effect") == 0) {
            parse_effect(p, &out->node_effect);
        } else if (strcmp(key, "choices") == 0) {
            consume(p, '[');
            int cc = 0;
            while (peek(p) != ']' && peek(p) != 0 && cc < MAX_CHOICES) {
                consume(p, '{');
                while (peek(p) != '}' && peek(p) != 0) {
                    char ckey[32];
                    parse_string(p, ckey, 32);
                    consume(p, ':');
                    if (strcmp(ckey, "text") == 0) {
                        parse_string(p, out->choice_text[cc], MAX_TEXT_LEN);
                    } else if (strcmp(ckey, "next") == 0) {
                        int ok; out->choice_next[cc] = parse_int(p, &ok);
                    } else if (strcmp(ckey, "effect") == 0) {
                        parse_effect(p, &out->choice_effect[cc]);
                    } else {
                        skip_value(p);
                    }
                    consume(p, ',');
                }
                consume(p, '}');
                cc++;
                consume(p, ',');
            }
            consume(p, ']');
            out->choice_count = cc;
        } else {
            skip_value(p);
        }
        consume(p, ',');
    }
    consume(p, '}');
    return 1;
}

// ── NEW: parse one keyed conversation into the DB ─────────────────────────────
static int parse_conversation(Parser* p, DialogueDB* db) {
    static Conversation scratch;   // used if DB is full (avoids stack cost)
    if (!consume(p, '{')) return 0;

    Conversation* c = (db->count < MAX_CONVERSATIONS)
                      ? &db->convos[db->count] : &scratch;
    memset(c, 0, sizeof(Conversation));

    while (peek(p) != '}' && peek(p) != 0) {
        char key[64];
        if (!parse_string(p, key, 64)) break;
        consume(p, ':');

        if (strcmp(key, "key") == 0) {
            parse_string(p, c->key, CONVO_KEY_LEN);
        } else if (strcmp(key, "nodes") == 0) {
            consume(p, '[');
            while (peek(p) != ']' && peek(p) != 0) {
                if (c->node_count < MAX_NODES_PER_CONVO) {
                    parse_node_local(p, &c->nodes[c->node_count]);
                    c->node_count++;
                } else {
                    DialogueNode throwaway;
                    parse_node_local(p, &throwaway);
                }
                consume(p, ',');
            }
            consume(p, ']');
        } else {
            skip_value(p);
        }
        consume(p, ',');
    }
    consume(p, '}');

    if (db->count < MAX_CONVERSATIONS && c->key[0] != '\0')
        db->count++;
    return 1;
}

int dialogue_load_json(DialogueSystem* ds, DialogueDB* db,
                       GameState* gs, const char* filepath) {
    (void)gs;
    FILE* f = fopen(filepath, "r");
    if (!f) {
        printf("dialogue_loader: %s not found, using hardcoded nodes\n",
               filepath);
        return 0;
    }

    fseek(f, 0, SEEK_END);
    long size = ftell(f);
    fseek(f, 0, SEEK_SET);

    char* buf = malloc(size + 1);
    if (!buf) { fclose(f); return 0; }
    fread(buf, 1, size, f);
    buf[size] = '\0';
    fclose(f);

    Parser p = { buf, 0, (int)size };

    consume(&p, '{');
    while (peek(&p) != '}' && peek(&p) != 0) {
        char key[32];
        parse_string(&p, key, 32);
        consume(&p, ':');
        if (strcmp(key, "nodes") == 0) {
            consume(&p, '[');
            while (peek(&p) != ']' && peek(&p) != 0) {
                parse_node(&p, ds);
                consume(&p, ',');
            }
            consume(&p, ']');
        } else if (strcmp(key, "conversations") == 0) {
            consume(&p, '[');
            while (peek(&p) != ']' && peek(&p) != 0) {
                parse_conversation(&p, db);
                consume(&p, ',');
            }
            consume(&p, ']');
        } else {
            skip_value(&p);
        }
        consume(&p, ',');
    }

    free(buf);
    printf("dialogue_loader: loaded %d legacy nodes, %d conversations from %s\n",
           ds->node_count, db ? db->count : 0, filepath);
    return 1;
}