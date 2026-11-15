#define _POSIX_C_SOURCE 200809L
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <pthread.h>
#include <time.h>

// Flag to stop the loop cleanly when the benchmark finishes
volatile sig_atomic_t running = 1;
unsigned long long completed_batches = 0;

void stop_handler(int sig) {
    (void)sig;
    running = 0;
}

double get_time_sec(void) {
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return ts.tv_sec + (ts.tv_nsec / 1e9);
}

// Worker thread that keeps the CPU busy doing math loops
void* cpu_worker(void* arg) {
    (void)arg;
    while (running) {
        volatile unsigned long long i;
        for (i = 0; i < 1000000; i++) {
            // Busy work to consume CPU cycles
        }
        completed_batches++;
    }
    return NULL;
}

int main(void) {
    // Catch SIGINT / SIGTERM so we can print the final count before exiting
    signal(SIGINT, stop_handler);
    signal(SIGTERM, stop_handler);

    double start_time = get_time_sec();

    pthread_t worker;
    if (pthread_create(&worker, NULL, cpu_worker, NULL) != 0) {
        perror("Error creating thread");
        return 1;
    }

    pthread_join(worker, NULL);

    double total_time = get_time_sec() - start_time;
    double work_rate = (total_time > 0) ? (completed_batches / total_time) : 0.0;

    printf("completed_batches=%llu, time_sec=%.3f, work_rate=%.2f batches/sec\n",
           completed_batches, total_time, work_rate);
    fflush(stdout);

    return 0;
}