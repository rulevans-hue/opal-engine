#include "status_screen.h"
#include <string.h>
#include <stdio.h>

// ── Drawing helpers ───────────────────────────────────────────────────────────

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

static void draw_stat_bar(SDL_Renderer* r, int x, int y, int w,
                           int val, int max,
                           Uint8 rr, Uint8 g, Uint8 b) {
    draw_rect(r, x, y, w, 8, 20, 20, 40, 255);
    if (max > 0) {
        int fill = (val * w) / max;
        draw_rect(r, x, y, fill, 8, rr, g, b, 255);
    }
}

// ── Init / open / close ───────────────────────────────────────────────────────

void statusscreen_init(StatusScreen* ss) {
    ss->state           = STATUS_CLOSED;
    ss->selected_member = 0;
    ss->selected_skill  = 0;
    ss->selected_slot   = 0;
    ss->browsing_equip  = 0;
    ss->equip_scroll    = 0;
    ss->selected_item   = 0;
    ss->scroll          = 0;
}

void statusscreen_open(StatusScreen* ss) {
    ss->state           = STATUS_MAIN;
    ss->selected_member = 0;
    ss->scroll          = 0;
}

void statusscreen_close(StatusScreen* ss) {
    ss->state = STATUS_CLOSED;
}

// ── Input ─────────────────────────────────────────────────────────────────────

void statusscreen_handle_input(StatusScreen* ss, SDL_Event* event,
                                Party* party, EquipDB* equipdb) {
    if (ss->state == STATUS_CLOSED) return;
    if (event->type != SDL_KEYDOWN) return;

    SDL_Keycode key = event->key.keysym.sym;

    // ── Overview ─────────────────────────────────────────────────────────────
    if (ss->state == STATUS_MAIN) {
        switch (key) {
            case SDLK_LEFT:
                ss->selected_member =
                    (ss->selected_member - 1 + party->count) % party->count;
                break;
            case SDLK_RIGHT:
                ss->selected_member =
                    (ss->selected_member + 1) % party->count;
                break;
            case SDLK_e:
            case SDLK_RETURN:
                ss->state          = STATUS_DETAIL;
                ss->selected_skill = 0;
                break;
            case SDLK_ESCAPE:
                statusscreen_close(ss);
                break;
        }

    // ── Detail ───────────────────────────────────────────────────────────────
    } else if (ss->state == STATUS_DETAIL) {
        switch (key) {
            case SDLK_LEFT:
                ss->selected_member =
                    (ss->selected_member - 1 + party->count) % party->count;
                break;
            case SDLK_RIGHT:
                ss->selected_member =
                    (ss->selected_member + 1) % party->count;
                break;
            case SDLK_e:
            case SDLK_RETURN:
                ss->state          = STATUS_SKILLS;
                ss->selected_skill = 0;
                break;
            case SDLK_q:
                // Q goes to equip screen
                ss->state          = STATUS_EQUIP;
                ss->selected_slot  = 0;
                ss->browsing_equip = 0;
                break;
            case SDLK_ESCAPE:
                ss->state = STATUS_MAIN;
                break;
        }

    // ── Skills ───────────────────────────────────────────────────────────────
    } else if (ss->state == STATUS_SKILLS) {
        StatBlock* m = &party->members[ss->selected_member];
        switch (key) {
            case SDLK_UP:
                ss->selected_skill =
                    (ss->selected_skill - 1 + m->skill_count)
                    % m->skill_count;
                break;
            case SDLK_DOWN:
                ss->selected_skill =
                    (ss->selected_skill + 1) % m->skill_count;
                break;
            case SDLK_ESCAPE:
                ss->state = STATUS_DETAIL;
                break;
        }

    // ── Equip ─────────────────────────────────────────────────────────────────
    } else if (ss->state == STATUS_EQUIP) {
        StatBlock* m = &party->members[ss->selected_member];

        if (!ss->browsing_equip) {
            // Navigating slots
            switch (key) {
                case SDLK_UP:
                    ss->selected_slot =
                        (ss->selected_slot - 1 + EQUIP_SLOT_COUNT)
                        % EQUIP_SLOT_COUNT;
                    break;
                case SDLK_DOWN:
                    ss->selected_slot =
                        (ss->selected_slot + 1) % EQUIP_SLOT_COUNT;
                    break;
                case SDLK_LEFT:
                    ss->selected_member =
                        (ss->selected_member - 1 + party->count)
                        % party->count;
                    break;
                case SDLK_RIGHT:
                    ss->selected_member =
                        (ss->selected_member + 1) % party->count;
                    break;
                case SDLK_e:
                case SDLK_RETURN:
                    // Open item browser for this slot
                    ss->browsing_equip = 1;
                    ss->selected_item  = 0;
                    ss->equip_scroll   = 0;
                    break;
                case SDLK_x:
                    // Unequip the selected slot
                    equip_set(equipdb, m, (EquipSlot)ss->selected_slot, -1);
                    break;
                case SDLK_ESCAPE:
                    ss->state = STATUS_DETAIL;
                    break;
            }
        } else {
            // Browsing items for a slot
            // Build a filtered list of items matching the current slot
            EquipSlot target_slot = (EquipSlot)ss->selected_slot;
            int matching[MAX_EQUIPMENT];
            int match_count = 0;
            for (int i = 0; i < equipdb->count; i++)
                if (equipdb->pieces[i].slot == target_slot)
                    matching[match_count++] = i;

            switch (key) {
                case SDLK_UP:
                    if (match_count > 0)
                        ss->selected_item =
                            (ss->selected_item - 1 + match_count)
                            % match_count;
                    break;
                case SDLK_DOWN:
                    if (match_count > 0)
                        ss->selected_item =
                            (ss->selected_item + 1) % match_count;
                    break;
                case SDLK_e:
                case SDLK_RETURN:
                    if (match_count > 0) {
                        int piece_id =
                            equipdb->pieces[matching[ss->selected_item]].id;
                        equip_set(equipdb, m, target_slot, piece_id);
                        ss->browsing_equip = 0;
                    }
                    break;
                case SDLK_ESCAPE:
                    ss->browsing_equip = 0;
                    break;
            }
        }
    }
}

