#include <stdarg.h>
#include "dialogue.h"
#include "dialogue_loader.h"
#include <string.h>
#include <stdio.h>
#include "bond.h"
DialogueDB g_dialogue_db;

// Returns the node array currently being played (flat or keyed).
static DialogueNode* dialogue_active_nodes(DialogueSystem* ds) {
    return ds->using_keyed ? ds->keyed_nodes : ds->nodes;
}

// Helper to set a simple linear node
static void set_node(DialogueSystem* ds, int id,
                     const char* speaker, const char* text,
                     int next, const char* flag, int flag_val) {
    DialogueNode* n = &ds->nodes[id];
    memset(n, 0, sizeof(DialogueNode));
    strncpy(n->speaker, speaker, 63);
    strncpy(n->text,    text,    MAX_TEXT_LEN - 1);
    n->choice_count    = 0;
    n->next            = next;
    if (flag && flag[0]) {
        strncpy(n->sets_flag, flag, FLAG_NAME_LEN - 1);
        n->sets_flag_value = flag_val;
    }
}

// Helper to set a choice node
static void set_choice_node(DialogueSystem* ds, int id,
                             const char* speaker, const char* text,
                             int num_choices, ...) {
    DialogueNode* n = &ds->nodes[id];
    memset(n, 0, sizeof(DialogueNode));
    strncpy(n->speaker, speaker, 63);
    strncpy(n->text,    text,    MAX_TEXT_LEN - 1);
    n->choice_count = num_choices;
    n->next         = -1;

    // choices passed as (text, next) pairs via va_list
    va_list args;
    va_start(args, num_choices);
    for (int i = 0; i < num_choices; i++) {
        const char* ct = va_arg(args, const char*);
        int         cn = va_arg(args, int);
        strncpy(n->choice_text[i], ct, MAX_TEXT_LEN - 1);
        n->choice_next[i] = cn;
    }
    va_end(args);
}

