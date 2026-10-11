#include "notify.h"
#include "activities.h"
#include "audio.h"
#include <string.h>
#include <stdio.h>

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

void activitydb_init(ActivityDB* db) {
    db->count = 0;

    #define ALL_SLOTS  {1,1,1,1,1,1}
    #define EVE_NIGHT  {0,0,0,0,1,1}
    #define AFT_EVE    {0,0,0,1,1,0}
    #define NIGHT_ONLY {0,0,0,0,0,1}

    // Study
    Activity study = {0};
    strncpy(study.name,        "Study",          63);
    strncpy(study.description, "Hit the books and improve your Knowledge.", 127);
    strncpy(study.location,    "Library",        63);
    int study_slots[] = ALL_SLOTS;
    memcpy(study.available_slots, study_slots, sizeof(study_slots));
    study.raises[0]  = SOCIAL_KNOWLEDGE;
    study.amounts[0] = 3;
    study.gain_count = 1;
    study.req_count  = 0;
    study.yen_reward = 0;
    activitydb_add(db, study);

    // Read Book
    Activity read = {0};
    strncpy(read.name,        "Read Book",       63);
    strncpy(read.description, "Read to expand your Knowledge.", 127);
    strncpy(read.location,    "Bedroom",         63);
    int read_slots[] = EVE_NIGHT;
    memcpy(read.available_slots, read_slots, sizeof(read_slots));
    read.raises[0]  = SOCIAL_KNOWLEDGE;
    read.amounts[0] = 2;
    read.gain_count = 1;
    read.req_count  = 0;
    read.yen_reward = 0;
    activitydb_add(db, read);

    // Watch TV
    Activity tv = {0};
    strncpy(tv.name,        "Watch TV",          63);
    strncpy(tv.description, "Relax and pick up some Charm.", 127);
    strncpy(tv.location,    "Living Room",       63);
    int tv_slots[] = EVE_NIGHT;
    memcpy(tv.available_slots, tv_slots, sizeof(tv_slots));
    tv.raises[0]  = SOCIAL_CHARM;
    tv.amounts[0] = 2;
    tv.gain_count = 1;
    tv.req_count  = 0;
    tv.yen_reward = 0;
    activitydb_add(db, tv);

    // Take Bath
    Activity bath = {0};
    strncpy(bath.name,        "Take Bath",       63);
    strncpy(bath.description, "A relaxing bath to improve your Charm.", 127);
    strncpy(bath.location,    "Bathhouse",       63);
    int bath_slots[] = EVE_NIGHT;
    memcpy(bath.available_slots, bath_slots, sizeof(bath_slots));
    bath.raises[0]  = SOCIAL_CHARM;
    bath.amounts[0] = 3;
    bath.gain_count = 1;
    bath.req_count  = 0;
    bath.yen_reward = 0;
    activitydb_add(db, bath);

    // Train
    Activity train = {0};
    strncpy(train.name,        "Train",          63);
    strncpy(train.description, "Push your limits and build Guts.", 127);
    strncpy(train.location,    "Gym",            63);
    int train_slots[] = AFT_EVE;
    memcpy(train.available_slots, train_slots, sizeof(train_slots));
    train.raises[0]  = SOCIAL_GUTS;
    train.amounts[0] = 3;
    train.gain_count = 1;
    train.req_count  = 0;
    train.yen_reward = 0;
    activitydb_add(db, train);

    // ── Part-time Job — Convenience Store ────────────────────────────────────
    // Earns yen + raises Kindness and Proficiency
    Activity job = {0};
    strncpy(job.name,        "Part-time Job",     63);
    strncpy(job.description,
            "Work the evening shift. Earn 3,500 yen + Kindness and Proficiency.", 127);
    strncpy(job.location,    "Convenience Store", 63);
    int job_slots[] = EVE_NIGHT;
    memcpy(job.available_slots, job_slots, sizeof(job_slots));
    job.raises[0]  = SOCIAL_KINDNESS;
    job.amounts[0] = 2;
    job.raises[1]  = SOCIAL_PROFICIENCY;
    job.amounts[1] = 2;
    job.gain_count = 2;
    job.req_count  = 0;
    job.yen_reward = 3500;
    activitydb_add(db, job);

    // ── Eat Curry ─────────────────────────────────────────────────────────────
    Activity curry = {0};
    strncpy(curry.name,        "Eat Curry",       63);
    strncpy(curry.description, "A hearty meal. Restores HP and SP.", 127);
    strncpy(curry.location,    "Cafe",         63);
    int curry_slots[] = EVE_NIGHT;
    memcpy(curry.available_slots, curry_slots, sizeof(curry_slots));
    curry.gain_count    = 0;
    curry.req_count     = 0;
    curry.restores_hp_sp = 1;
    curry.yen_reward    = 0;
    activitydb_add(db, curry);
}