// ── Render helpers ────────────────────────────────────────────────────────────

static void render_equip_screen(StatusScreen* ss, SDL_Renderer* renderer,
                                 TTF_Font* font, Party* party,
                                 EquipDB* equipdb) {
    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color yellow = {255, 220, 100, 255};
    SDL_Color gray   = {150, 150, 180, 255};
    SDL_Color green  = {100, 220, 100, 255};
    SDL_Color red    = {220,  60,  60, 255};
    SDL_Color cyan   = { 80, 200, 220, 255};

    StatBlock* m = &party->members[ss->selected_member];

    draw_rect(renderer,   40, 40, 1200, 640, 10, 10, 30, 255);
    draw_border(renderer, 40, 40, 1200, 640, 80, 80, 160, 255);

    // Header
    char header[80];
    snprintf(header, 80, "%s  —  Equipment", m->name);
    draw_text(renderer, font, header, yellow, 60, 52);

    // Member switcher hint
    draw_text(renderer, font, "L/R: switch member", gray, 900, 52);

    // ── Left panel: equipped slots ─────────────────────────────────────────
    draw_text(renderer, font, "Equipped", gray, 60, 90);
    draw_rect(renderer, 60, 110, 520, 200, 15, 15, 45, 255);
    draw_border(renderer, 60, 110, 520, 200, 60, 60, 120, 255);

    for (int s = 0; s < EQUIP_SLOT_COUNT; s++) {
        int sy = 122 + s * 58;
        int is_selected = (!ss->browsing_equip && s == ss->selected_slot);

        if (is_selected)
            draw_rect(renderer, 68, sy - 4, 504, 50, 40, 40, 80, 255);

        // Slot label
        char slot_label[32];
        snprintf(slot_label, 32, "%s%s",
                 is_selected ? "> " : "  ",
                 equip_slot_name((EquipSlot)s));
        draw_text(renderer, font, slot_label,
                  is_selected ? yellow : gray, 76, sy);

        // Equipped piece name
        EquipmentPiece* p = equip_get(equipdb, m->equipped[s]);
        if (p) {
            draw_text(renderer, font, p->name, white, 220, sy);

            // Show stat mods inline
            char mods[128] = "";
            for (int mi = 0; mi < p->mod_count; mi++) {
                char buf[32];
                int v = p->mods[mi].value;
                snprintf(buf, 32, "%s%s%d ",
                         v > 0 ? "+" : "",
                         equip_mod_name(p->mods[mi].type), v);
                strncat(mods, buf, sizeof(mods) - strlen(mods) - 1);
            }
            draw_text(renderer, font, mods, cyan, 220, sy + 22);
        } else {
            draw_text(renderer, font, "— none —", gray, 220, sy);
        }
    }

    draw_text(renderer, font, "E: change  X: unequip", gray, 76, 320);

    // ── Right panel: stat summary ──────────────────────────────────────────
    draw_text(renderer, font, "Stats", gray, 620, 90);
    draw_rect(renderer, 620, 110, 580, 200, 15, 15, 45, 255);
    draw_border(renderer, 620, 110, 580, 200, 60, 60, 120, 255);

    const char* stat_names[] = { "ST", "MA", "EN", "AG", "LU", "HP", "SP" };
    StatModType stat_types[] = {
        MOD_ST, MOD_MA, MOD_EN, MOD_AG, MOD_LU, MOD_MAX_HP, MOD_MAX_SP
    };
    int stat_vals[] = {
        m->st, m->ma, m->en, m->ag, m->lu, m->max_hp, m->max_sp
    };

    for (int i = 0; i < 7; i++) {
        int col = i / 4;
        int row = i % 4;
        int sx  = 636 + col * 290;
        int sy  = 122 + row * 44;

        int bonus = equip_get_bonus(equipdb, m, stat_types[i]);

        char line[64];
        snprintf(line, 64, "%s  %d", stat_names[i], stat_vals[i]);
        draw_text(renderer, font, line, white, sx, sy);

        if (bonus != 0) {
            char bstr[32];
            snprintf(bstr, 32, "(%s%d)", bonus > 0 ? "+" : "", bonus);
            draw_text(renderer, font, bstr,
                      bonus > 0 ? green : red, sx + 100, sy);
        }
    }

    // ── Item browser (overlay) ─────────────────────────────────────────────
    if (ss->browsing_equip) {
        EquipSlot target_slot = (EquipSlot)ss->selected_slot;

        // Build matching list
        int matching[MAX_EQUIPMENT];
        int match_count = 0;
        for (int i = 0; i < equipdb->count; i++)
            if (equipdb->pieces[i].slot == target_slot)
                matching[match_count++] = i;

        draw_rect(renderer,   200, 340, 880, 310, 8, 8, 28, 255);
        draw_border(renderer, 200, 340, 880, 310, 120, 120, 200, 255);

        char browser_title[64];
        snprintf(browser_title, 64, "Choose %s",
                 equip_slot_name(target_slot));
        draw_text(renderer, font, browser_title, yellow, 220, 352);

        // Column headers
        draw_text(renderer, font, "Name",    gray, 220, 380);
        draw_text(renderer, font, "Bonuses", gray, 580, 380);
        draw_rect(renderer, 220, 400, 840, 1, 60, 60, 120, 255);

        int visible = 9;
        int start   = 0;
        if (ss->selected_item >= visible)
            start = ss->selected_item - visible + 1;

        if (match_count == 0) {
            draw_text(renderer, font, "No items available.", gray, 220, 415);
        }

        for (int i = start;
             i < match_count && i < start + visible; i++) {
            EquipmentPiece* p = &equipdb->pieces[matching[i]];
            int iy = 410 + (i - start) * 28;
            int is_sel = (i == ss->selected_item);

            if (is_sel)
                draw_rect(renderer, 218, iy - 2, 844, 26, 40, 40, 80, 255);

            char label[80];
            snprintf(label, 80, "%s %s",
                     is_sel ? ">" : " ", p->name);
            draw_text(renderer, font, label,
                      is_sel ? yellow : white, 220, iy);

            // Show stat mods
            char mods[128] = "";
            for (int mi = 0; mi < p->mod_count; mi++) {
                char buf[32];
                int v = p->mods[mi].value;
                snprintf(buf, 32, "%s%s%d ",
                         v > 0 ? "+" : "",
                         equip_mod_name(p->mods[mi].type), v);
                strncat(mods, buf, sizeof(mods) - strlen(mods) - 1);
            }
            draw_text(renderer, font, mods,
                      is_sel ? cyan : gray, 580, iy);

            // Mark currently equipped
            if (m->equipped[(int)target_slot] == p->id)
                draw_text(renderer, font, "[E]", green, 1020, iy);
        }

        // Description of highlighted item
        if (match_count > 0) {
            EquipmentPiece* p =
                &equipdb->pieces[matching[ss->selected_item]];
            draw_rect(renderer, 220, 612, 840, 1, 60, 60, 120, 255);
            draw_text(renderer, font, p->description, gray, 220, 622);
        }
    }

    // Navigation hints
    if (!ss->browsing_equip)
        draw_text(renderer, font,
                  "Up/Down: slot  E: change  X: unequip  Q/Esc: back",
                  gray, 60, 650);
    else
        draw_text(renderer, font,
                  "Up/Down: select  E: equip  Esc: cancel",
                  gray, 220, 648);
}

