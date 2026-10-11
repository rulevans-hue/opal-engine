#include "party.h"
#include <string.h>

void party_init(Party* p) {
    p->count        = 0;
    p->active_index = 0;
}

void party_add_member(Party* p, StatBlock member) {
    if (p->count >= PARTY_MAX) return;
    p->members[p->count++] = member;
}

StatBlock* party_active(Party* p) {
    return &p->members[p->active_index];
}

void party_next_turn(Party* p) {
    // Cycle to next living member
    for (int i = 1; i <= p->count; i++) {
        int next = (p->active_index + i) % p->count;
        if (p->members[next].alive) {
            p->active_index = next;
            return;
        }
    }
}

int party_all_dead(Party* p) {
    // Prota (slot 0) dying = game over, even if others live (protagonist rule)
    if (p->count > 0 && !p->members[0].alive) return 1;
    for (int i = 0; i < p->count; i++)
        if (p->members[i].alive) return 0;
    return 1;
}

int party_living_count(Party* p) {
    int count = 0;
    for (int i = 0; i < p->count; i++)
        if (p->members[i].alive) count++;
    return count;
}