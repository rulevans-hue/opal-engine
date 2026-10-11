#include "bond.h"
#include <string.h>
#include <stdio.h>
#include "summon.h"
#include "bond_loader.h"

static void draw_text(SDL_Renderer* r, TTF_Font* font,
                      const char* text, SDL_Color color, int x, int y) {
    if (!text || !text[0]) return;
    SDL_Surface* surf = TTF_RenderText_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_Rect dst = { x, y, surf->w, surf->h };
    SDL_RenderCopy(r, tex, NULL, &dst);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

static void draw_rect(SDL_Renderer* r, int x, int y, int w, int h,
                      Uint8 rr, Uint8 g, Uint8 b, Uint8 a) {
    SDL_SetRenderDrawColor(r, rr, g, b, a);
    SDL_Rect rect = { x, y, w, h };
    SDL_RenderFillRect(r, &rect);
}

static void draw_border(SDL_Renderer* r, int x, int y, int w, int h,
                        Uint8 rr, Uint8 g, Uint8 b, Uint8 a) {
    SDL_SetRenderDrawColor(r, rr, g, b, a);
    SDL_Rect rect = { x, y, w, h };
    SDL_RenderDrawRect(r, &rect);
}

// ─── Perk helpers ────────────────────────────────────────────────────────────

int bond_get_shop_discount(BondDB* db) {
    int best = 0;
    for (int i = 0; i < db->count; i++) {
        Bond* c = &db->bonds[i];
        for (int r = 0; r < c->current_rank; r++) {
            if (c->ranks[r].perk.type == PERK_SHOP_DISCOUNT)
                if (c->ranks[r].perk.value > best)
                    best = c->ranks[r].perk.value;
        }
    }
    return best;
}

int bond_get_exp_bonus(BondDB* db) {
    int total = 0;
    for (int i = 0; i < db->count; i++) {
        Bond* c = &db->bonds[i];
        for (int r = 0; r < c->current_rank; r++) {
            if (c->ranks[r].perk.type == PERK_EXP_BONUS)
                total += c->ranks[r].perk.value;
        }
    }
    return total;
}

int bond_has_perk(BondDB* db, PerkType perk) {
    for (int i = 0; i < db->count; i++) {
        Bond* c = &db->bonds[i];
        for (int r = 0; r < c->current_rank; r++) {
            if (c->ranks[r].perk.type == perk) return 1;
        }
    }
    return 0;
}

int bond_get_perk_value(BondDB* db, PerkType perk) {
    int best = 0;
    for (int i = 0; i < db->count; i++) {
        Bond* c = &db->bonds[i];
        for (int r = 0; r < c->current_rank; r++) {
            if (c->ranks[r].perk.type == perk)
                if (c->ranks[r].perk.value > best)
                    best = c->ranks[r].perk.value;
        }
    }
    return best;
}

// ─── Init ────────────────────────────────────────────────────────────────────

static void make_perk(Perk* p, PerkType type, int value,
                      const char* desc) {
    p->type  = type;
    p->value = value;
    strncpy(p->description, desc, 127);
}

static void make_rank(BondRank* r, int pts, PerkType ptype,
                      int pvalue, const char* pdesc,
                      const char* msg,
                      int req_stat, int req_rank) {
    r->points_required = pts;
    make_perk(&r->perk, ptype, pvalue, pdesc);
    strncpy(r->rank_up_message, msg, 127);
    r->req_stat_type = req_stat;
    r->req_stat_rank = req_rank;
}

void bonddb_init(BondDB* db) {
    db->count = 0;

    // Try data-driven load first; fall back to hardcoded below if it fails.
    if (bond_load_json(db, "data/bonds.json"))
        return;

    // ── FALLBACK: hardcoded bonds (used only if JSON load fails) ──────
    // ── Stranger (Magician) ───────────────────────────────────────────────
    {
        Bond* c = &db->bonds[db->count++];
        memset(c, 0, sizeof(Bond));
        strncpy(c->name,        "Stranger",  63);
        strncpy(c->arcana,      "Magician",  63);
        strncpy(c->description,
                "A mysterious figure who knows the town well.",
                255);
        strncpy(c->npc_name_match, "Stranger", 63);
        c->current_rank      = 0;
        c->points            = 0;
        c->max_rank          = 10;
        c->points_per_hangout = 20;
        c->active            = 1;

        // Rank 1 — auto unlock
        make_rank(&c->ranks[0], 0, PERK_NONE, 0,
                  "A bond has formed.",
                  "You've met the Stranger. A bond begins.",
                  -1, 0);
        // Rank 2
        make_rank(&c->ranks[1], 20, PERK_STEAL_CHANCE, 20,
                  "Pickpocket: 20% chance to steal items on physical attack.",
                  "The Stranger teaches you a trick.",
                  -1, 0);
        // Rank 3
        make_rank(&c->ranks[2], 50, PERK_FLEE_BONUS, 25,
                  "Smooth Talk: +25% flee success rate.",
                  "You've earned the Stranger's trust.",
                  -1, 0);
        // Rank 4
        make_rank(&c->ranks[3], 90, PERK_FOLLOWUP_ATTACK, 30,
                  "Swift Strike: 30% chance of follow-up after knockdown.",
                  "The Stranger shows you how to press an advantage.",
                  -1, 0);
        // Rank 5 — requires Guts 3
        make_rank(&c->ranks[4], 140, PERK_STEAL_CHANCE, 50,
                  "Master Pickpocket: 50% steal chance.",
                  "Your boldness impresses the Stranger.",
                  SOCIAL_GUTS, 3);
        // Rank 6
        make_rank(&c->ranks[5], 200, PERK_GUN_IGNORE_RESIST, 1,
                  "Ace Gunner: gun attacks ignore resistances.",
                  "The Stranger reveals a secret technique.",
                  -1, 0);
        // Rank 7
        make_rank(&c->ranks[6], 270, PERK_TECHNICAL_BONUS, 25,
                  "Cheap Shot: +25% technical damage.",
                  "You understand the Stranger's methods.",
                  -1, 0);
        // Rank 8 — requires Guts 4
        make_rank(&c->ranks[7], 350, PERK_FOLLOWUP_ATTACK, 60,
                  "Swift Strike+: 60% follow-up chance.",
                  "The Stranger trusts you completely.",
                  SOCIAL_GUTS, 4);
        // Rank 9
        make_rank(&c->ranks[8], 440, PERK_NONE, 0,
                  "Unshackled: immunity to Forget and Confuse.",
                  "You've seen through all illusions.",
                  -1, 0);
        // Rank 10
        make_rank(&c->ranks[9], 540, PERK_EXTRA_TURN_CHANCE, 15,
                  "Wild Card: 15% chance to act again after any action.",
                  "Your bond with the Stranger is complete.",
                  -1, 0);
    }

    // ── Shopkeeper (Death/Takemi) ─────────────────────────────────────────
    {
        Bond* c = &db->bonds[db->count++];
        memset(c, 0, sizeof(Bond));
        strncpy(c->name,        "Shopkeeper", 63);
        strncpy(c->arcana,      "Death",      63);
        strncpy(c->description,
                "A skilled merchant with rare medical knowledge.",
                255);
        strncpy(c->npc_name_match, "Shopkeeper", 63);
        c->current_rank       = 0;
        c->points             = 0;
        c->max_rank           = 10;
        c->points_per_hangout = 20;
        c->active             = 1;

        make_rank(&c->ranks[0], 0, PERK_NONE, 0,
                  "A business relationship begins.",
                  "You've become a customer at the clinic.",
                  -1, 0);
        make_rank(&c->ranks[1], 20, PERK_SHOP_DISCOUNT, 10,
                  "Discount: 10% off all items.",
                  "The Shopkeeper offers you a deal.",
                  -1, 0);
        make_rank(&c->ranks[2], 50, PERK_UNLOCK_ITEMS, 1,
                  "New Stock: revival items now available.",
                  "New medicines arrive in stock.",
                  -1, 0);
        make_rank(&c->ranks[3], 90, PERK_SHOP_DISCOUNT, 20,
                  "Better Discount: 20% off all items.",
                  "The Shopkeeper values your patronage.",
                  SOCIAL_CHARM, 3);
        make_rank(&c->ranks[4], 140, PERK_UNLOCK_ITEMS, 2,
                  "New Stock 2: SP items now available.",
                  "Experimental medicines become available.",
                  -1, 0);
        make_rank(&c->ranks[5], 200, PERK_ITEM_POTENCY, 25,
                  "Treatment: items restore 25% more HP/SP.",
                  "The Shopkeeper improves her formulas.",
                  -1, 0);
        make_rank(&c->ranks[6], 270, PERK_SHOP_DISCOUNT, 30,
                  "Better Discount 2: 30% off all items.",
                  "You are her most trusted customer.",
                  SOCIAL_CHARM, 4);
        make_rank(&c->ranks[7], 350, PERK_UNLOCK_ITEMS, 3,
                  "New Stock 3: rare items now available.",
                  "Her finest work becomes available.",
                  -1, 0);
        make_rank(&c->ranks[8], 440, PERK_ITEM_POTENCY, 50,
                  "Full Treatment: items restore 50% more HP/SP.",
                  "Her medicine reaches its full potential.",
                  -1, 0);
        make_rank(&c->ranks[9], 540, PERK_UNLOCK_ITEMS, 4,
                  "Soma now available in the shop.",
                  "Your bond with the Shopkeeper is complete.",
                  -1, 0);
    }

    // ── Hooded Figure (Moon) ──────────────────────────────────────────────
    {
        Bond* c = &db->bonds[db->count++];
        memset(c, 0, sizeof(Bond));
        strncpy(c->name,        "Hooded Figure", 63);
        strncpy(c->arcana,      "Moon",          63);
        strncpy(c->description,
                "A shadowy figure who lurks in the back alleys.",
                255);
        strncpy(c->npc_name_match, "Hooded Figure", 63);
        c->current_rank       = 0;
        c->points             = 0;
        c->max_rank           = 10;
        c->points_per_hangout = 20;
        c->active             = 1;

        make_rank(&c->ranks[0], 0, PERK_NONE, 0,
                  "A strange connection forms.",
                  "You've met the Hooded Figure.",
                  -1, 0);
        make_rank(&c->ranks[1], 20, PERK_STATUS_DURATION, 1,
                  "Enlargement: status effects last 1 extra turn.",
                  "The Figure teaches you to exploit weakness.",
                  -1, 0);
        make_rank(&c->ranks[2], 50, PERK_EXP_BONUS, 10,
                  "Poet of Kirijo: +10% EXP from battles.",
                  "The Figure shares forbidden knowledge.",
                  SOCIAL_KNOWLEDGE, 2);
        make_rank(&c->ranks[3], 90, PERK_TECHNICAL_BONUS, 25,
                  "Hollow Haze: +25% technical damage.",
                  "You understand the Figure's methods.",
                  -1, 0);
        make_rank(&c->ranks[4], 140, PERK_EXP_BONUS, 25,
                  "Lunatic: +25% EXP from battles.",
                  "The Figure's teachings deepen.",
                  -1, 0);
        make_rank(&c->ranks[5], 200, PERK_STATUS_DURATION, 2,
                  "Enlargement+: status effects last 2 extra turns.",
                  "Your mastery of status grows.",
                  SOCIAL_KNOWLEDGE, 3);
        make_rank(&c->ranks[6], 270, PERK_TECHNICAL_BONUS, 50,
                  "Distorted Mind: +50% technical damage.",
                  "You see the patterns the Figure sees.",
                  -1, 0);
        make_rank(&c->ranks[7], 350, PERK_STATUS_NO_SP, 1,
                  "Distorted Thinking: status skills cost no SP.",
                  "The Figure's secrets are yours.",
                  -1, 0);
        make_rank(&c->ranks[8], 440, PERK_EXP_BONUS, 50,
                  "Lunatic 2: +50% EXP from battles.",
                  "Knowledge flows freely between you.",
                  -1, 0);
        make_rank(&c->ranks[9], 540, PERK_STATUS_ALWAYS_HIT, 1,
                  "Frenzied Mind: status skills always hit.",
                  "Your bond with the Hooded Figure is complete.",
                  -1, 0);
    }
}

// ─── Hangout ─────────────────────────────────────────────────────────────────

int bond_can_hangout(Bond* c, SocialStats* ss) {
    if (!c->active) return 0;
    if (c->current_rank >= c->max_rank) return 0;

    // Check if next rank has stat requirement
    int next = c->current_rank;
    if (next < c->max_rank) {
        BondRank* r = &c->ranks[next];
        if (r->req_stat_type >= 0) {
            if (ss->rank[r->req_stat_type] < r->req_stat_rank)
                return 0;
        }
    }
    return 1;
}

// Reusable rank-up check — advances ranks while points suffice.
void bond_check_rankup(Bond* c, NotifySystem* ns) {
    char msg[128];
    while (c->current_rank < c->max_rank) {
        BondRank* next_rank = &c->ranks[c->current_rank];
        if (c->points >= next_rank->points_required) {
            c->current_rank++;
            if (ns) {
                snprintf(msg, 128, "%s - Rank %d!", c->name, c->current_rank);
                notify_success(ns, msg);
                notify_info(ns, next_rank->perk.description);
            }
        } else {
            break;
        }
    }
}

// Arcana bonus: if Prota holds a Summon of the bond's Arcana,
// points are multiplied by x1.5. Returns the scaled point value.
static int bond_apply_arcana_bonus(Bond* c, int points) {
    extern SummonStock g_summon_stock;
    if (!c || points <= 0) return points;
    for (int i = 0; i < g_summon_stock.count; i++) {
        const char* pa = arcana_name(g_summon_stock.stock[i].arcana);
        if (pa && strcmp(pa, c->arcana) == 0) {
            return points * 3 / 2;   // x1.5, integer math
        }
    }
    return points;
}

// Award points to a bond identified by NPC-match name (dialogue choices).
void bond_award_points_by_name(BondDB* db, const char* name,
                                     int points, NotifySystem* ns) {
    if (!db || !name || !name[0] || points == 0) return;
    for (int i = 0; i < db->count; i++) {
        Bond* c = &db->bonds[i];
        if (!c->active) continue;
        if (strcmp(c->npc_name_match, name) == 0 ||
            strcmp(c->name, name) == 0) {
            int awarded = bond_apply_arcana_bonus(c, points);
            c->points += awarded;
            (void)ns;
            return;
        }
    }
}

Bond* bond_find_by_name(BondDB* db, const char* name) {
    if (!db || !name || !name[0]) return NULL;
    for (int i = 0; i < db->count; i++) {
        Bond* c = &db->bonds[i];
        if (strcmp(c->npc_name_match, name) == 0 ||
            strcmp(c->name, name) == 0) {
            return c;
        }
    }
    return NULL;
}

// Apply a single rank-up (the rank-up scene completed). Advances ONE rank
// and fires the perk notification. Points are NOT reset (they carry
// forward toward the next rank).
void bond_apply_rankup(Bond* c, NotifySystem* ns) {
    if (!c || c->current_rank >= c->max_rank) return;
    BondRank* reached = &c->ranks[c->current_rank];
    c->current_rank++;
    if (ns) {
        char msg[128];
        snprintf(msg, 128, "%s - Rank %d!", c->name, c->current_rank);
        notify_success(ns, msg);
        notify_info(ns, reached->perk.description);
    }
}

// Called when a dialogue conversation ends. Any bond flagged with a
// rank-up scene in progress now advances its rank (the scene delivered it).
void bond_resolve_pending_rankups(BondDB* db, NotifySystem* ns) {
    if (!db) return;
    for (int i = 0; i < db->count; i++) {
        Bond* c = &db->bonds[i];
        if (c->rankup_in_progress) {
            c->rankup_in_progress = 0;
            bond_apply_rankup(c, ns);
        }
    }
}

void bond_hangout(Bond* c, SocialStats* ss,
                       NotifySystem* ns, TimeSystem* ts) {
    if (!bond_can_hangout(c, ss)) return;

    c->points += c->points_per_hangout;

    char msg[128];
    snprintf(msg, 128, "Spent time with %s.", c->name);
    notify_info(ns, msg);

    // Check for rank up (shared logic)
    bond_check_rankup(c, ns);

    // Costs a time slot
    timesystem_advance(ts);
}

// ─── Menu ─────────────────────────────────────────────────────────────────────

void bondmenu_init(BondMenu* cm) {
    cm->state    = BOND_MENU_CLOSED;
    cm->selected = 0;
    cm->pending  = 0;
}

void bondmenu_open(BondMenu* cm) {
    cm->state    = BOND_MENU_LIST;
    cm->selected = 0;
}

void bondmenu_close(BondMenu* cm) {
    cm->state = BOND_MENU_CLOSED;
}

void bondmenu_handle_input(BondMenu* cm, SDL_Event* event,
                                 BondDB* db, SocialStats* ss,
                                 NotifySystem* ns, TimeSystem* ts) {
    if (cm->state == BOND_MENU_CLOSED) return;
    if (event->type != SDL_KEYDOWN) return;

    SDL_Keycode key = event->key.keysym.sym;

    if (cm->state == BOND_MENU_LIST) {
        switch (key) {
            case SDLK_UP:
                cm->selected =
                    (cm->selected - 1 + db->count) % db->count;
                break;
            case SDLK_DOWN:
                cm->selected =
                    (cm->selected + 1) % db->count;
                break;
            case SDLK_ESCAPE:
                bondmenu_close(cm);
                break;
            case SDLK_e:
            case SDLK_RETURN:
                cm->state = BOND_MENU_DETAIL;
                break;
        }
    } else if (cm->state == BOND_MENU_DETAIL) {
        switch (key) {
            case SDLK_ESCAPE:
                cm->state = BOND_MENU_LIST;
                break;
            case SDLK_e:
            case SDLK_RETURN: {
                Bond* c = &db->bonds[cm->selected];
                if (bond_can_hangout(c, ss)) {
                    bond_hangout(c, ss, ns, ts);
                    cm->state = BOND_MENU_LIST;
                } else {
                    notify_warning(ns, "Can't hang out right now.");
                }
                break;
            }
        }
    }
}

void bondmenu_render(BondMenu* cm, SDL_Renderer* renderer,
                           TTF_Font* font, BondDB* db,
                           SocialStats* ss) {
    if (cm->state == BOND_MENU_CLOSED) return;

    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color yellow = {255, 220, 100, 255};
    SDL_Color gray   = {150, 150, 180, 255};
    SDL_Color green  = {100, 220, 100, 255};
    SDL_Color red    = {220,  60,  60, 255};

    draw_rect(renderer, 0, 0, 1280, 720, 0, 0, 0, 180);

    if (cm->state == BOND_MENU_LIST) {
        draw_rect(renderer,   160, 60, 960, 600, 10, 10, 30, 255);
        draw_border(renderer, 160, 60, 960, 600, 80, 80, 160, 255);
        draw_text(renderer, font, "Bonds", yellow, 570, 72);

        for (int i = 0; i < db->count; i++) {
            Bond* c = &db->bonds[i];
            int cy = 120 + i * 90;
            int cx = 180;

            SDL_Color bc = (i == cm->selected) ?
                           (SDL_Color){40, 40, 80, 255} :
                           (SDL_Color){15, 15, 40, 255};
            draw_rect(renderer,   cx, cy, 920, 80,
                      bc.r, bc.g, bc.b, 255);
            draw_border(renderer, cx, cy, 920, 80,
                        i == cm->selected ? 180 : 60,
                        i == cm->selected ? 180 : 60,
                        i == cm->selected ? 255 : 120,
                        255);

            // Name and arcana
            char header[80];
            snprintf(header, 80, "%s  [%s]", c->name, c->arcana);
            draw_text(renderer, font, header, yellow, cx + 10, cy + 8);

            // Rank bar
            draw_text(renderer, font, "Rank:", gray, cx + 10, cy + 36);
            for (int r = 0; r < c->max_rank; r++) {
                SDL_Color rc = r < c->current_rank ? green : gray;
                draw_rect(renderer,
                          cx + 70 + r * 36, cy + 38,
                          30, 14,
                          rc.r, rc.g, rc.b, 255);
            }

            // Can hang out?
            if (c->current_rank >= c->max_rank) {
                draw_text(renderer, font, "MAX", green,
                          cx + 480, cy + 36);
            } else if (bond_can_hangout(c, ss)) {
                draw_text(renderer, font, "Available", green,
                          cx + 480, cy + 36);
            } else {
                // Show requirement
                int next = c->current_rank;
                BondRank* nr = &c->ranks[next];
                if (nr->req_stat_type >= 0) {
                    char req[64];
                    snprintf(req, 64, "Needs %s rank %d",
                             social_stat_name(
                                 (SocialStatType)nr->req_stat_type),
                             nr->req_stat_rank);
                    draw_text(renderer, font, req, red,
                              cx + 480, cy + 36);
                }
            }

            // Points to next rank
            if (c->current_rank < c->max_rank) {
                int next_pts = c->ranks[c->current_rank].points_required;
                char pts[64];
                snprintf(pts, 64, "%d / %d pts",
                         c->points, next_pts);
                draw_text(renderer, font, pts, gray,
                          cx + 700, cy + 36);
            }
        }

        draw_text(renderer, font, "E: view details  Esc: close",
                  gray, 180, 630);

    } else if (cm->state == BOND_MENU_DETAIL) {
        Bond* c = &db->bonds[cm->selected];

        draw_rect(renderer,   160, 60, 960, 600, 10, 10, 30, 255);
        draw_border(renderer, 160, 60, 960, 600, 80, 80, 160, 255);

        char header[80];
        snprintf(header, 80, "%s  [%s]", c->name, c->arcana);
        draw_text(renderer, font, header, yellow, 180, 72);
        draw_text(renderer, font, c->description, gray, 180, 100);

        // Rank list
        for (int r = 0; r < c->max_rank; r++) {
            int ry = 130 + r * 46;
            BondRank* rank = &c->ranks[r];

            SDL_Color rc = r < c->current_rank  ? green :
                           r == c->current_rank ? yellow : gray;

            char rank_label[8];
            snprintf(rank_label, 8, "Rank %d", r + 1);
            draw_text(renderer, font, rank_label, rc, 180, ry);

            if (rank->perk.type != PERK_NONE)
                draw_text(renderer, font,
                          rank->perk.description, rc, 280, ry);

            if (rank->req_stat_type >= 0) {
                char req[64];
                snprintf(req, 64, "(Req: %s %d)",
                         social_stat_name(
                             (SocialStatType)rank->req_stat_type),
                         rank->req_stat_rank);
                draw_text(renderer, font, req, red, 880, ry);
            }
        }

        // Hang out button
        if (bond_can_hangout(c, ss)) {
            draw_rect(renderer,   480, 610, 320, 40,
                      40, 100, 40, 255);
            draw_border(renderer, 480, 610, 320, 40,
                        60, 200, 60, 255);
            draw_text(renderer, font, "E: Hang Out (costs 1 time slot)",
                      green, 490, 620);
        } else if (c->current_rank >= c->max_rank) {
            draw_text(renderer, font, "Bond complete!", green, 530, 620);
        } else {
            draw_text(renderer, font,
                      "Requirements not met.", red, 530, 620);
        }

        draw_text(renderer, font, "Esc: back", gray, 180, 630);
    }
}