#ifndef ADAPTIVE_CONTROLLER_H
#define ADAPTIVE_CONTROLLER_H

#include <stdint.h>
#include <stdbool.h>
#include <stdio.h>

// Calibrated spin budgets in nanoseconds (0, short, long)
#define BUDGET_ZERO  0
#define BUDGET_SHORT 2000
#define BUDGET_LONG  10000

typedef struct {
    uint32_t current_budget;
    uint64_t cpu_allowance_limit;
    uint64_t measured_checking_time;
    int consecutive_productive_hits;
    int probe_counter;
    FILE *log_file;
} AdaptiveController;

void controller_init(AdaptiveController *ctrl, uint64_t allowance_limit, const char *log_path);
uint32_t controller_decide_budget(AdaptiveController *ctrl, bool hit_productive, uint64_t elapsed_cpu);
void controller_close(AdaptiveController *ctrl);

#endif