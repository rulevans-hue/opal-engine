#include "quest.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

// ── Minimal JSON parser (same style as npc.c) ─────────────────────────────────
typedef struct { const char* src; int pos; int len; } QParser;

static void q_skip_ws(QParser* p) {
    while (p->pos < p->len && isspace((unsigned char)p->src[p->pos])) p->pos++;
}
static int q_peek(QParser* p) {
    q_skip_ws(p);
    return (p->pos >= p->len) ? 0 : p->src[p->pos];
}
static int q_consume(QParser* p, char c) {
    q_skip_ws(p);
    if (p->pos >= p->len || p->src[p->pos] != c) return 0;
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
    char c = p->src[p->pos];
    if (c == '"') { char t[256]; q_parse_string(p, t, 256); }
    else if (c == '{' || c == '[') {
        char open = c, close = (c=='{') ? '}' : ']';
        p->pos++; int depth = 1;
        while (p->pos < p->len && depth > 0) {
            char ch = p->src[p->pos];
            if (ch == '"') { char t[256]; q_parse_string(p, t, 256); continue; }
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

static int q_parse_quest(QParser* p, Quest* out) {
    if (!q_consume(p, '{')) return 0;
    memset(out, 0, sizeof(Quest));
    out->active = 1;

    while (q_peek(p) != '}' && q_peek(p) != 0) {
        char key[64];
        if (!q_parse_string(p, key, 64)) break;
        q_consume(p, ':');

        if      (strcmp(key, "id") == 0)            q_parse_string(p, out->id, QUEST_ID_LEN);
        else if (strcmp(key, "title") == 0)         q_parse_string(p, out->title, QUEST_TEXT_LEN);
        else if (strcmp(key, "description") == 0)   q_parse_string(p, out->description, QUEST_TEXT_LEN);
        else if (strcmp(key, "unlock_flag") == 0)   q_parse_string(p, out->unlock_flag, QUEST_FLAG_LEN);
        else if (strcmp(key, "complete_flag") == 0) q_parse_string(p, out->complete_flag, QUEST_FLAG_LEN);
        else if (strcmp(key, "rewards") == 0) {
            q_consume(p, '[');
            int ri = 0;
            while (q_peek(p) != ']' && q_peek(p) != 0) {
                QuestReward rw;
                memset(&rw, 0, sizeof(QuestReward));
                rw.type = QUEST_REWARD_NONE;
                q_consume(p, '{');
                char rtype[64] = "none";
                while (q_peek(p) != '}' && q_peek(p) != 0) {
                    char rkey[64];
                    if (!q_parse_string(p, rkey, 64)) break;
                    q_consume(p, ':');
                    if      (strcmp(rkey, "type") == 0)      q_parse_string(p, rtype, 64);
                    else if (strcmp(rkey, "amount") == 0)    rw.amount = q_parse_int(p);
                    else if (strcmp(rkey, "item_id") == 0)   rw.item_id = q_parse_int(p);
                    else if (strcmp(rkey, "bond") == 0) q_parse_string(p, rw.bond, QUEST_ID_LEN);
                    else q_skip_value(p);
                    q_consume(p, ',');
                }
                q_consume(p, '}');
                if      (strcmp(rtype, "yen") == 0)       rw.type = QUEST_REWARD_YEN;
                else if (strcmp(rtype, "item") == 0)      rw.type = QUEST_REWARD_ITEM;
                else if (strcmp(rtype, "bond") == 0) rw.type = QUEST_REWARD_BOND;
                if (ri < MAX_QUEST_REWARDS) {
                    out->rewards[ri] = rw;
                    ri++;
                }
                q_consume(p, ',');
            }
            q_consume(p, ']');
            out->reward_count = ri;
        }
        else q_skip_value(p);

        q_consume(p, ',');
    }
    q_consume(p, '}');
    return 1;
}

int quest_registry_load_json(QuestRegistry* reg, const char* filepath) {
    FILE* f = fopen(filepath, "r");
    if (!f) { printf("quest_registry: %s not found\n", filepath); return 0; }
    fseek(f, 0, SEEK_END); long size = ftell(f); fseek(f, 0, SEEK_SET);
    char* buf = malloc(size + 1);
    if (!buf) { fclose(f); return 0; }
    fread(buf, 1, size, f); buf[size] = '\0'; fclose(f);

    QParser p = { buf, 0, (int)size };
    q_consume(&p, '{');
    while (q_peek(&p) != '}' && q_peek(&p) != 0) {
        char key[64];
        q_parse_string(&p, key, 64);
        q_consume(&p, ':');
        if (strcmp(key, "quests") == 0) {
            q_consume(&p, '[');
            while (q_peek(&p) != ']' && q_peek(&p) != 0) {
                if (reg->count < MAX_QUESTS) {
                    if (q_parse_quest(&p, &reg->quests[reg->count]))
                        reg->count++;
                } else {
                    Quest throwaway;
                    q_parse_quest(&p, &throwaway);
                }
                q_consume(&p, ',');
            }
            q_consume(&p, ']');
        } else {
            q_skip_value(&p);
        }
        q_consume(&p, ',');
    }
    free(buf);
    printf("quest_registry: loaded %d quests from %s\n", reg->count, filepath);
    return 1;
}

void quest_registry_init(QuestRegistry* reg) {
    memset(reg, 0, sizeof(QuestRegistry));
    quest_registry_load_json(reg, "data/quests.json");
}

Quest* quest_find_by_id(QuestRegistry* reg, const char* id) {
    if (!reg || !id) return NULL;
    for (int i = 0; i < reg->count; i++)
        if (strcmp(reg->quests[i].id, id) == 0) return &reg->quests[i];
    return NULL;
}

static void quest_grant_reward(Quest* q, Inventory* inv, BondDB* cdb,
                               NotifySystem* ns) {
    for (int i = 0; i < q->reward_count; i++) {
        QuestReward* rw = &q->rewards[i];
        switch (rw->type) {
            case QUEST_REWARD_YEN: {
                inv->yen += rw->amount;
                char msg[80];
                snprintf(msg, 80, "Reward: %d yen", rw->amount);
                notify_success(ns, msg);
                break;
            }
            case QUEST_REWARD_ITEM: {
                inventory_add(inv, rw->item_id, rw->amount > 0 ? rw->amount : 1);
                notify_success(ns, "Reward: item received!");
                break;
            }
            case QUEST_REWARD_BOND: {
                bond_award_points_by_name(cdb, rw->bond, rw->amount, ns);
                char msg[80];
                snprintf(msg, 80, "Reward: bond with %s deepened", rw->bond);
                notify_success(ns, msg);
                break;
            }
            default: break;
        }
    }
}

int quest_tick(QuestRegistry* reg, GameState* gs, Inventory* inv,
               BondDB* cdb, NotifySystem* ns) {
    if (!reg || !gs) return 0;
    int just_completed = 0;

    for (int i = 0; i < reg->count; i++) {
        Quest* q = &reg->quests[i];
        if (!q->active) continue;

        if (!q->unlocked && q->unlock_flag[0] &&
            gamestate_get_flag(gs, q->unlock_flag)) {
            q->unlocked = 1;
            char msg[128];
            snprintf(msg, 128, "New request: %s", q->title);
            if (ns) notify_info(ns, msg);
        }

        if (q->unlocked && !q->completed && q->complete_flag[0] &&
            gamestate_get_flag(gs, q->complete_flag)) {
            q->completed = 1;
            char msg[128];
            snprintf(msg, 128, "Request complete: %s", q->title);
            if (ns) notify_success(ns, msg);
            just_completed++;
        }

        if (q->completed && !q->reward_given) {
            q->reward_given = 1;
            quest_grant_reward(q, inv, cdb, ns);
        }
    }
    return just_completed;
}