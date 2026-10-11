#include "social_stats.h"
#include <string.h>
#include <stdio.h>

void social_stats_init(SocialStats* ss) {
    for (int i = 0; i < SOCIAL_COUNT; i++) {
        ss->exp[i]  = 0;
        ss->rank[i] = 1;
    }
}

void social_stats_add_exp(SocialStats* ss, SocialStatType stat,
                           int amount) {
    if (ss->rank[stat] >= SOCIAL_MAX_RANK) return;

    ss->exp[stat] += amount;
    while (ss->exp[stat] >= SOCIAL_EXP_PER_RANK &&
           ss->rank[stat] < SOCIAL_MAX_RANK) {
        ss->exp[stat] -= SOCIAL_EXP_PER_RANK;
        ss->rank[stat]++;
        printf("%s rank up! Now: %s\n",
               social_stat_name(stat),
               social_rank_name(stat, ss->rank[stat]));
    }
    if (ss->rank[stat] >= SOCIAL_MAX_RANK)
        ss->exp[stat] = 0;
}

int social_stats_get_rank(SocialStats* ss, SocialStatType stat) {
    return ss->rank[stat];
}

const char* social_stat_name(SocialStatType stat) {
    switch (stat) {
        case SOCIAL_KNOWLEDGE:   return "Knowledge";
        case SOCIAL_CHARM:       return "Charm";
        case SOCIAL_GUTS:        return "Guts";
        case SOCIAL_KINDNESS:    return "Kindness";
        case SOCIAL_PROFICIENCY: return "Proficiency";
        default:                 return "Unknown";
    }
}

const char* social_rank_name(SocialStatType stat, int rank) {
    if (rank < 1) rank = 1;
    if (rank > 5) rank = 5;

    static const char* knowledge[] = {
        "Ordinary", "Informed", "Scholarly", "Expert", "Genius"
    };
    static const char* charm[] = {
        "Rough", "Unpolished", "Refined", "Charismatic", "Alluring"
    };
    static const char* guts[] = {
        "Timid", "Cautious", "Resolute", "Dauntless", "Lionhearted"
    };
    static const char* kindness[] = {
        "Apathetic", "Kind", "Empathetic", "Generous", "Saintly"
    };
    static const char* proficiency[] = {
        "Inept", "Capable", "Skilled", "Masterful", "Virtuoso"
    };

    switch (stat) {
        case SOCIAL_KNOWLEDGE:   return knowledge[rank-1];
        case SOCIAL_CHARM:       return charm[rank-1];
        case SOCIAL_GUTS:        return guts[rank-1];
        case SOCIAL_KINDNESS:    return kindness[rank-1];
        case SOCIAL_PROFICIENCY: return proficiency[rank-1];
        default:                 return "Unknown";
    }
}