// harness.h - the io_uring read harness (plan task 1.3)
//
// One ring, a fixed limit on outstanding reads, aligned direct I/O, and ONE submission path
// that every policy shares. IOPOLL, SQPOLL and deferred task run are not enabled (flags = 0).
//
// Loop:  submit every request whose arrival time has come (while below the outstanding limit)
//        -> collect any completions that are already there -> if nothing to do, ask the policy
//        to wait (with a timeout equal to the time until the next arrival).
#pragma once
#include <fcntl.h>
#include <unistd.h>

#include <cerrno>
#include <cstdlib>
#include <cstring>
#include <stdexcept>
#include <string>
#include <vector>

#include "common.h"
#include "pattern.h"
#include "workload.h"

struct HarnessOptions {
    std::string file;
    unsigned depth = 32;     // fixed maximum number of outstanding reads
    bool direct = true;      // O_DIRECT; turn off only for quick tests on filesystems without it
    bool verify = true;      // check bytes against the known pattern
};

struct RunResult {
    std::vector<Record> records;  // indexed by request id
    uint64_t wall_ns = 0;
    uint64_t cpu_ns = 0;          // process CPU time (user+system) for the whole run
    uint64_t errors = 0;          // result < 0
    uint64_t short_reads = 0;     // 0 <= result < size
    uint64_t verify_failures = 0; // bytes did not match the pattern
    uint64_t waits = 0;           // times the policy's wait() was called
};

inline uint64_t process_cpu_ns() {
    timespec ts;
    clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &ts);
    return static_cast<uint64_t>(ts.tv_sec) * 1000000000ull + static_cast<uint64_t>(ts.tv_nsec);
}

inline RunResult run_harness(const HarnessOptions& opt, const std::vector<wl::Request>& trace, const Policy& policy) {
    RunResult out;
    const size_t n = trace.size();
    out.records.resize(n);
    for (size_t i = 0; i < n; i++) {
        out.records[i].id = trace[i].id;
        out.records[i].offset = trace[i].offset;
        out.records[i].size = trace[i].size;
        out.records[i].arrival_ns = trace[i].arrival_ns;
    }
    if (n == 0) return out;

    int fd = open(opt.file.c_str(), O_RDONLY | (opt.direct ? O_DIRECT : 0));
    if (fd < 0) throw std::runtime_error("open " + opt.file + ": " + std::strerror(errno));

    uint32_t max_size = 0;
    for (auto& r : trace) if (r.size > max_size) max_size = r.size;

    // One aligned buffer per outstanding slot. A buffer stays untouched until its completion
    // has been processed (the slot is only returned to the free list after that).
    std::vector<void*> bufs(opt.depth, nullptr);
    for (unsigned s = 0; s < opt.depth; s++)
        if (posix_memalign(&bufs[s], 4096, max_size) != 0) throw std::runtime_error("posix_memalign");
    std::vector<unsigned> free_slots;
    for (unsigned s = opt.depth; s-- > 0;) free_slots.push_back(s);
    std::vector<unsigned> slot_of(n, 0);

    struct io_uring ring;
    int rc = io_uring_queue_init(opt.depth, &ring, 0);  // flags 0: no IOPOLL, no SQPOLL, no DEFER_TASKRUN
    if (rc < 0) throw std::runtime_error(std::string("io_uring_queue_init: ") + std::strerror(-rc));

    const uint64_t cpu0 = process_cpu_ns();
    const uint64_t t0 = now_ns();
    size_t next = 0, done = 0, inflight = 0;

    auto handle = [&](struct io_uring_cqe* cqe) {
        const uint64_t t = now_ns() - t0;
        const size_t idx = static_cast<size_t>(io_uring_cqe_get_data64(cqe));
        Record& r = out.records[idx];
        r.complete_ns = t;
        r.result = cqe->res;
        const unsigned slot = slot_of[idx];
        io_uring_cqe_seen(&ring, cqe);  // only now may the slot (buffer) be reused
        if (r.result < 0) out.errors++;
        else {
            if (static_cast<uint32_t>(r.result) < r.size) out.short_reads++;
            if (opt.verify) {
                r.verify_ok = pat::verify(bufs[slot], r.offset, static_cast<size_t>(r.result)) == -1;
                if (!r.verify_ok) out.verify_failures++;
            } else r.verify_ok = 1;
        }
        free_slots.push_back(slot);
        inflight--;
        done++;
    };

    while (done < n) {
        uint64_t now = now_ns() - t0;

        // 1. Common submission path: submit everything that has arrived and fits.
        unsigned queued = 0;
        while (next < n && trace[next].arrival_ns <= now && !free_slots.empty()) {
            struct io_uring_sqe* sqe = io_uring_get_sqe(&ring);
            if (!sqe) break;
            const unsigned slot = free_slots.back();
            free_slots.pop_back();
            slot_of[next] = slot;
            io_uring_prep_read(sqe, fd, bufs[slot], trace[next].size, trace[next].offset);
            io_uring_sqe_set_data64(sqe, next);
            out.records[next].submit_ns = now_ns() - t0;
            next++;
            inflight++;
            queued++;
        }
        if (queued) {
            int s = io_uring_submit(&ring);
            if (s < 0) throw std::runtime_error(std::string("io_uring_submit: ") + std::strerror(-s));
        }

        // 2. Collect completions that are already available (no waiting).
        struct io_uring_cqe* cqe = nullptr;
        bool got = false;
        while (io_uring_peek_cqe(&ring, &cqe) == 0) { handle(cqe); got = true; }
        if (got || done >= n) continue;

        // 3. Nothing to do: let the policy wait. If another request is due, wake up for it.
        int64_t timeout = -1;
        if (next < n && !free_slots.empty()) {
            now = now_ns() - t0;
            timeout = trace[next].arrival_ns > now ? static_cast<int64_t>(trace[next].arrival_ns - now) : 0;
        }
        out.waits++;
        int w = policy.wait(&ring, &cqe, timeout);
        if (w == 0 && cqe) handle(cqe);
        else if (w != -ETIME && w != -EINTR && w != -EAGAIN)
            throw std::runtime_error(std::string("policy wait failed: ") + std::strerror(-w));
    }

    out.wall_ns = now_ns() - t0;
    out.cpu_ns = process_cpu_ns() - cpu0;
    io_uring_queue_exit(&ring);
    for (void* b : bufs) free(b);
    close(fd);
    return out;
}
