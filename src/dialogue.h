#ifndef DIALOGUE_H
#define DIALOGUE_H

#ifdef OPAL_GDEXTENSION
#include "sdl_compat.h"
#else
#include <SDL2/SDL.h>
#include <SDL2/SDL_ttf.h>
#endif
#include "gamestate.h"
#include "bond.h"
#include "notify.h"

#define MAX_CONVERSATIONS      512
#define MAX_NODES_PER_CONVO    32
#define CONVO_KEY_LEN          64
#define MAX_TEXT_LEN  256
#define MAX_CHOICES   4
#define MAX_NODES     64

// ── Choice / node effects (Phase 2 uses these; defined now to avoid
//    re-shaping structs later). All fields optional; empty = no-op. ──
typedef struct {
    char set_flag[FLAG_NAME_LEN];   // set a gamestate flag
    int  set_flag_value;
    char award_bond[64];       // award points to this bond
    int  award_points;
    char goto_conversation[CONVO_KEY_LEN]; // jump to another conversation
    int  open_shop;                 // 1 = open shop
    int  start_combat;              // 1 = end dialogue, begin combat
} DialogueChoiceEffect;

typedef struct {
    char speaker[64];
    char text[MAX_TEXT_LEN];
    int  choice_count;
    char choice_text[MAX_CHOICES][MAX_TEXT_LEN];
    int  choice_next[MAX_CHOICES];

    DialogueChoiceEffect choice_effect[MAX_CHOICES];  // per-choice effects
    DialogueChoiceEffect node_effect;                 // effect for linear nodes

    // Flag to SET when this node is reached (empty = none)
    char sets_flag[FLAG_NAME_LEN];
    int  sets_flag_value;

    int  next; // -1 = end
    int  offers_hangout;    // 1 = show hang out option after this node
    char bond_name[64]; // which bond to hang out with
} DialogueNode;

typedef struct {
    DialogueNode nodes[MAX_NODES];
    int node_count;
    DialogueNode keyed_nodes[MAX_NODES_PER_CONVO];  // playback buffer for keyed convos
    int          keyed_node_count;
    int          using_keyed;   // 0 = flat (ds->nodes), 1 = keyed (keyed_nodes)
    int current_node;
    int active;
    int hangout_pending;   // set when a bond hangout starts; bridge advances time on end
} DialogueSystem;

// ── Keyed conversation containers (Phase 1) ──────────────────────────────────
// A Conversation is a self-contained dialogue tree addressed by string key.
// Node indices inside it are LOCAL (next:1 = node 1 of THIS conversation).
typedef struct {
    char         key[CONVO_KEY_LEN];
    DialogueNode nodes[MAX_NODES_PER_CONVO];
    int          node_count;
} Conversation;

typedef struct {
    Conversation convos[MAX_CONVERSATIONS];
    int          count;
} DialogueDB;

void dialogue_init(DialogueSystem* ds, GameState* gs);
void dialogue_start(DialogueSystem* ds, int start_node);
void dialogue_advance(DialogueSystem* ds, int choice, GameState* gs,
                      BondDB* cdb, NotifySystem* ns);
void dialogue_render(SDL_Renderer* renderer, DialogueSystem* ds, TTF_Font* font);

// ── Keyed conversation system (Phase 1) ──────────────────────────────────────
extern DialogueDB g_dialogue_db;
Conversation* dialogue_db_find(DialogueDB* db, const char* key);
void dialogue_start_key(DialogueSystem* ds, const char* key);

#endif