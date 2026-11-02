#define _GNU_SOURCE
#include <sched.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>

// Thread function that performs heavy computations on a pinned CPU core
void* run_competing_workload(void *arg) {
    int target_core = *((int *)arg);
    cpu_set_t cpuset;
    
    // Clear and set the target core in the CPU set
    CPU_ZERO(&cpuset);
    CPU_SET(target_core, &cpuset);

    // Bind this thread to the specified core
    if (pthread_setaffinity_np(pthread_self(), sizeof(cpu_set_t), &cpuset) != 0) {
        perror("Failed to set CPU affinity");
        return NULL;
    }

    printf("Competing CPU task successfully pinned and running on core %d\n", target_core);

    uint64_t work_counter = 0;
    
    // Continuous loop to consume CPU cycles for Study C
    while (true) {
        for (volatile int i = 0; i < 1000000; i++) {
            work_counter += i;
        }
    }
    
    return NULL;
}

int main() {
    int core_to_pin = 2; // Choose the core matching your study setup
    pthread_t competitor_thread;

    // Create the background competing thread
    if (pthread_create(&competitor_thread, NULL, run_competing_workload, &core_to_pin) != 0) {
        perror("Failed to create competing thread");
        return 1;
    }

    // Keep the main process alive while the background thread runs
    pthread_join(competitor_thread, NULL);
    
    return 0;
}