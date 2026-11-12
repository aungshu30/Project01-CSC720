#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <unistd.h>

// Simple CPU-bound workload for Study C competition
void* spin_cpu(void* arg) {
    // Infinite loop or controlled loop to consume CPU cycles
    while (1) {
        // Simple mathematical operation to keep CPU busy
        volatile unsigned long long i = 0;
        for (i = 0; i < 1000000; i++);
    }
    return NULL;
}

int main(int argc, char* argv[]) {
    printf("Starting Study C CPU-bound competing task...\n");

    pthread_t thread;
    // Create a worker thread or just run the busy loop directly in main
    if (pthread_create(&thread, NULL, spin_cpu, NULL) != 0) {
        perror("Failed to create thread");
        return 1;
    }

    // Keep the main process alive so taskset can monitor/pin it
    pthread_join(thread, NULL);

    return 0;
}