
#include <stdint.h>
#include "cytypes.h"

#include <Arduino.h>
#include "elec_stats.h"

class ElecStatField {
public:
  uint32_t min_value;  // value max is 99999
  uint32_t max_value;  // value max is 99999
  uint32_t sum_values;
  uint16_t nb_samples;  // max 900

  // Default constructor initializes all fields to 0
  ElecStatField()
    : min_value(0), max_value(0), sum_values(0), nb_samples(0){};
};

// Initialise tele_stats_quarter with all values set to 0.
static ElecStatField elec_stats[NB_TELE_STAT_FIELD];


void reset_elec_stats() {
  for (int i = 0; i < NB_TELE_STAT_FIELD; ++i) {
    elec_stats[i] = ElecStatField();
  }
}

void update_elec_stats(int ix, uint32_t value) {
  if ((ix >= 0) && ix < NB_TELE_STAT_FIELD) {
    if (elec_stats[ix].nb_samples == 0) {
      elec_stats[ix].min_value = value;
      elec_stats[ix].max_value = value;
      elec_stats[ix].sum_values = 0;
    } else {
      if (value < elec_stats[ix].min_value) {
        elec_stats[ix].min_value = value;
      }
      if (value > elec_stats[ix].max_value) {
        elec_stats[ix].max_value = value;
      }
    }
    elec_stats[ix].sum_values += value;
    elec_stats[ix].nb_samples += 1;
  }
}

const uint32_t get_elec_stat_min_value(int ix) {
  if ((ix >= 0) && ix < NB_TELE_STAT_FIELD) {
    return elec_stats[ix].min_value;
  }
  return 0;
}
const uint32_t get_elec_stat_max_value(int ix) {
  if ((ix >= 0) && ix < NB_TELE_STAT_FIELD) {
    return elec_stats[ix].max_value;
  }
  return 0;
}

const uint32_t get_elec_stat_mean_value(int ix) {
  if ((ix >= 0) && ix < NB_TELE_STAT_FIELD) {
    if (elec_stats[ix].nb_samples == 0) return 0;
    return elec_stats[ix].sum_values / elec_stats[ix].nb_samples;
  }
  return 0;
}

const uint16_t get_elec_stat_nb_samples(int ix) {
  if ((ix >= 0) && ix < NB_TELE_STAT_FIELD) {
    return elec_stats[ix].nb_samples;
  }
  return 0;
}
