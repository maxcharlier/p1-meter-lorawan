#ifndef _VOLTAGE_STAT_H_
#define _VOLTAGE_STAT_H_

#include <stdint.h>

#define NB_PHASES 1

#define OVER_VOLTAGE_PHASE_1_INDEX 0
#define OVER_VOLTAGE_PHASE_2_INDEX 1
#define OVER_VOLTAGE_PHASE_3_INDEX 2

// Synergrid C10/11 / EN 50549-1 thresholds for Belgium (230V nominal)
// Stage 1: 1.10 × 230V = 253V, trip time 1.5s (adjustable)
// Stage 2: 1.15 × 230V = 264.5V, trip time 0.2s (instant)
#define STAGE1_OVERVOLTAGE_DV 2530  // 253.0V in decivolts
#define STAGE2_OVERVOLTAGE_DV 2645  // 264.5V in decivolts

void reset_over_voltage_counters();
void update_over_voltage_counter(int ix, uint32_t voltage_dv);
uint16_t get_nb_samples_over_stage1(int ix);
uint16_t get_nb_samples_over_stage2(int ix);
uint16_t get_max_consecutive_over_stage1(int ix);

#endif
