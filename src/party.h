#ifndef PARTY_H
#define PARTY_H

#include "stats.h"
#include "skills.h"

#define PARTY_MAX 4

typedef struct {
    StatBlock members[PARTY_MAX];
    int       count;
    int       active_index;  // whose turn it is
} Party;

void party_init(Party* p);
void party_add_member(Party* p, StatBlock member);
StatBlock* party_active(Party* p);
void party_next_turn(Party* p);
int  party_all_dead(Party* p);
int  party_living_count(Party* p);

#endif