void dialogue_init(DialogueSystem* ds, GameState* gs) {
    memset(ds, 0, sizeof(DialogueSystem));
    memset(&g_dialogue_db, 0, sizeof(DialogueDB));
    ds->active = 0;

    // Try loading from JSON first — allows editing dialogue without recompiling
    if (dialogue_load_json(ds, &g_dialogue_db, gs, "data/dialogue.json"))
        return;

    // Fallback: hardcoded nodes
    (void)gs;

    // ── Node 0 — Stranger first meeting ──────────────────────────────────────
    set_node(ds, 0, "Stranger",
             "Hey. You're new around here, aren't you.",
             1, "", 0);

    set_node(ds, 1, "Stranger",
             "This town has a way of chewing people up. Watch yourself.",
             2, "", 0);

    set_node(ds, 2, "Stranger",
             "...The name's not important. What's yours?",
             3, "", 0);

    set_node(ds, 3, "You",
             "(You introduce yourself.)",
             4, "", 0);

    set_node(ds, 4, "Stranger",
             "Hm. Well, don't get into trouble you can't walk away from.",
             -1, "", 0);

    // ── Node 5 — Hooded Figure first meeting ─────────────────────────────────
    set_node(ds, 5, "Hooded Figure",
             "...You can see me.",
             6, "", 0);

    set_node(ds, 6, "Hooded Figure",
             "Most people walk right past. Interesting.",
             7, "", 0);

    set_node(ds, 7, "Hooded Figure",
             "...Don't worry. I'm not dangerous. Probably.",
             19, "", 0);

    // Node 19 — Hooded Figure hangout offer (first)
    set_node(ds, 19, "Hooded Figure",
             "You're welcome back here, if you want to talk.",
             20, "", 0);

    set_node(ds, 20, "Hooded Figure",
             "...Would you like to stay a while?",
             -1, "", 0);
    ds->nodes[20].choice_count = 2;
    strncpy(ds->nodes[20].choice_text[0], "Sure.", MAX_TEXT_LEN-1);
    strncpy(ds->nodes[20].choice_text[1], "Not today.", MAX_TEXT_LEN-1);
    ds->nodes[20].choice_next[0] = 11;
    ds->nodes[20].choice_next[1] = 12;

    // ── Node 7 — Hooded Figure repeat ────────────────────────────────────────
    // (overwritten above, re-set properly)
    set_node(ds, 7, "Hooded Figure",
             "...You came back. Most people avoid this alley.",
             17, "", 0);

    set_node(ds, 17, "Hooded Figure",
             "I suppose you're not like most people.",
             18, "", 0);

    set_node(ds, 18, "Hooded Figure",
             "...Would you like to stay a while?",
             -1, "", 0);
    ds->nodes[18].choice_count = 2;
    strncpy(ds->nodes[18].choice_text[0], "Sure, I'll stay.", MAX_TEXT_LEN-1);
    strncpy(ds->nodes[18].choice_text[1], "Not today.", MAX_TEXT_LEN-1);
    ds->nodes[18].choice_next[0] = 11;
    ds->nodes[18].choice_next[1] = 12;

    // ── Node 8 — Shopkeeper repeat ────────────────────────────────────────────
    set_node(ds, 8, "Shopkeeper",
             "You again. Need something, or just visiting?",
             -1, "", 0);
    ds->nodes[8].choice_count = 2;
    strncpy(ds->nodes[8].choice_text[0], "Just visiting.", MAX_TEXT_LEN-1);
    strncpy(ds->nodes[8].choice_text[1], "Just browsing.", MAX_TEXT_LEN-1);
    ds->nodes[8].choice_next[0] = 13;
    ds->nodes[8].choice_next[1] = 14;

    // ── Nodes 9-14 — hangout accept/decline ──────────────────────────────────
    set_node(ds, 9,  "Stranger",
             "Alright. Let's talk for a while.",
             -1, "hangout_Stranger", 1);

    set_node(ds, 10, "Stranger",
             "Maybe another time then.",
             -1, "", 0);

    set_node(ds, 11, "Hooded Figure",
             "Very well. Come, sit.",
             -1, "hangout_Hooded Figure", 1);

    set_node(ds, 12, "Hooded Figure",
             "As you wish.",
             -1, "", 0);

    set_node(ds, 13, "Shopkeeper",
             "Pull up a chair then.",
             -1, "hangout_Shopkeeper", 1);

    set_node(ds, 14, "Shopkeeper",
             "Let me know if you need anything.",
             -1, "", 0);

    // ── Node 15 — Shopkeeper interaction choice ───────────────────────────────
    set_node(ds, 15, "Shopkeeper",
             "What can I do for you?",
             -1, "", 0);
    ds->nodes[15].choice_count = 2;
    strncpy(ds->nodes[15].choice_text[0], "I want to browse your wares.", MAX_TEXT_LEN-1);
    strncpy(ds->nodes[15].choice_text[1], "Just wanted to talk.", MAX_TEXT_LEN-1);
    ds->nodes[15].choice_next[0] = 16;
    ds->nodes[15].choice_next[1] = 8;

    // ── Node 16 — open shop flag ──────────────────────────────────────────────
    set_node(ds, 16, "Shopkeeper",
             "Take your time.",
             -1, "open_shop", 1);

    // ── Stranger repeat (node 4 is first-meet end, use separate repeat) ───────
    // Stranger repeat starts at node 4 (reused as repeat node)
    // Already defined above as ending node — that works fine

    // ── Nodes 22-28 — Boss pre/post battle ───────────────────────────────────

    // Node 22 — Shadow King confrontation, first line
    set_node(ds, 22, "Shadow King",
             "So. Intruders have reached my throne at last.",
             23, "", 0);

    // Node 23 — Shadow King taunts
    set_node(ds, 23, "Shadow King",
             "You are brave. Or perhaps simply foolish. It makes no difference.",
             24, "", 0);

    // Node 24 — Shadow King reveals himself
    set_node(ds, 24, "Shadow King",
             "Everything in this dungeon belongs to me. Every coin, every life,"
             " every dream. And now... so do you.",
             25, "", 0);

    // Node 25 — Player choice: fight or flee
    set_node(ds, 25, "You",
             "(The Shadow King raises his hand. The air grows heavy.)",
             -1, "", 0);
    ds->nodes[25].choice_count = 2;
    strncpy(ds->nodes[25].choice_text[0],
            "We're taking you down!", MAX_TEXT_LEN-1);
    strncpy(ds->nodes[25].choice_text[1],
            "...(Stay silent.)", MAX_TEXT_LEN-1);
    // Both choices lead to combat starting — flag triggers it
    ds->nodes[25].choice_next[0] = -1;  // end dialogue, combat begins
    ds->nodes[25].choice_next[1] = -1;

    // Node 26 — Post-battle, Shadow King defeated (first line)
    set_node(ds, 26, "Shadow King",
             "...Impossible. I am this dungeon. I am the desire itself...",
             27, "", 0);

    // Node 27 — Shadow King fading
    set_node(ds, 27, "Shadow King",
             "How... how can mere children deny what I represent?",
             28, "", 0);

    // Node 28 — Shadow King's final words, dungeon begins to collapse
    set_node(ds, 28, "Shadow King",
             "...Perhaps greed was never as eternal as I believed."
             " Do with this... what you will.",
             29, "dungeon_1_cleared", 1);

    // Node 29 — Party reacts
    set_node(ds, 29, "You",
             "The dungeon is collapsing! Everyone, get out now!",
             30, "", 0);

    // Node 30 — Final node, triggers exit
    set_node(ds, 30, "Narrator",
             "The Castle of Greed crumbles. You escape into the real world.",
             -1, "boss_post_dialogue_done", 1);


    // ── Nodes 42-48 — Team meeting (first time reaching final floor) ──────────
    // Triggered automatically when player first steps onto the final floor.
    // Party discusses the plan before the boss.

    set_node(ds, 42, "Rook",
             "So this is it... the throne room floor.",
             43, "", 0);

    set_node(ds, 43, "Wren",
             "The ruler's presence is all over this place. Be careful.",
             44, "", 0);

    set_node(ds, 44, "Sable",
             "We've confirmed the route to the treasure."
             " The ruler is waiting at the end of it.",
             45, "", 0);

    set_node(ds, 45, "Rook",
             "Right. We stay together, find the ruler,"
             " and take it for real.",
             46, "", 0);

    set_node(ds, 46, "Wren",
             "Once we're in the throne room, there's no turning back."
             " Be ready.",
             47, "", 0);

    set_node(ds, 47, "You",
             "(You memorize the layout. The ruler is close.)",
             -1, "team_meeting_done", 1);

    ds->node_count = 48;
}

