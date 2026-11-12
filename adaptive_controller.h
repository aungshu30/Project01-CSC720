/**
 * @file adaptive_controller.h
 * @brief Header for Milestone 3 Adaptive Policy Controller (Sirisha's Task)
 */

#ifndef ADAPTIVE_CONTROLLER_H
#define ADAPTIVE_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// Candidate budgets calibrated in Milestone 2
#define BUDGET_ZERO   0
#define BUDGET_SHORT  1000  
#define BUDGET_LONG   5000   

typedef struct {
    uint32_t current_budget;
    uint64_t cpu_allowance_limit;
    uint64_t measured_checking_time;
    uint32_t consecutive_productive_hits;
    uint32_t probe_counter;
    FILE *log_file;
} AdaptiveController;

// Function declarations
void controller_init(AdaptiveController *ctrl, uint64_t allowance_limit, const char *log_path);
uint32_t controller_decide_budget(AdaptiveController *ctrl, bool hit_productive, uint64_t elapsed_cpu);
void controller_close(AdaptiveController *ctrl);

#endif // ADAPTIVE_CONTROLLER_H