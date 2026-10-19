// Unit tests for the workload generator and the pattern checker. No framework needed.
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "pattern.h"
#include "workload.h"

static int failures = 0;
#define CHECK(cond, msg)                                        \
    do {                                                        \
        if (!(cond)) {                                          \
            std::printf("FAIL: %s  (line %d)\n", msg, __LINE__); \
            failures++;                                         \
        } else {                                                \
            std::printf("ok:   %s\n", msg);                     \
        }                                                       \
    } while (0)

static bool same(const std::vector<wl::Request>& a, const std::vector<wl::Request>& b) {
    if (a.size() != b.size()) return false;
    for (size_t i = 0; i < a.size(); i++)
        if (a[i].id != b[i].id || a[i].arrival_ns != b[i].arrival_ns || a[i].offset != b[i].offset ||
            a[i].size != b[i].size)
            return false;
    return true;
}

int main() {
    wl::Config cfg;
    cfg.seed = 42;
    cfg.file_size = 64ull << 20;  // 64 MiB

    // 1. Same seed -> identical trace; different seed -> different trace.
    auto a = wl::generate_steady(cfg, 10000, 2.0);
    auto b = wl::generate_steady(cfg, 10000, 2.0);
    cfg.seed = 43;
    auto c = wl::generate_steady(cfg, 10000, 2.0);
    cfg.seed = 42;
    CHECK(same(a, b), "same seed gives identical trace");
    CHECK(!same(a, c), "different seed gives different trace");

    // 2. Offsets aligned and inside the file; sizes right; ids sequential.
    bool aligned = true, inside = true, ids = true;
    for (size_t i = 0; i < a.size(); i++) {
        if (a[i].offset % cfg.req_size != 0) aligned = false;
        if (a[i].offset + a[i].size > cfg.file_size) inside = false;
        if (a[i].id != i) ids = false;
    }
    CHECK(aligned, "offsets are aligned to the request size");
    CHECK(inside, "every read stays inside the file");
    CHECK(ids, "ids are 0,1,2,... in order");

    // 3. Arrival times never go backwards and stay within the duration.
    bool mono = true, bounded = true;
    for (size_t i = 1; i < a.size(); i++)
        if (a[i].arrival_ns < a[i - 1].arrival_ns) mono = false;
    for (auto& r : a)
        if (r.arrival_ns >= 2000000000ull) bounded = false;
    CHECK(mono, "arrival times are non-decreasing");
    CHECK(bounded, "arrivals fall inside the requested duration");

    // 4. Request count is close to rate * duration (20000 expected, Poisson sd ~141).
    CHECK(a.size() > 19000 && a.size() < 21000, "steady count is near rate x duration");

    // 5. Fixed spacing gives exactly rate*duration - 1 evenly spaced requests.
    wl::Config fx = cfg;
    fx.poisson = false;
    auto f = wl::generate_steady(fx, 1000, 1.0);
    CHECK(f.size() == 999, "fixed-gap trace has the exact count");
    {
        bool even = f.size() > 2;
        for (size_t i = 1; i < f.size() && even; i++) {
            long long gap = (long long)f[i].arrival_ns - (long long)f[i - 1].arrival_ns;
            if (gap < 999999 || gap > 1000001) even = false;  // 1 ms, +/- 1 ns of rounding
        }
        CHECK(even, "fixed-gap trace is evenly spaced (1 ms apart)");
    }

    // 6. Bursty trace: the middle phase must be much denser than the outer phases.
    auto t = wl::generate_bursty(cfg, 1000, 50000, 2.0, 1.0);
    size_t n1 = 0, n2 = 0, n3 = 0;
    for (auto& r : t) {
        if (r.arrival_ns < 2000000000ull) n1++;
        else if (r.arrival_ns < 3000000000ull) n2++;
        else n3++;
    }
    CHECK(n2 > 10 * n1 && n2 > 10 * n3, "burst phase is far denser than sparse phases");
    CHECK(n1 > 0 && n3 > 0, "both sparse phases still have traffic");

    // 7. Bad settings are rejected instead of making invalid direct-I/O requests.
    wl::Config bad = cfg;
    bad.req_size = 1000;
    bool threw = false;
    try { wl::generate_steady(bad, 100, 1.0); } catch (const std::invalid_argument&) { threw = true; }
    CHECK(threw, "request size that is not a multiple of 4096 is rejected");

    // 8. Pattern: fill then verify passes; a flipped byte is caught at the right word.
    std::vector<unsigned char> buf(64 * 1024);
    pat::fill(buf.data(), 5 * 4096, buf.size());
    CHECK(pat::verify(buf.data(), 5 * 4096, buf.size()) == -1, "pattern verifies when correct");
    buf[8192 + 3] ^= 0xFF;
    CHECK(pat::verify(buf.data(), 5 * 4096, buf.size()) == 8192, "corrupt byte is found at the right word");
    CHECK(pat::verify(buf.data(), 6 * 4096, buf.size()) != -1, "data from the wrong offset is rejected");

    // 9. Short read: verifying only the bytes returned still passes on the prefix.
    pat::fill(buf.data(), 0, buf.size());
    CHECK(pat::verify(buf.data(), 0, 4096) == -1, "a short (partial) read verifies on the bytes returned");

    std::printf("\n%s (%d failure%s)\n", failures ? "TESTS FAILED" : "ALL TESTS PASSED", failures,
                failures == 1 ? "" : "s");
    return failures ? 1 : 0;
}
