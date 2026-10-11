#include "summon_menu.h"
#include "audio.h"
#include <string.h>
#include <stdio.h>

static void draw_rect(SDL_Renderer* r, int x, int y, int w, int h,
                      Uint8 rr, Uint8 g, Uint8 b, Uint8 a) {
    SDL_SetRenderDrawColor(r, rr, g, b, a);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderFillRect(r, &rect);
}
static void draw_border(SDL_Renderer* r, int x, int y, int w, int h,
                        Uint8 rr, Uint8 g, Uint8 b, Uint8 a) {
    SDL_SetRenderDrawColor(r, rr, g, b, a);
    SDL_Rect rect = {x, y, w, h};
    SDL_RenderDrawRect(r, &rect);
}
static void draw_text(SDL_Renderer* r, TTF_Font* font,
                      const char* text, SDL_Color color, int x, int y) {
    if (!text || !text[0]) return;
    SDL_Surface* surf = TTF_RenderText_Blended(font, text, color);
    if (!surf) return;
    SDL_Texture* tex = SDL_CreateTextureFromSurface(r, surf);
    SDL_Rect dst = {x, y, surf->w, surf->h};
    SDL_RenderCopy(r, tex, NULL, &dst);
    SDL_FreeSurface(surf);
    SDL_DestroyTexture(tex);
}

void summon_menu_init(SummonMenu* pm) {
    pm->state      = PMENU_CLOSED;
    pm->selected   = 0;
    pm->fuse_idx_a = -1;
    pm->comp_ref   = NULL;
    pm->inv_ref    = NULL;
}
void summon_menu_open(SummonMenu* pm, SummonCompendium* comp, Inventory* inv) {
    pm->state    = PMENU_BROWSE;
    pm->selected = 0;
    pm->comp_ref = comp;
    pm->inv_ref  = inv;
}
void summon_menu_close(SummonMenu* pm) {
    pm->state      = PMENU_CLOSED;
    pm->selected   = 0;
    pm->fuse_idx_a = -1;
}