void dialogue_start(DialogueSystem* ds, int start_node) {
    if (start_node < 0 || start_node >= ds->node_count) return;
    ds->using_keyed  = 0;     // flat nodes
    ds->current_node = start_node;
    ds->active = 1;
}

void dialogue_advance(DialogueSystem* ds, int choice, GameState* gs,
                      BondDB* cdb, NotifySystem* ns) {
    if (!ds->active) return;
    fprintf(stderr, "ADV: using_keyed=%d current_node=%d keyed_count=%d node_next=%d\n",
            ds->using_keyed, ds->current_node, ds->keyed_node_count,
            dialogue_active_nodes(ds)[ds->current_node].next);
    DialogueNode* node = &dialogue_active_nodes(ds)[ds->current_node];

    // Apply node-level legacy flag
    if (strlen(node->sets_flag) > 0)
        gamestate_set_flag(gs, node->sets_flag, node->sets_flag_value);

    // Apply node-level effect
    if (node->node_effect.set_flag[0])
        gamestate_set_flag(gs, node->node_effect.set_flag,
                           node->node_effect.set_flag_value);
    if (node->node_effect.award_bond[0] && cdb)
        bond_award_points_by_name(cdb, node->node_effect.award_bond,
                                       node->node_effect.award_points, ns);

    if (node->choice_count > 0) {
        if (choice >= 0 && choice < node->choice_count) {
            DialogueChoiceEffect* eff = &node->choice_effect[choice];
            if (eff->set_flag[0])
                gamestate_set_flag(gs, eff->set_flag, eff->set_flag_value);
            if (eff->award_bond[0] && cdb)
                bond_award_points_by_name(cdb, eff->award_bond,
                                               eff->award_points, ns);

            int next = node->choice_next[choice];
            if (next == -1)
                ds->active = 0;
            else
                ds->current_node = next;
        }
    } else {
        if (node->next == -1)
            ds->active = 0;
        else
            ds->current_node = node->next;
    }

    // Conversation just ended — resolve any rank-up the scene delivered.
    if (!ds->active && cdb)
        bond_resolve_pending_rankups(cdb, ns);
}