// ── Main render ───────────────────────────────────────────────────────────────

void statusscreen_render(StatusScreen* ss, SDL_Renderer* renderer,
                          TTF_Font* font, Party* party,
                          SkillDB* skilldb, EquipDB* equipdb) {
    if (ss->state == STATUS_CLOSED) return;

    SDL_Color white  = {255, 255, 255, 255};
    SDL_Color yellow = {255, 220, 100, 255};
    SDL_Color gray   = {150, 150, 180, 255};
    SDL_Color green  = {100, 220, 100, 255};
    SDL_Color cyan   = { 80, 200, 220, 255};
    SDL_Color red    = {220,  60,  60, 255};

    // Dark overlay
    draw_rect(renderer, 0, 0, 1280, 720, 0, 0, 0, 200);

    // ── Equip screen ─────────────────────────────────────────────────────────
    if (ss->state == STATUS_EQUIP) {
        render_equip_screen(ss, renderer, font, party, equipdb);
        return;
    }

    // ── Overview ─────────────────────────────────────────────────────────────
    if (ss->state == STATUS_MAIN) {
        draw_rect(renderer,   40, 40, 1200, 640, 10, 10, 30, 255);
        draw_border(renderer, 40, 40, 1200, 640, 80, 80, 160, 255);
        draw_text(renderer, font, "Party Status", yellow, 560, 52);

        for (int i = 0; i < party->count; i++) {
            StatBlock* m = &party->members[i];
            int px = 60 + i * 295;
            int py = 90;

            SDL_Color bc = (i == ss->selected_member) ?
                           (SDL_Color){50, 50, 100, 255} :
                           (SDL_Color){15, 15, 45,  255};
            draw_rect(renderer,   px, py, 275, 560,
                      bc.r, bc.g, bc.b, 255);
            draw_border(renderer, px, py, 275, 560,
                        i == ss->selected_member ? 200 : 60,
                        i == ss->selected_member ? 200 : 60,
                        i == ss->selected_member ? 255 : 120,
                        255);

            draw_text(renderer, font, m->name, yellow, px + 10, py + 10);
            char lvl[32];
            snprintf(lvl, 32, "Lv. %d", m->level);
            draw_text(renderer, font, lvl, white, px + 10, py + 36);

            char hp[32];
            snprintf(hp, 32, "HP %d/%d", m->hp, m->max_hp);
            draw_text(renderer, font, hp, white, px + 10, py + 70);
            draw_stat_bar(renderer, px + 10, py + 94, 255,
                          m->hp, m->max_hp, 60, 200, 60);

            char sp[32];
            snprintf(sp, 32, "SP %d/%d", m->sp, m->max_sp);
            draw_text(renderer, font, sp, cyan, px + 10, py + 110);
            draw_stat_bar(renderer, px + 10, py + 134, 255,
                          m->sp, m->max_sp, 60, 100, 220);

            int sy = py + 160;
            char stat[32];
            snprintf(stat, 32, "ST  %d", m->st);
            draw_text(renderer, font, stat, white, px + 10, sy);
            snprintf(stat, 32, "MA  %d", m->ma);
            draw_text(renderer, font, stat, white, px + 10, sy + 26);
            snprintf(stat, 32, "EN  %d", m->en);
            draw_text(renderer, font, stat, white, px + 10, sy + 52);
            snprintf(stat, 32, "AG  %d", m->ag);
            draw_text(renderer, font, stat, white, px + 10, sy + 78);
            snprintf(stat, 32, "LU  %d", m->lu);
            draw_text(renderer, font, stat, white, px + 10, sy + 104);

            char exp[32];
            int threshold = m->level * 100;
            snprintf(exp, 32, "EXP %d/%d", m->exp, threshold);
            draw_text(renderer, font, exp, gray, px + 10, sy + 140);
            draw_stat_bar(renderer, px + 10, sy + 164, 255,
                          m->exp, threshold, 180, 100, 220);

            if (m->status != STATUS_NONE)
                draw_text(renderer, font,
                          status_name(m->status), red,
                          px + 10, sy + 188);

            // Show equipped weapon name (brief summary)
            EquipmentPiece* wp = equip_get(equipdb, m->equipped[EQUIP_WEAPON]);
            if (wp) {
                char weap[48];
                snprintf(weap, 48, "[W] %s", wp->name);
                draw_text(renderer, font, weap, cyan, px + 10, sy + 215);
            }

            draw_text(renderer, font, "Affinities:", gray,
                      px + 10, sy + 240);

            const char* elem_short[] = {
                "Ph","Gu","Fi","Ic","El","Wi","Ps","Nu","Bl","Cu"
            };
            for (int el = 0; el < ELEMENT_COUNT; el++) {
                Affinity aff = m->affinities[el];
                if (aff == AFF_NORMAL) continue;
                SDL_Color ac;
                switch (aff) {
                    case AFF_WEAK:   ac=(SDL_Color){255,100,100,255};break;
                    case AFF_RESIST: ac=(SDL_Color){100,100,255,255};break;
                    case AFF_NULL:   ac=(SDL_Color){180,180,180,255};break;
                    case AFF_REPEL:  ac=(SDL_Color){255,200, 50,255};break;
                    case AFF_ABSORB: ac=(SDL_Color){ 50,200, 50,255};break;
                    default:         ac=gray;break;
                }
                char tag[16];
                snprintf(tag, 16, "%s:%s",
                         elem_short[el], affinity_name(aff));
                int row = el / 5, col2 = el % 5;
                draw_text(renderer, font, tag, ac,
                          px + 10 + col2 * 52, sy + 263 + row * 22);
            }

            if (i == ss->selected_member)
                draw_text(renderer, font, "E: details  Q: equip",
                          gray, px + 10, py + 530);
        }

        draw_text(renderer, font,
                  "L/R: switch member  Esc: close",
                  gray, 480, 650);

    // ── Detail ───────────────────────────────────────────────────────────────
    } else if (ss->state == STATUS_DETAIL) {
        StatBlock* m = &party->members[ss->selected_member];

        draw_rect(renderer,   40, 40, 1200, 640, 10, 10, 30, 255);
        draw_border(renderer, 40, 40, 1200, 640, 80, 80, 160, 255);

        char header[64];
        snprintf(header, 64, "%s  —  Lv. %d", m->name, m->level);
        draw_text(renderer, font, header, yellow, 60, 52);

        int lx = 60, ly = 90;
        draw_text(renderer, font, "Stats", gray, lx, ly);

        char stat[32];
        snprintf(stat, 32, "HP   %d / %d", m->hp, m->max_hp);
        draw_text(renderer, font, stat, white, lx, ly + 30);
        draw_stat_bar(renderer, lx, ly + 54, 280,
                      m->hp, m->max_hp, 60, 200, 60);

        snprintf(stat, 32, "SP   %d / %d", m->sp, m->max_sp);
        draw_text(renderer, font, stat, cyan, lx, ly + 70);
        draw_stat_bar(renderer, lx, ly + 94, 280,
                      m->sp, m->max_sp, 60, 100, 220);

        snprintf(stat, 32, "ST   %d", m->st);
        draw_text(renderer, font, stat, white, lx, ly + 120);
        snprintf(stat, 32, "MA   %d", m->ma);
        draw_text(renderer, font, stat, white, lx, ly + 146);
        snprintf(stat, 32, "EN   %d", m->en);
        draw_text(renderer, font, stat, white, lx, ly + 172);
        snprintf(stat, 32, "AG   %d", m->ag);
        draw_text(renderer, font, stat, white, lx, ly + 198);
        snprintf(stat, 32, "LU   %d", m->lu);
        draw_text(renderer, font, stat, white, lx, ly + 224);

        int threshold = m->level * 100;
        snprintf(stat, 32, "EXP  %d / %d", m->exp, threshold);
        draw_text(renderer, font, stat, gray, lx, ly + 260);
        draw_stat_bar(renderer, lx, ly + 284, 280,
                      m->exp, threshold, 180, 100, 220);

        if (m->status != STATUS_NONE) {
            char sts[64];
            snprintf(sts, 64, "Status: %s (%d turns)",
                     status_name(m->status), m->status_turns);
            draw_text(renderer, font, sts, red, lx, ly + 310);
        }

        // Affinities
        int rx = 400, ry = 90;
        draw_text(renderer, font, "Affinities", gray, rx, ry);
        const char* elem_names[] = {
            "Physical","Gun","Fire","Ice","Electric",
            "Wind","Psychic","Nuclear","Bless","Curse"
        };
        for (int el = 0; el < ELEMENT_COUNT; el++) {
            Affinity aff = m->affinities[el];
            SDL_Color ac;
            switch (aff) {
                case AFF_WEAK:   ac=(SDL_Color){255,100,100,255};break;
                case AFF_RESIST: ac=(SDL_Color){100,100,255,255};break;
                case AFF_NULL:   ac=(SDL_Color){180,180,180,255};break;
                case AFF_REPEL:  ac=(SDL_Color){255,200, 50,255};break;
                case AFF_ABSORB: ac=(SDL_Color){ 50,200, 50,255};break;
                default:         ac=gray;break;
            }
            char row[64];
            snprintf(row, 64, "%-10s %s",
                     elem_names[el], affinity_name(aff));
            draw_text(renderer, font, row, ac, rx, ry + 30 + el * 26);
        }

        // Skills
        int sx2 = 780, sy2 = 90;
        draw_text(renderer, font, "Skills", gray, sx2, sy2);
        draw_text(renderer, font, "E: manage  Q: equip",
                  gray, sx2, sy2 + 20);

        for (int i = 0; i < m->skill_count; i++) {
            Skill* sk = skilldb_get(skilldb, m->skill_ids[i]);
            if (!sk) continue;
            char skl[80];
            snprintf(skl, 80, "%-14s SP:%-3d %s",
                     sk->name, sk->sp_cost,
                     element_name(sk->element));
            draw_text(renderer, font, skl, white,
                      sx2, sy2 + 50 + i * 28);
        }

        draw_text(renderer, font,
                  "L/R: switch  E: skills  Q: equip  Esc: back",
                  gray, 400, 650);

    // ── Skills ───────────────────────────────────────────────────────────────
    } else if (ss->state == STATUS_SKILLS) {
        StatBlock* m = &party->members[ss->selected_member];

        draw_rect(renderer,   200, 80, 880, 560, 10, 10, 30, 255);
        draw_border(renderer, 200, 80, 880, 560, 80, 80, 160, 255);

        char header[64];
        snprintf(header, 64, "%s's Skills", m->name);
        draw_text(renderer, font, header, yellow, 220, 92);

        draw_text(renderer, font, "Skill",   gray, 220, 122);
        draw_text(renderer, font, "Element", gray, 500, 122);
        draw_text(renderer, font, "SP Cost", gray, 680, 122);
        draw_text(renderer, font, "Effect",  gray, 800, 122);
        draw_rect(renderer, 220, 146, 840, 1, 80, 80, 160, 255);

        for (int i = 0; i < m->skill_count; i++) {
            Skill* sk = skilldb_get(skilldb, m->skill_ids[i]);
            if (!sk) continue;

            int iy = 155 + i * 36;

            if (i == ss->selected_skill)
                draw_rect(renderer, 220, iy - 2, 840, 32,
                          40, 40, 80, 255);

            SDL_Color col = (i == ss->selected_skill) ? yellow : white;

            char label[32];
            snprintf(label, 32, "%s %s",
                     i == ss->selected_skill ? ">" : " ", sk->name);
            draw_text(renderer, font, label, col, 220, iy);
            draw_text(renderer, font,
                      element_name(sk->element), col, 500, iy);

            char spcost[16];
            snprintf(spcost, 16, "%d", sk->sp_cost);
            draw_text(renderer, font, spcost, col, 680, iy);

            if (sk->inflicts != STATUS_NONE) {
                char effect[32];
                snprintf(effect, 32, "%s %.0f%%",
                         status_name(sk->inflicts),
                         sk->status_chance * 100);
                draw_text(renderer, font, effect, col, 800, iy);
            } else if (sk->is_heal) {
                draw_text(renderer, font, "Heal", green, 800, iy);
            }
        }

        if (ss->selected_skill < m->skill_count) {
            Skill* sk = skilldb_get(skilldb,
                                    m->skill_ids[ss->selected_skill]);
            if (sk) {
                draw_rect(renderer, 220, 580, 840, 1,
                          80, 80, 160, 255);
                draw_text(renderer, font, sk->description,
                          gray, 220, 592);
            }
        }

        draw_text(renderer, font, "Up/Down: select  Esc: back",
                  gray, 440, 622);
    }
}