void summon_menu_handle_input(SummonMenu* pm, SDL_Event* event,
                                SummonStock* stock, SummonDB* db,
                                SummonCompendium* comp,
                                NotifySystem* ns, Inventory* inv) {
    if (pm->state == PMENU_CLOSED) return;
    if (event->type != SDL_KEYDOWN) return;
    SDL_Keycode key = event->key.keysym.sym;

    if (pm->state == PMENU_BROWSE) {
        switch (key) {
            case SDLK_UP:
                pm->selected = (pm->selected - 1 + stock->count) % stock->count;
                audio_play_sfx(SFX_MENU_MOVE); break;
            case SDLK_DOWN:
                pm->selected = (pm->selected + 1) % stock->count;
                audio_play_sfx(SFX_MENU_MOVE); break;
            case SDLK_e: case SDLK_RETURN:
                if (pm->selected != stock->active_idx) {
                    summon_switch(stock, pm->selected);
                    char msg[80];
                    snprintf(msg, 80, "Switched to %s.",
                             stock->stock[pm->selected].name);
                    notify_success(ns, msg);
                    audio_play_sfx(SFX_MENU_CONFIRM);
                }
                summon_menu_close(pm); break;
            case SDLK_f:
                if (stock->count < 2)
                    notify_info(ns, "Need at least 2 Summons to fuse.");
                else { pm->state = PMENU_FUSE_A; pm->fuse_idx_a = -1; pm->selected = 0; }
                break;
            case SDLK_r: {
                Summon* cur = &stock->stock[stock->active_idx];
                int already_best = 0;
                for (int i = 0; i < comp->count; i++) {
                    if (strcmp(comp->entries[i].name, cur->name) == 0) {
                        if (comp->entries[i].level >= cur->level) already_best = 1;
                        break;
                    }
                }
                if (already_best)
                    notify_info(ns, "Compendium already has this level or higher.");
                else {
                    summon_reregister(comp, stock);
                    char msg[80];
                    snprintf(msg, 80, "%s (Lv.%d) saved to compendium.", cur->name, cur->level);
                    notify_success(ns, msg);
                }
                break;
            }
            case SDLK_c: pm->state = PMENU_COMPENDIUM; pm->selected = 0; break;
            case SDLK_ESCAPE: summon_menu_close(pm); break;
        }
        return;
    }

    if (pm->state == PMENU_FUSE_A) {
        switch (key) {
            case SDLK_UP:
                pm->selected = (pm->selected - 1 + stock->count) % stock->count;
                audio_play_sfx(SFX_MENU_MOVE); break;
            case SDLK_DOWN:
                pm->selected = (pm->selected + 1) % stock->count;
                audio_play_sfx(SFX_MENU_MOVE); break;
            case SDLK_e: case SDLK_RETURN:
                pm->fuse_idx_a = pm->selected;
                pm->state = PMENU_FUSE_B;
                pm->selected = pm->selected == 0 ? 1 : 0;
                break;
            case SDLK_ESCAPE: pm->state = PMENU_BROWSE; pm->selected = 0; break;
        }
        return;
    }

    if (pm->state == PMENU_FUSE_B) {
        switch (key) {
            case SDLK_UP:
                pm->selected = (pm->selected - 1 + stock->count) % stock->count;
                if (pm->selected == pm->fuse_idx_a)
                    pm->selected = (pm->selected - 1 + stock->count) % stock->count;
                audio_play_sfx(SFX_MENU_MOVE); break;
            case SDLK_DOWN:
                pm->selected = (pm->selected + 1) % stock->count;
                if (pm->selected == pm->fuse_idx_a)
                    pm->selected = (pm->selected + 1) % stock->count;
                audio_play_sfx(SFX_MENU_MOVE); break;
            case SDLK_e: case SDLK_RETURN:
                if (pm->selected == pm->fuse_idx_a) {
                    notify_info(ns, "Can't fuse a Summon with itself."); break;
                }
                if (summon_fuse(stock, pm->fuse_idx_a, pm->selected, db, &pm->fuse_result))
                    pm->state = PMENU_CONFIRM;
                else {
                    notify_info(ns, "Fusion failed.");
                    pm->state = PMENU_BROWSE; pm->selected = 0;
                }
                break;
            case SDLK_ESCAPE:
                pm->state = PMENU_FUSE_A; pm->selected = pm->fuse_idx_a; break;
        }
        return;
    }

    if (pm->state == PMENU_CONFIRM) {
        if (key == SDLK_e || key == SDLK_RETURN) {
            int idx_b = pm->selected;
            int hi = pm->fuse_idx_a > idx_b ? pm->fuse_idx_a : idx_b;
            int lo = pm->fuse_idx_a < idx_b ? pm->fuse_idx_a : idx_b;
            summon_register(comp, &stock->stock[hi]);
            summon_register(comp, &stock->stock[lo]);
            summon_remove(stock, hi);
            summon_remove(stock, lo);
            summon_add(stock, &pm->fuse_result);
            summon_register(comp, &pm->fuse_result);
            stock->active_idx = stock->count - 1;
            char msg[80];
            snprintf(msg, 80, "%s created!", pm->fuse_result.name);
            notify_success(ns, msg);
            audio_play_sfx(SFX_MENU_CONFIRM);
            summon_menu_close(pm);
        } else if (key == SDLK_ESCAPE) {
            pm->state = PMENU_BROWSE; pm->selected = 0;
        }
        return;
    }

    if (pm->state == PMENU_COMPENDIUM) {
        switch (key) {
            case SDLK_UP:
                if (comp->count > 0)
                    pm->selected = (pm->selected - 1 + comp->count) % comp->count;
                audio_play_sfx(SFX_MENU_MOVE); break;
            case SDLK_DOWN:
                if (comp->count > 0)
                    pm->selected = (pm->selected + 1) % comp->count;
                audio_play_sfx(SFX_MENU_MOVE); break;
            case SDLK_e: case SDLK_RETURN:
                if (comp->count > 0 && inv) {
                    Summon* cp = &comp->entries[pm->selected];
                    int cost = summon_resummon_cost(cp);
                    int has = 0;
                    for (int i = 0; i < stock->count; i++)
                        if (strcmp(stock->stock[i].name, cp->name) == 0) { has = 1; break; }
                    if (has) notify_info(ns, "Already in your stock.");
                    else if (stock->count >= MAX_PROTA_SUMMONS) notify_info(ns, "Stock is full.");
                    else if (inv->yen < cost) {
                        char msg[64];
                        snprintf(msg, 64, "Need %d yen (have %d).", cost, inv->yen);
                        notify_info(ns, msg);
                    } else if (summon_resummon(comp, pm->selected, stock, &inv->yen)) {
                        char msg[64];
                        snprintf(msg, 64, "%s resummoned! (-%d yen)", cp->name, cost);
                        notify_success(ns, msg);
                        audio_play_sfx(SFX_MENU_CONFIRM);
                    }
                }
                break;
            case SDLK_ESCAPE: pm->state = PMENU_BROWSE; pm->selected = 0; break;
        }
        return;
    }
}