void activitydb_add(ActivityDB* db, Activity act) {
    if (db->count >= MAX_ACTIVITIES) return;
    db->activities[db->count++] = act;
}

int activity_is_available(Activity* act, TimeSlot slot, SocialStats* ss) {
    if (!act->available_slots[slot]) return 0;
    for (int i = 0; i < act->req_count; i++)
        if (ss->rank[act->req_stat[i]] < act->req_rank[i]) return 0;
    return 1;
}

void activitymenu_init(ActivityMenu* am) {
    am->state    = ACTIVITY_CLOSED;
    am->selected = 0;
}

void activitymenu_open(ActivityMenu* am) {
    am->state    = ACTIVITY_OPEN;
    am->selected = 0;
}

void activitymenu_close(ActivityMenu* am) {
    am->state = ACTIVITY_CLOSED;
}

void activitymenu_handle_input(ActivityMenu* am, SDL_Event* event,
                                ActivityDB* db, SocialStats* ss,
                                TimeSystem* ts, Party* party,
                                NotifySystem* ns, Inventory* inv) {
    if (am->state == ACTIVITY_CLOSED) return;
    if (event->type != SDL_KEYDOWN) return;

    switch (event->key.keysym.sym) {
        case SDLK_UP:
            am->selected = (am->selected - 1 + db->count) % db->count;
            audio_play_sfx(SFX_MENU_MOVE);
            break;
        case SDLK_DOWN:
            am->selected = (am->selected + 1) % db->count;
            audio_play_sfx(SFX_MENU_MOVE);
            break;
        case SDLK_ESCAPE:
            activitymenu_close(am);
            break;
        case SDLK_e:
        case SDLK_RETURN: {
            Activity* act = &db->activities[am->selected];
            if (!activity_is_available(act, ts->slot, ss)) break;

            // Apply stat gains
            for (int i = 0; i < act->gain_count; i++) {
                int old_rank = ss->rank[act->raises[i]];
                social_stats_add_exp(ss, act->raises[i], act->amounts[i]);
                int new_rank = ss->rank[act->raises[i]];
                char msg[128];
                if (new_rank > old_rank) {
                    snprintf(msg, 128, "%s ranked up! (%s)",
                             social_stat_name(act->raises[i]),
                             social_rank_name(act->raises[i], new_rank));
                    notify_success(ns, msg);
                } else {
                    snprintf(msg, 128, "%s +%d",
                             social_stat_name(act->raises[i]),
                             act->amounts[i]);
                    notify_info(ns, msg);
                }
            }

            // Apply yen reward
            if (act->yen_reward > 0 && inv) {
                inv->yen += act->yen_reward;
                char msg[64];
                snprintf(msg, 64, "Earned %d yen!", act->yen_reward);
                notify_success(ns, msg);
            }

            // Restore HP/SP
            if (act->restores_hp_sp) {
                for (int i = 0; i < party->count; i++) {
                    party->members[i].hp = party->members[i].max_hp;
                    party->members[i].sp = party->members[i].max_sp;
                }
                notify_success(ns, "HP and SP fully restored!");
            }

            // Advance time
            if (act->advances_day)
                timesystem_sleep(ts);
            else
                timesystem_advance(ts);

            activitymenu_close(am);
            break;
        }
    }
}

