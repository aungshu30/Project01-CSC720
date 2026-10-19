// policy_wait.h - policy 1: wait immediately (plan task 1.4)
//
// Never checks in a loop. If there is no completion yet, the thread sleeps inside the kernel
// (io_uring_wait_cqe) until one arrives. When the harness has no reads outstanding and the
// next request is not due yet, it passes a timeout equal to the time until that arrival, so
// this same call also does the "wait for the next arrival" sleep.
#pragma once
#include <cerrno>

#include "common.h"

inline int policy_wait_immediately(struct io_uring* ring, struct io_uring_cqe** cqe, int64_t timeout_ns) {
    *cqe = nullptr;
    if (timeout_ns < 0) return io_uring_wait_cqe(ring, cqe);
    struct __kernel_timespec ts;
    ts.tv_sec = timeout_ns / 1000000000;
    ts.tv_nsec = timeout_ns % 1000000000;
    return io_uring_wait_cqe_timeout(ring, cqe, &ts);
}

inline const Policy kPolicyWaitImmediately = {"wait_immediately", policy_wait_immediately};