static void render_detail(SDL_Renderer* renderer, TTF_Font* font,
                           Summon* p, SkillDB* skilldb) {
    if (!p) return;
    SDL_Color yellow = {255,220,100,255};
    SDL_Color white  = {255,255,255,255};
    SDL_Color gray   = {150,150,180,255};
    SDL_Color green  = {100,220,100,255};
    SDL_Color red    = {220, 80, 80,255};
    SDL_Color cyan   = { 80,200,220,255};

    int x = 820, y = 90, w = 350, h = 530;
    draw_rect(renderer,   x, y, w, h, 12, 12, 42, 245);
    draw_border(renderer, x, y, w, h, 80, 60, 180, 255);

    draw_text(renderer, font, p->name, yellow, x+16, y+14);
    char lv[48]; snprintf(lv, 48, "%s  Lv.%d", arcana_name(p->arcana), p->level);
    draw_text(renderer, font, lv, cyan, x+16, y+40);

    draw_text(renderer, font, "Skills:", gray, x+16, y+74);
    for (int i = 0; i < p->skill_count; i++) {
        char sk[64];
        if (skilldb) {
            Skill* s = skilldb_get(skilldb, p->skill_ids[i]);
            snprintf(sk, 64, "  %s", s ? s->name : "???");
        } else snprintf(sk, 64, "  Skill %d", p->skill_ids[i]);
        draw_text(renderer, font, sk, white, x+16, y+96+i*24);
    }

    draw_text(renderer, font, "Affinities:", gray, x+16, y+290);
    const char* en[] = {"Phys","Gun","Fire","Ice","Elec","Wind","Psy","Nuke","Bless","Curse"};
    const char* an[] = {"-","Wk","Rs","Nu","Rp","Ab"};
    int ay = y+314;
    for (int e = 0; e < ELEMENT_COUNT; e++) {
        if (p->affinities[e] == AFF_NORMAL) continue;
        SDL_Color ac = p->affinities[e]==AFF_WEAK  ? red  :
                       p->affinities[e]==AFF_RESIST ? green :
                       p->affinities[e]==AFF_NULL   ? cyan  : white;
        char buf[32]; snprintf(buf, 32, "%s: %s", en[e], an[p->affinities[e]]);
        draw_text(renderer, font, buf, ac, x+16, ay); ay += 22;
    }
}

