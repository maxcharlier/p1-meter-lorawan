/**
 * Voltage statistics for Synergrid C10/11 / EN 50549-1 compliance monitoring
 * 
 * Belgium grid protection thresholds:
 * - Stage 1 (U>):  253.0V (1.10 × Un) - trip after 1.5s sustained
 * - Stage 2 (U>>): 264.5V (1.15 × Un) - instant trip (0.2s)
 * 
 * This module tracks:
 * - Total samples exceeding each threshold
 * - Maximum consecutive samples over Stage 1 (to detect sustained overvoltage)
 */

#include <stdint.h>
#include "voltage_stat.h"

class OverVoltageCounters {
public:
  uint16_t total_samples_over_stage1;       // Total count over 253V
  uint16_t total_samples_over_stage2;       // Total count over 264.5V (instant trip)
  uint16_t consecutive_over_stage1;         // Current streak over 253V
  uint16_t max_consecutive_over_stage1;     // Longest streak this period

  OverVoltageCounters()
    : total_samples_over_stage1(0), 
      total_samples_over_stage2(0),
      consecutive_over_stage1(0),
      max_consecutive_over_stage1(0) {};
};

static OverVoltageCounters over_voltage_counters[NB_PHASES];

void reset_over_voltage_counters() {
  for (int i = 0; i < NB_PHASES; ++i) {
    over_voltage_counters[i] = OverVoltageCounters();
  }
}

void update_over_voltage_counter(int ix, uint32_t voltage_dv) {
  if (ix < 0 || ix >= NB_PHASES) {
    return;
  }

  // Stage 1: 253V - sustained overvoltage (1.5s trip time)
  if (voltage_dv >= STAGE1_OVERVOLTAGE_DV) {
    over_voltage_counters[ix].total_samples_over_stage1++;
    over_voltage_counters[ix].consecutive_over_stage1++;
    
    // Track maximum consecutive streak
    if (over_voltage_counters[ix].consecutive_over_stage1 > 
        over_voltage_counters[ix].max_consecutive_over_stage1) {
      over_voltage_counters[ix].max_consecutive_over_stage1 = 
        over_voltage_counters[ix].consecutive_over_stage1;
    }
  } else {
    // Voltage dropped below threshold, reset consecutive counter
    over_voltage_counters[ix].consecutive_over_stage1 = 0;
  }

  // Stage 2: 264.5V - instant trip
  if (voltage_dv >= STAGE2_OVERVOLTAGE_DV) {
    over_voltage_counters[ix].total_samples_over_stage2++;
  }
}

uint16_t get_nb_samples_over_stage1(int ix) {
  if (ix < 0 || ix >= NB_PHASES) {
    return 0;
  }
  return over_voltage_counters[ix].total_samples_over_stage1;
}

uint16_t get_nb_samples_over_stage2(int ix) {
  if (ix < 0 || ix >= NB_PHASES) {
    return 0;
  }
  return over_voltage_counters[ix].total_samples_over_stage2;
}

uint16_t get_max_consecutive_over_stage1(int ix) {
  /**
   * Returns the longest streak of consecutive samples over 253V.
   * If this value >= 2 (with 1 sample/second), the inverter likely tripped
   * due to sustained overvoltage (C10/11 Stage 1 trip time is 1.5s).
   */
  if (ix < 0 || ix >= NB_PHASES) {
    return 0;
  }
  return over_voltage_counters[ix].max_consecutive_over_stage1;
}
