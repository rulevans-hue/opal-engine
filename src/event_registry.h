#ifndef EVENT_REGISTRY_H
#define EVENT_REGISTRY_H

// ── Event Registry ────────────────────────────────────────────────────────────
// Clean builder API for defining story events without touching engine code.
// Events are defined declaratively in events.c using these helpers.
//
// Usage:
//   er_on_day_slot(es, "intro", 1, TIMESLOT_MORNING,
//       er_set_flag("game_started", 1, "A new story begins..."));
//
//   er_on_flag(es, "welcome", "game_started", 1,
//       er_notify("Explore the town. Talk to people."));
//
//   er_on_stat_rank(es, "knowledge_boost", SOCIAL_KNOWLEDGE, 2,
//       er_set_flag("knowledge_rank2", 1, "Your knowledge has grown."));

#include "events.h"

// ── Trigger builders ──────────────────────────────────────────────────────────

// Fire on a specific day+slot
GameEvent* er_on_day_slot(EventSystem* es, const char* name,
                           int day, TimeSlot slot);

// Fire when a flag reaches a value
GameEvent* er_on_flag(EventSystem* es, const char* name,
                       const char* flag, int value);

// Fire when a stat reaches a rank
GameEvent* er_on_stat_rank(EventSystem* es, const char* name,
                            SocialStatType stat, int rank);

// Fire on a specific day (any slot)
GameEvent* er_on_day(EventSystem* es, const char* name, int day);

// ── Requirement chaining ──────────────────────────────────────────────────────

// Require a flag before this event fires (chain after er_on_*)
GameEvent* er_require_flag(GameEvent* e, const char* flag, int value);

// ── Action setters ────────────────────────────────────────────────────────────

// Set event action to: show a notification
GameEvent* er_notify(GameEvent* e, const char* message);

// Set event action to: set a flag + optional notification
GameEvent* er_set_flag(GameEvent* e, const char* flag, int value,
                        const char* message);

// Set event action to: force a dialogue node
GameEvent* er_dialogue(GameEvent* e, int node);

// Mark event as repeatable (fires every time conditions are met)
GameEvent* er_repeatable(GameEvent* e);

#endif