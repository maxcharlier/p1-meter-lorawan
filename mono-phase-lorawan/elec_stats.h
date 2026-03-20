#ifndef _ELEC_STAT_H_
#define _ELEC_STAT_H_

#define NB_TELE_STAT_FIELD 4
#define NB_QUARTERS 4

#define NO_STATS_INDEX -1
// #define AVG_ENERGY_IMPORT_STATS_INDEX 0
#define CONS_W_STATS_INDEX 0
#define PROD_W_STATS_INDEX 1
// #define CONS_L1_W_STATS_INDEX       2
// #define PROD_L1_W_STATS_INDEX       3
#define INST_L1_DECI_V_STATS_INDEX 2
#define INST_L1_CENTI_A_STATS_INDEX 3
// #define INST_L1_POS_W_STATS_INDEX   6
// #define INST_L1_NEG_W_STATS_INDEX   7

void reset_elec_stats();
void update_elec_stats(int ix, uint32_t value);
const uint32_t get_elec_stat_min_value(int ix);
const uint32_t get_elec_stat_max_value(int ix);
const uint32_t get_elec_stat_mean_value(int ix);
const uint16_t get_elec_stat_nb_samples(int ix);

#endif