void activitymenu_render(ActivityMenu* am, SDL_Renderer* renderer,
                          TTF_Font* font, ActivityDB* db,
                          SocialStats* ss, TimeSystem* ts,
                          Inventory* inv) {
    if (am->state == ACTIVITY_CLOSED) return;

    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color yellow = {255, 220, 100, 255};
    SDL_Color gray   = {150, 150, 180, 255};
    SDL_Color red    = {220,  60,  60, 255};
    SDL_Color green  = {100, 220, 100, 255};
    SDL_Color gold   = {255, 200,  50, 255};

    draw_rect(renderer, 0, 0, 1280, 720, 0, 0, 0, 180);
    draw_rect(renderer,   160, 60, 960, 600, 10, 10, 30, 255);
    draw_border(renderer, 160, 60, 960, 600, 80, 80, 160, 255);

    draw_text(renderer, font, "Activities", yellow, 580, 72);

    // Time display
    char timestr[64];
    snprintf(timestr, 64, "%s  %s",
             dayofweek_name(ts->day_of_week),
             timeslot_name(ts->slot));
    draw_text(renderer, font, timestr, gray, 180, 72);

    // Yen display
    if (inv) {
        char yen_str[32];
        snprintf(yen_str, 32, "%d yen", inv->yen);
        draw_text(renderer, font, yen_str, gold, 920, 72);
    }

    // Column headers
    draw_text(renderer, font, "Activity",  gray, 180, 108);
    draw_text(renderer, font, "Location",  gray, 440, 108);
    draw_text(renderer, font, "Gains",     gray, 660, 108);
    draw_text(renderer, font, "Pay",       gray, 930, 108);
    draw_rect(renderer, 180, 130, 920, 1, 80, 80, 160, 255);

    for (int i = 0; i < db->count; i++) {
        Activity* act = &db->activities[i];
        int ay = 140 + i * 38;
        int available = activity_is_available(act, ts->slot, ss);

        if (i == am->selected)
            draw_rect(renderer, 180, ay - 2, 920, 34, 40, 40, 80, 255);

        SDL_Color col = !available ? (SDL_Color){100,100,100,255} :
                        i == am->selected ? yellow : white;

        char label[80];
        snprintf(label, 80, "%s %s", i == am->selected ? ">" : " ", act->name);
        draw_text(renderer, font, label, col, 180, ay);
        draw_text(renderer, font, act->location, col, 440, ay);

        // Stat gains — abbreviated to fit column
        char gains[64] = "";
        for (int j = 0; j < act->gain_count; j++) {
            char buf[24];
            // Use short stat names to save space
            const char* sname = social_stat_name(act->raises[j]);
            char short_name[8];
            strncpy(short_name, sname, 4);
            short_name[4] = '\0';
            snprintf(buf, 24, "%s+%d ", short_name, act->amounts[j]);
            strncat(gains, buf, sizeof(gains) - strlen(gains) - 1);
        }
        if (act->restores_hp_sp)
            strncat(gains, "HP/SP", sizeof(gains) - strlen(gains) - 1);
        draw_text(renderer, font, gains,
                  available ? green : gray, 660, ay);

        // Yen reward
        if (act->yen_reward > 0) {
            char pay[32];
            snprintf(pay, 32, "+%d", act->yen_reward);
            draw_text(renderer, font, pay,
                      available ? gold : gray, 930, ay);
        }

        // Unavailability reason
        if (!available && i == am->selected) {
            if (!act->available_slots[ts->slot])
                draw_text(renderer, font,
                          "Not available at this time", red, 180, ay + 20);
            else {
                for (int j = 0; j < act->req_count; j++) {
                    char req[64];
                    snprintf(req, 64, "Requires %s rank %d",
                             social_stat_name(act->req_stat[j]),
                             act->req_rank[j]);
                    draw_text(renderer, font, req, red, 180, ay + 20);
                }
            }
        }
    }

    if (am->selected < db->count) {
        Activity* act = &db->activities[am->selected];
        draw_rect(renderer, 180, 580, 920, 1, 80, 80, 160, 255);
        draw_text(renderer, font, act->description, gray, 180, 590);
    }

    draw_text(renderer, font, "E: do activity  Esc: close",
              gray, 180, 622);
}