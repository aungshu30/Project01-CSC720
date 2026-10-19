/**
 * @file metrics.c
 * @author Sirisha Konda
 * @course CSC 720: Storage Systems - Milestone 1
 * @brief Implementation of high-precision nanosecond timing and CSV export functions.
 */

#include "metrics.h"

/**
 * @brief Fetches monotonic wall-clock time in nanoseconds.
 * Using CLOCK_MONOTONIC ensures timestamps are unaffected by system clock adjustments.
 */
uint64_t get_monotonic_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * @brief Fetches thread-level CPU execution time.
 * Useful for measuring active CPU cycles spent spinning or processing requests.
 */
uint64_t get_cpu_time_ns(void) {
    struct timespec ts;
    clock_gettime(CLOCK_THREAD_CPUTIME_ID, &ts);
    return (uint64_t)ts.tv_sec * 1000000000ULL + (uint64_t)ts.tv_nsec;
}

/**
 * @brief Initializes a request record struct with default benchmark values.
 */
void init_request_record(struct request_record *rec, uint64_t id, uint64_t offset, uint32_t len) {
    rec->request_id = id;
    rec->offset = offset;
    rec->length = len;
    rec->arrival_time_ns = 0;
    rec->submission_time_ns = 0;
    rec->completion_time_ns = 0;
    rec->cpu_time_start_ns = 0;
    rec->cpu_time_end_ns = 0;
    rec->status_error = 0;
}

/**
 * @brief Dumps all completed request metrics into a CSV file for analysis.
 */
int write_metrics_to_csv(const char *filename, struct request_record *records, size_t count) {
    FILE *fp = fopen(filename, "w");
    if (fp == NULL) {
        perror("Error opening metrics CSV file for writing");
        return -1;
    }

    // Write CSV header matching the shared schema
    fprintf(fp, "request_id,offset,length,arrival_ns,submission_ns,completion_ns,cpu_start_ns,cpu_end_ns,status_error\n");

    // Iterate through request records and output comma-separated values
    for (size_t i = 0; i < count; i++) {
        fprintf(fp, "%lu,%lu,%u,%lu,%lu,%lu,%lu,%lu,%d\n",
                records[i].request_id,
                records[i].offset,
                records[i].length,
                records[i].arrival_time_ns,
                records[i].submission_time_ns,
                records[i].completion_time_ns,
                records[i].cpu_time_start_ns,
                records[i].cpu_time_end_ns,
                records[i].status_error);
    }

    fclose(fp);
    return 0;
}