void summon_menu_render(SummonMenu* pm, SDL_Renderer* renderer,
                          TTF_Font* font, SummonStock* stock,
                          SkillDB* skilldb) {
    if (pm->state == PMENU_CLOSED) return;

    SummonCompendium* comp = pm->comp_ref;
    Inventory*         inv  = pm->inv_ref;

    SDL_Color white  = {255,255,255,255};
    SDL_Color yellow = {255,220,100,255};
    SDL_Color gray   = {150,150,180,255};
    SDL_Color green  = {100,220,100,255};
    SDL_Color red    = {220, 80, 80,255};
    SDL_Color gold   = {255,200, 50,255};
    SDL_Color cyan   = { 80,200,220,255};

    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_SetRenderDrawColor(renderer, 0, 0, 0, 200);
    SDL_Rect ov = {0,0,1280,720}; SDL_RenderFillRect(renderer, &ov);

    draw_rect(renderer,   80, 40, 1120, 640, 8, 8, 28, 252);
    draw_border(renderer, 80, 40, 1120, 640, 100, 60, 200, 255);

    // Yen always top-right
    if (inv) {
        char yen[32]; snprintf(yen, 32, "Yen: %d", inv->yen);
        draw_text(renderer, font, yen, gold, 1000, 52);
    }

    // ── COMPENDIUM ────────────────────────────────────────────────────────────
    if (pm->state == PMENU_COMPENDIUM) {
        draw_text(renderer, font, "COMPENDIUM", yellow, 540, 52);

        if (!comp || comp->count == 0) {
            draw_text(renderer, font, "No Summons registered yet.", gray, 160, 200);
        } else {
            for (int i = 0; i < comp->count; i++) {
                Summon* cp = &comp->entries[i];
                int cy = 96 + i * 52;
                int is_sel = (i == pm->selected);
                int cost   = summon_resummon_cost(cp);
                int in_stock = 0;
                for (int j = 0; j < stock->count; j++)
                    if (strcmp(stock->stock[j].name, cp->name) == 0) { in_stock = 1; break; }
                int can_afford = inv && inv->yen >= cost;

                if (is_sel) draw_rect(renderer, 96, cy-4, 700, 48, 40,30,90,255);

                SDL_Color col = in_stock ? gray : is_sel ? yellow : white;
                char row[80];
                snprintf(row, 80, "%s %s | %s  Lv.%d",
                         is_sel ? ">" : " ", cp->name,
                         arcana_name(cp->arcana), cp->level);
                draw_text(renderer, font, row, col, 106, cy);

                // Cost column — always visible, colour-coded
                char cost_str[32]; snprintf(cost_str, 32, "%d yen", cost);
                draw_text(renderer, font, cost_str,
                          in_stock ? gray : can_afford ? gold : red,
                          660, cy);

                if (in_stock)
                    draw_text(renderer, font, "[In Stock]", gray, 790, cy);

                // Affinities on row 2
                const char* es[]={"Ph","Gu","Fi","Ic","El","Wi","Ps","Nu","Bl","Cu"};
                const char* as[]={"","Wk","Rs","Nu","Rp","Ab"};
                char aff[64]="";
                for (int e=0;e<ELEMENT_COUNT;e++) {
                    if (cp->affinities[e]!=AFF_NORMAL) {
                        char b[12]; snprintf(b,12,"%s:%s ",es[e],as[cp->affinities[e]]);
                        strncat(aff,b,sizeof(aff)-strlen(aff)-1);
                    }
                }
                if (aff[0]) draw_text(renderer, font, aff, gray, 120, cy+26);
            }
            if (pm->selected < comp->count)
                render_detail(renderer, font, &comp->entries[pm->selected], skilldb);
        }
        draw_text(renderer, font, "E: resummon  Esc: back", gray, 100, 660);
        return;
    }

    // ── STOCK VIEW (Browse / Fuse A / Fuse B / Confirm) ──────────────────────
    const char* titles[] = {"SUMMON","FUSION — Select First",
                             "FUSION — Select Second","FUSION — Confirm",""};
    const char* banners[] = {
        "E: activate  F: fuse  R: save to compendium  C: compendium  Esc: close",
        "Choose the FIRST Summon to fuse into something new.",
        "Choose the SECOND Summon to fuse.",
        "Confirm? The two Summons will be consumed.",""};
    int ti = pm->state==PMENU_BROWSE  ? 0 :
             pm->state==PMENU_FUSE_A  ? 1 :
             pm->state==PMENU_FUSE_B  ? 2 :
             pm->state==PMENU_CONFIRM ? 3 : 0;

    draw_text(renderer, font, titles[ti], yellow, 540, 52);
    draw_text(renderer, font, banners[ti], gray, 96, 78);

    for (int i = 0; i < stock->count; i++) {
        Summon* p = &stock->stock[i];
        int py = 110 + i * 56;
        int is_sel    = (i == pm->selected);
        int is_active = (i == stock->active_idx);
        int is_fuse_a = (pm->state==PMENU_FUSE_B && i==pm->fuse_idx_a);
        int is_fuse_b = (pm->state==PMENU_FUSE_B && i==pm->selected && i!=pm->fuse_idx_a);

        if (is_sel)    draw_rect(renderer,   96, py-4, 700, 50, 40,30,90,255);
        if (is_active) draw_border(renderer, 96, py-4, 700, 50, 80,200,80,200);
        if (is_fuse_a) draw_border(renderer, 96, py-4, 700, 50, 220,180,60,255);
        if (is_fuse_b) draw_border(renderer, 96, py-4, 700, 50, 80,180,255,255);

        SDL_Color col = is_sel ? yellow : white;
        char name_str[80];
        snprintf(name_str, 80, "%s %s | %s  Lv.%d",
                 is_sel ? ">" : " ", p->name, arcana_name(p->arcana), p->level);
        draw_text(renderer, font, name_str, col, 106, py);

        int tx = 620;
        if (is_active) { draw_text(renderer,font,"[Active]",green,tx,py); tx+=90; }
        if (is_fuse_a)   draw_text(renderer,font,"[A]",gold,tx,py);
        if (is_fuse_b)   draw_text(renderer,font,"[B]",cyan,tx,py);

        const char* es[]={"Ph","Gu","Fi","Ic","El","Wi","Ps","Nu","Bl","Cu"};
        const char* as[]={"","Wk","Rs","Nu","Rp","Ab"};
        char aff[96]="";
        for (int e=0;e<ELEMENT_COUNT;e++) {
            if (p->affinities[e]!=AFF_NORMAL) {
                char b[12]; snprintf(b,12,"%s:%s ",es[e],as[p->affinities[e]]);
                strncat(aff,b,sizeof(aff)-strlen(aff)-1);
            }
        }
        if (aff[0]) draw_text(renderer,font,aff,gray,120,py+28);
    }

    if (pm->state == PMENU_CONFIRM) {
        Summon* a = &stock->stock[pm->fuse_idx_a];
        Summon* b = &stock->stock[pm->selected];
        draw_rect(renderer, 96, 490, 700, 100, 20,10,50,240);
        draw_border(renderer, 96, 490, 700, 100, 180,100,255,255);
        char fline[128];
        snprintf(fline,128,"%s (Lv.%d)  +  %s (Lv.%d)",
                 a->name,a->level,b->name,b->level);
        draw_text(renderer,font,fline,gray,110,500);
        char rline[64];
        snprintf(rline,64,"= %s (Lv.%d)  [%s]",
                 pm->fuse_result.name,pm->fuse_result.level,
                 arcana_name(pm->fuse_result.arcana));
        draw_text(renderer,font,rline,yellow,110,528);
        draw_text(renderer,font,"E: confirm    Esc: cancel",gray,110,558);
        render_detail(renderer,font,&pm->fuse_result,skilldb);
    } else if (pm->selected < stock->count) {
        render_detail(renderer,font,&stock->stock[pm->selected],skilldb);
    }
}