void dialogue_render(SDL_Renderer* renderer, DialogueSystem* ds,
                     TTF_Font* font) {
    if (!ds->active) return;

    DialogueNode* node = &dialogue_active_nodes(ds)[ds->current_node];

    // Dark semi-transparent dialogue box
    SDL_SetRenderDrawColor(renderer, 10, 10, 30, 230);
    SDL_Rect box = { 40, 510, 1200, 190 };
    SDL_RenderFillRect(renderer, &box);
    SDL_SetRenderDrawColor(renderer, 180, 180, 255, 255);
    SDL_RenderDrawRect(renderer, &box);

    // Speaker name plate
    if (strlen(node->speaker) > 0 && node->choice_count == 0) {
        SDL_SetRenderDrawColor(renderer, 30, 30, 80, 255);
        SDL_Rect nameplate = { 40, 486, 300, 28 };
        SDL_RenderFillRect(renderer, &nameplate);
        SDL_SetRenderDrawColor(renderer, 180, 180, 255, 255);
        SDL_RenderDrawRect(renderer, &nameplate);

        SDL_Color nameColor = {255, 220, 100, 255};
        SDL_Surface* ns = TTF_RenderText_Blended(font, node->speaker, nameColor);
        if (ns) {
            SDL_Texture* nt = SDL_CreateTextureFromSurface(renderer, ns);
            SDL_Rect ndst = { 52, 490, ns->w, ns->h };
            SDL_RenderCopy(renderer, nt, NULL, &ndst);
            SDL_FreeSurface(ns);
            SDL_DestroyTexture(nt);
        }
    }

    if (node->choice_count > 0) {
        // Choice header
        SDL_Color headerColor = {200, 200, 255, 255};
        SDL_Surface* hs = TTF_RenderText_Blended(font, node->text, headerColor);
        if (hs) {
            SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
            SDL_Rect hdst = { 80, 522, hs->w, hs->h };
            SDL_RenderCopy(renderer, ht, NULL, &hdst);
            SDL_FreeSurface(hs);
            SDL_DestroyTexture(ht);
        }

        // Choice options
        for (int i = 0; i < node->choice_count; i++) {
            char label[MAX_TEXT_LEN + 8];
            snprintf(label, sizeof(label), "%d. %s",
                     i + 1, node->choice_text[i]);
            SDL_Color color = {220, 220, 255, 255};
            SDL_Surface* surf = TTF_RenderText_Blended(font, label, color);
            if (surf) {
                SDL_Texture* tex = SDL_CreateTextureFromSurface(renderer, surf);
                SDL_Rect dst = { 100, 554 + i * 38, surf->w, surf->h };
                SDL_RenderCopy(renderer, tex, NULL, &dst);
                SDL_FreeSurface(surf);
                SDL_DestroyTexture(tex);
            }
        }

        SDL_Color hint = {120, 120, 180, 255};
        SDL_Surface* hs2 = TTF_RenderText_Blended(font,
                           "Press 1-4 to choose", hint);
        if (hs2) {
            SDL_Texture* ht2 = SDL_CreateTextureFromSurface(renderer, hs2);
            SDL_Rect hdst2 = { 80, 672, hs2->w, hs2->h };
            SDL_RenderCopy(renderer, ht2, NULL, &hdst2);
            SDL_FreeSurface(hs2);
            SDL_DestroyTexture(ht2);
        }
    } else {
        // Main dialogue text
        SDL_Color textColor = {220, 220, 255, 255};
        SDL_Surface* ts = TTF_RenderText_Blended(font, node->text, textColor);
        if (ts) {
            SDL_Texture* tt = SDL_CreateTextureFromSurface(renderer, ts);
            SDL_Rect tdst = { 80, 526, ts->w, ts->h };
            SDL_RenderCopy(renderer, tt, NULL, &tdst);
            SDL_FreeSurface(ts);
            SDL_DestroyTexture(tt);
        }

        SDL_Color hint = {120, 120, 180, 255};
        SDL_Surface* hs = TTF_RenderText_Blended(font,
                          "Press E to continue...", hint);
        if (hs) {
            SDL_Texture* ht = SDL_CreateTextureFromSurface(renderer, hs);
            SDL_Rect hdst = { 80, 672, hs->w, hs->h };
            SDL_RenderCopy(renderer, ht, NULL, &hdst);
            SDL_FreeSurface(hs);
            SDL_DestroyTexture(ht);
        }
    }
}

// ── Keyed conversation system (Phase 1) ──────────────────────────────────────

Conversation* dialogue_db_find(DialogueDB* db, const char* key) {
    if (!db || !key) return NULL;
    for (int i = 0; i < db->count; i++) {
        if (strcmp(db->convos[i].key, key) == 0)
            return &db->convos[i];
    }
    return NULL;
}

void dialogue_start_key(DialogueSystem* ds, const char* key) {
    Conversation* c = dialogue_db_find(&g_dialogue_db, key);
    if (!c) {
        printf("dialogue_start_key: conversation '%s' not found\n",
               key ? key : "(null)");
        return;
    }
    int n = c->node_count;
    if (n > MAX_NODES_PER_CONVO) n = MAX_NODES_PER_CONVO;
    memcpy(ds->keyed_nodes, c->nodes, sizeof(DialogueNode) * n);
    ds->keyed_node_count = n;
    ds->using_keyed      = 1;   // play from keyed buffer
    ds->current_node     = 0;
    ds->active           = 1;

    fprintf(stderr, "START_KEY: '%s' loaded %d nodes, node0.next=%d\n",
            key, ds->keyed_node_count, ds->keyed_nodes[0].next);
}