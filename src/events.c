#include "events.h"
#include "event_registry.h"
#include <string.h>
#include <stdio.h>

void eventsystem_init(EventSystem* es) {
    es->count = 0;

    // ── Day 1: Game start ─────────────────────────────────────────────────────
    er_set_flag(
        er_on_day_slot(es, "intro", 1, TIMESLOT_MORNING),
        "game_started", 1, "A new story begins...");

    er_notify(
        er_on_flag(es, "welcome", "game_started", 1),
        "Explore the town. Talk to people.");

    // ── Day 3: Mysterious signal ──────────────────────────────────────────────
    er_set_flag(
        er_on_day_slot(es, "mysterious_signal", 3, TIMESLOT_EVENING),
        "signal_received", 1,
        "You feel a strange pull from somewhere nearby...");

    // ── Day 5: Hungerscape unlocks ──────────────────────────────────────────────
    er_require_flag(
        er_set_flag(
            er_on_day_slot(es, "hungerscape_unlock", 5, TIMESLOT_AFTERSCHOOL),
            "hungerscape_unlocked", 1,
            "The Hungerscape has been unlocked!"),
        "signal_received", 1);

    // ── Day 7: Deadline warning ───────────────────────────────────────────────
    er_require_flag(
        er_notify(
            er_on_day_slot(es, "dungeon_warning", 7, TIMESLOT_MORNING),
            "Warning: Clear the dungeon before the deadline!"),
        "hungerscape_unlocked", 1);

    // ── Social stat milestones ────────────────────────────────────────────────
    er_set_flag(
        er_on_stat_rank(es, "knowledge_check", SOCIAL_KNOWLEDGE, 2),
        "knowledge_rank2", 1,
        "Your Knowledge has grown. New options available.");

    er_set_flag(
        er_on_stat_rank(es, "guts_rank2", SOCIAL_GUTS, 2),
        "guts_rank2", 1,
        "Your courage grows. You feel more confident.");

    er_set_flag(
        er_on_stat_rank(es, "charm_rank3", SOCIAL_CHARM, 3),
        "charm_rank3", 1,
        "Your charm opens new doors.");

    // ── Story beat: Stranger opens up ────────────────────────────────────────
    er_require_flag(
        er_notify(
            er_on_flag(es, "stranger_opens_up", "met_Stranger", 1),
            "The Stranger seems to know more than they let on."),
        "hungerscape_unlocked", 1);

    // ── Dungeon 1 cleared: story continues ────────────────────────────────────
    er_notify(
        er_on_flag(es, "dungeon1_cleared_notif", "dungeon_1_cleared", 1),
        "The Castle of Greed has fallen. A Change of Heart occurred.");

    // ── Dungeon 2 unlocks after dungeon 1 is cleared ───────────────────────────
    er_require_flag(
        er_set_flag(
            er_on_flag(es, "dungeon2_unlock", "dungeon_1_cleared", 1),
            "dungeon_2_unlocked", 1,
            "A new target has emerged. The investigation continues."),
        "dungeon_1_cleared", 1);

    // ────────────────────────────────────────────────────────────────────────
    // To add new story events:
    //
    // er_set_flag(
    //     er_on_day_slot(es, "dungeon2_unlock", 25, TIMESLOT_MORNING),
    //     "dungeon_2_unlocked", 1,
    //     "A new target has been identified.");
    //
    // er_require_flag(..., "dungeon_1_cleared", 1);  // chain requirements
    // ────────────────────────────────────────────────────────────────────────
}

// ─── Engine (unchanged from original) ────────────────────────────────────────

static int check_conditions(GameEvent* e, TimeSystem* ts,
                              GameState* gs, SocialStats* ss) {
    switch (e->trigger_type) {
        case EVENT_TRIGGER_DAY:
            if (ts->day != e->trigger_day) return 0;
            break;
        case EVENT_TRIGGER_DAY_SLOT:
            if (ts->day  != e->trigger_day)  return 0;
            if (ts->slot != e->trigger_slot) return 0;
            break;
        case EVENT_TRIGGER_FLAG:
            if (gamestate_get_flag(gs, e->trigger_flag) !=
                e->trigger_flag_value) return 0;
            break;
        case EVENT_TRIGGER_STAT_RANK:
            if (ss->rank[e->trigger_stat] < e->trigger_stat_rank)
                return 0;
            break;
    }
    for (int i = 0; i < e->req_flag_count; i++)
        if (gamestate_get_flag(gs, e->req_flags[i]) !=
            e->req_flag_values[i]) return 0;
    return 1;
}

void eventsystem_update(EventSystem* es, TimeSystem* ts,
                        GameState* gs, NotifySystem* ns,
                        SocialStats* ss, WorldSystem* ws,
                        int* force_dialogue_node) {
    *force_dialogue_node = -1;

    for (int i = 0; i < es->count; i++) {
        GameEvent* e = &es->events[i];
        if (e->triggered && !e->repeatable) continue;
        if (!check_conditions(e, ts, gs, ss)) continue;

        e->triggered = 1;
        printf("Event fired: %s\n", e->name);

        switch (e->action_type) {
            case EVENT_ACTION_NOTIFY:
                notify_info(ns, e->action_message);
                break;
            case EVENT_ACTION_SET_FLAG:
            case EVENT_ACTION_UNLOCK_ROOM:
                gamestate_set_flag(gs, e->action_flag, e->action_flag_value);
                if (e->action_message[0])
                    notify_success(ns, e->action_message);
                break;
            case EVENT_ACTION_FORCE_DIALOGUE:
                *force_dialogue_node = e->action_dialogue_node;
                break;
            case EVENT_ACTION_WORLD_CHANGE:
                ws->current = e->action_world;
                if (e->action_message[0])
                    notify_info(ns, e->action_message);
                break;
        }
    }
}