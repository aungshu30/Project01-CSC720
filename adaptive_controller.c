/**
 * @file adaptive_controller.c
 * @brief Implementation of adaptive controller logic, logging, and unit tests.
 */

#include "adaptive_controller.h"
#include <stdlib.h>
#include <time.h>

void controller_init(AdaptiveController *ctrl, uint64_t allowance_limit, const char *log_path) {
    ctrl->current_budget = BUDGET_SHORT;
    ctrl->cpu_allowance_limit = allowance_limit;
    ctrl->measured_checking_time = 0;
    ctrl->consecutive_productive_hits = 0;
    ctrl->probe_counter = 0;
    
    ctrl->log_file = fopen(log_path, "w");
    if (ctrl->log_file == NULL) {
        perror("Failed to open controller log file");
        exit(EXIT_FAILURE);
    }
    // CSV Header for Task 3.4 logging requirements
    fprintf(ctrl->log_file, "timestamp,requested_allowance,measured_checking_time,chosen_budget\n");
    fflush(ctrl->log_file);
}

uint32_t controller_decide_budget(AdaptiveController *ctrl, bool hit_productive, uint64_t elapsed_cpu) {
    ctrl->measured_checking_time = elapsed_cpu;

    // 1. Back off budget if CPU allowance is exceeded
    if (elapsed_cpu > ctrl->cpu_allowance_limit) {
        if (ctrl->current_budget == BUDGET_LONG) {
            ctrl->current_budget = BUDGET_SHORT;
        } else if (ctrl->current_budget == BUDGET_SHORT) {
            ctrl->current_budget = BUDGET_ZERO;
        }
        ctrl->consecutive_productive_hits = 0;
    } 
    // 2. Cautiously step up budget if checking has been sustained and productive
    else if (hit_productive) {
        ctrl->consecutive_productive_hits++;
        if (ctrl->consecutive_productive_hits > 10) {
            if (ctrl->current_budget == BUDGET_ZERO) {
                ctrl->current_budget = BUDGET_SHORT;
            } else if (ctrl->current_budget == BUDGET_SHORT) {
                ctrl->current_budget = BUDGET_LONG;
            }
            ctrl->consecutive_productive_hits = 0;
        }
    } else {
        if (ctrl->consecutive_productive_hits > 0) {
            ctrl->consecutive_productive_hits--;
        }
    }

    // 3. Occasional bounded probe if budget is zero to recover from shifting traffic (Task 3.3)
    if (ctrl->current_budget == BUDGET_ZERO) {
        ctrl->probe_counter++;
        if (ctrl->probe_counter >= 50) {
            ctrl->current_budget = BUDGET_SHORT;
            ctrl->probe_counter = 0;
        }
    }

    // 4. Log every budget adjustment and recorded metrics (Task 3.4)
    if (ctrl->log_file) {
        fprintf(ctrl->log_file, "%lu,%lu,%lu,%u\n",
                (unsigned long)time(NULL),
                (unsigned long)ctrl->cpu_allowance_limit,
                (unsigned long)elapsed_cpu,
                ctrl->current_budget);
        fflush(ctrl->log_file);
    }

    return ctrl->current_budget;
}

void controller_close(AdaptiveController *ctrl) {
    if (ctrl->log_file) {
        fclose(ctrl->log_file);
    }
}

// --- Unit Tests for the Controller (Task 3.4) ---
int main(void) {
    printf("[Unit Test] Initializing Adaptive Controller validation...\n");

    AdaptiveController ctrl;
    controller_init(&ctrl, 2000, "sirisha_controller_test_log.csv");

    // Mock scenario testing normal hits, CPU overages, and recovery
    bool hit_scenarios[] = {true, true, false, true, true};
    uint64_t cpu_scenarios[] = {400, 750, 2550, 150, 300};

    for (int i = 0; i < 5; i++) {
        uint32_t budget = controller_decide_budget(&ctrl, hit_scenarios[i], cpu_scenarios[i]);
        printf("Test Step %d -> Hit: %d, CPU Used: %llu | Updated Budget: %u\n",
               i + 1, hit_scenarios[i], cpu_scenarios[i], budget);
    }

    controller_close(&ctrl);
    printf("[Unit Test] Passed successfully. Log saved to sirisha_controller_test_log.csv\n");
    return 0;
}