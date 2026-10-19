// workload.h - seeded, repeatable read-request traces (Role A, Milestone 1 task 1.4)
//
// A trace is a list of (id, arrival_ns, offset, size). It is generated up front and
// does NOT depend on when reads complete, so every policy replays identical traffic.
//
// Reproducibility: std::mt19937_64 is fully specified by the C++ standard, and we
// avoid std::*_distribution (their output can differ between standard libraries).
// Same seed => same trace on every machine.
#pragma once
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <random>
#include <stdexcept>
#include <string>
#include <vector>

namespace wl {

struct Request {
    uint64_t id;          // 0,1,2,... in arrival order
    uint64_t arrival_ns;  // intended arrival time, relative to run start
    uint64_t offset;      // byte offset in the test file (aligned to size)
    uint32_t size;        // bytes to read
};

// One segment of load: run at `rate` requests/second for `duration_s` seconds.
struct Phase {
    double duration_s;
    double rate;  // requests per second; 0 means idle
};

struct Config {
    uint64_t seed = 1;
    uint64_t file_size = 1ull << 30;  // bytes
    uint32_t req_size = 4096;         // must be a multiple of 4096 (direct I/O)
    bool poisson = true;              // true: random gaps; false: evenly spaced
};

inline void validate(const Config& c) {
    if (c.req_size == 0 || c.req_size % 4096 != 0)
        throw std::invalid_argument("req_size must be a non-zero multiple of 4096");
    if (c.file_size < c.req_size)
        throw std::invalid_argument("file_size must be at least req_size");
}

// Uniform double in [0,1) from raw 64-bit output (53 bits of precision).
inline double u01(std::mt19937_64& g) {
    return static_cast<double>(g() >> 11) * (1.0 / 9007199254740992.0);
}

// Generate a trace from a list of phases. Offsets are uniform over the file and
// aligned to req_size (so aligned direct I/O is always valid).
inline std::vector<Request> generate(const Config& cfg, const std::vector<Phase>& phases) {
    validate(cfg);
    std::mt19937_64 rng(cfg.seed);
    const uint64_t nslots = cfg.file_size / cfg.req_size;

    std::vector<Request> out;
    double t = 0.0;          // seconds
    double phase_start = 0.0;
    uint64_t id = 0;

    for (const Phase& p : phases) {
        const double phase_end = phase_start + p.duration_s;
        if (p.rate > 0.0) {
            // Exponential gaps are memoryless, so restarting at each phase boundary
            // is statistically correct.
            t = phase_start;
            for (;;) {
                double gap = cfg.poisson ? -std::log(1.0 - u01(rng)) / p.rate : 1.0 / p.rate;
                t += gap;
                if (t >= phase_end) break;
                Request r;
                r.id = id++;
                r.arrival_ns = static_cast<uint64_t>(t * 1e9);
                r.offset = (rng() % nslots) * cfg.req_size;
                r.size = cfg.req_size;
                out.push_back(r);
            }
        }
        phase_start = phase_end;
    }
    return out;
}

// Steady stream: one phase.
inline std::vector<Request> generate_steady(const Config& cfg, double rate, double duration_s) {
    return generate(cfg, {{duration_s, rate}});
}

// Changing trace for Study B: sparse -> burst -> sparse.
inline std::vector<Request> generate_bursty(const Config& cfg, double sparse_rate, double burst_rate,
                                            double sparse_s, double burst_s) {
    return generate(cfg, {{sparse_s, sparse_rate}, {burst_s, burst_rate}, {sparse_s, sparse_rate}});
}

inline void write_csv(const std::vector<Request>& v, const std::string& path) {
    FILE* f = std::fopen(path.c_str(), "w");
    if (!f) throw std::runtime_error("cannot open " + path);
    std::fprintf(f, "id,arrival_ns,offset,size\n");
    for (const Request& r : v)
        std::fprintf(f, "%llu,%llu,%llu,%u\n", (unsigned long long)r.id,
                     (unsigned long long)r.arrival_ns, (unsigned long long)r.offset, r.size);
    std::fclose(f);
}

}  // namespace wl
