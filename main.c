/**
 * @file main.c
 * @author Sirisha Konda
 * @course CSC 720: Storage Systems - Milestone 1
 * @brief Test harness and overhead validation for the measurement layer.
 */

#include <stdio.h>
#include <stdint.h>
#include "metrics.h"

/**
 * @brief Measures the overhead of calling the high-resolution monotonic clock.
 * This ensures that timestamp collection does not distort SSD performance data.
 */
void measure_logging_overhead(void) {
    const int iterations = 100000;
    uint64_t start_time = get_monotonic_time_ns();
    
    for (int i = 0; i < iterations; i++) {
        volatile uint64_t current_timestamp = get_monotonic_time_ns();
        (void)current_timestamp; // Suppress unused variable warning
    }
    
    uint64_t end_time = get_monotonic_time_ns();
    double total_elapsed_ns = (double)(end_time - start_time);
    double avg_overhead_ns = total_elapsed_ns / iterations;
    
    printf("[Benchmark] Average timestamp overhead: %.2f nanoseconds per call\n", avg_overhead_ns);
}

int main(void) {
    printf("=== Initializing Milestone 1 Measurement Layer Tests ===\n");

    // Step 1: Validate timestamp overhead
    measure_logging_overhead();

    // Step 2: Initialize a mock request record to simulate an SSD read lifecycle
    struct request_record sample_record;
    uint64_t mock_req_id = 1001;
    uint64_t mock_offset = 4096; // 4 KiB aligned read
    uint32_t mock_length = 4096;

    init_request_record(&sample_record, mock_req_id, mock_offset, mock_length);

    // Simulate timeline progression using monotonic clock
    sample_record.arrival_time_ns = get_monotonic_time_ns();
    sample_record.submission_time_ns = sample_record.arrival_time_ns + 150;     // Simulated queue wait
    sample_record.completion_time_ns = sample_record.submission_time_ns + 1200; // Simulated SSD read latency
    sample_record.cpu_time_start_ns = 50;
    sample_record.cpu_time_end_ns = 120;
    sample_record.status_error = 0; // Success

    // Step 3: Test CSV export functionality
    struct request_record records_batch[1] = { sample_record };
    const char *output_filename = "test_output.csv";
    
    int result = write_metrics_to_csv(output_filename, records_batch, 1);

    if (result == 0) {
        printf("[Success] Metrics successfully written to '%s'.\n", output_filename);
    } else {
        fprintf(stderr, "[Error] Failed to write metrics to CSV.\n");
        return 1;
    }

    printf("=== All Measurement Layer Tests Passed Successfully ===\n");
    return 0;
}