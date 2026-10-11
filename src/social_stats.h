#ifndef SOCIAL_STATS_H
#define SOCIAL_STATS_H

#define SOCIAL_MAX_RANK  5
#define SOCIAL_EXP_PER_RANK 10

typedef enum {
    SOCIAL_KNOWLEDGE = 0,
    SOCIAL_CHARM,
    SOCIAL_GUTS,
    SOCIAL_KINDNESS,
    SOCIAL_PROFICIENCY,
    SOCIAL_COUNT
} SocialStatType;

typedef struct {
    int exp[SOCIAL_COUNT];   // current exp toward next rank
    int rank[SOCIAL_COUNT];  // current rank 1-5
} SocialStats;

void social_stats_init(SocialStats* ss);
void social_stats_add_exp(SocialStats* ss, SocialStatType stat, int amount);
int  social_stats_get_rank(SocialStats* ss, SocialStatType stat);

const char* social_stat_name(SocialStatType stat);
const char* social_rank_name(SocialStatType stat, int rank);

#endif