/**
 * @file metrics.h
 * @author Sirisha Konda
 * @course CSC 720: Storage Systems - Milestone 1
 * @brief Header file for high-resolution request timing and CSV metrics logging.
 */

#ifndef METRICS_H
#define METRICS_H

#include <stdint.h>
#include <stdio.h>
#include <time.h>

// Structure to track the precise lifecycle and performance of an individual read request
struct request_record {
    uint64_t request_id;
    uint64_t offset;
    uint32_t length;
    
    // Wall-clock timestamps using a monotonic clock (measured in nanoseconds)
    uint64_t arrival_time_ns;
    uint64_t submission_time_ns;
    uint64_t completion_time_ns;
    
    // Thread-level CPU time consumption for active processing checks
    uint64_t cpu_time_start_ns;
    uint64_t cpu_time_end_ns;
    
    int status_error; // 0 for success, non-zero error code otherwise
};

// Function prototypes for timing and logging operations
uint64_t get_monotonic_time_ns(void);
uint64_t get_cpu_time_ns(void);
void init_request_record(struct request_record *rec, uint64_t id, uint64_t offset, uint32_t len);
int write_metrics_to_csv(const char *filename, struct request_record *records, size_t count);

#endif