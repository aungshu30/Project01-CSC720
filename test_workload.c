/* Unit tests for the workload generator (Task 1.5) */
#include "../workload.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

#define BS   4096u
#define FSZ  (64ull * 1024 * 1024)

static void test_same_seed_same_sequence(void)
{
    wl_t a, b, c;
    CHECK(wl_init_steady(&a, 42, 1000, 2, WL_ARRIVAL_POISSON, FSZ, BS) == 0);
    CHECK(wl_init_steady(&b, 42, 1000, 2, WL_ARRIVAL_POISSON, FSZ, BS) == 0);
    CHECK(wl_init_steady(&c, 43, 1000, 2, WL_ARRIVAL_POISSON, FSZ, BS) == 0);
    CHECK(a.n == b.n);
    CHECK(memcmp(a.reqs, b.reqs, a.n * sizeof(wl_req_t)) == 0);
    CHECK(a.n != c.n || memcmp(a.reqs, c.reqs, a.n * sizeof(wl_req_t)) != 0);
    wl_free(&a); wl_free(&b); wl_free(&c);
}

static void test_fixed_stream(void)
{
    wl_t w;
    CHECK(wl_init_steady(&w, 1, 2000, 1.5, WL_ARRIVAL_FIXED, FSZ, BS) == 0);
    CHECK(w.n == 3000);
    for (size_t i = 0; i < w.n; i++) {
        CHECK(w.reqs[i].id == i);
        CHECK(w.reqs[i].arrival_ns == i * 500000ull); /* 1/2000 s */
        CHECK(w.reqs[i].offset % BS == 0);
        CHECK(w.reqs[i].offset + BS <= FSZ);
    }
    wl_free(&w);
}

static void test_poisson_rate(void)
{
    wl_t w;
    CHECK(wl_init_steady(&w, 7, 10000, 5, WL_ARRIVAL_POISSON, FSZ, BS) == 0);
    double rate = w.n / 5.0;
    CHECK(rate > 9500 && rate < 10500);
    for (size_t i = 1; i < w.n; i++)
        CHECK(w.reqs[i].arrival_ns >= w.reqs[i - 1].arrival_ns);
    wl_free(&w);
}

/* Offsets must not change when only the arrival mode changes */
static void test_offsets_independent_of_arrival_mode(void)
{
    wl_t f, p;
    CHECK(wl_init_steady(&f, 9, 1000, 1, WL_ARRIVAL_FIXED, FSZ, BS) == 0);
    CHECK(wl_init_steady(&p, 9, 1000, 1, WL_ARRIVAL_POISSON, FSZ, BS) == 0);
    size_t m = f.n < p.n ? f.n : p.n;
    for (size_t i = 0; i < m; i++)
        CHECK(f.reqs[i].offset == p.reqs[i].offset);
    wl_free(&f); wl_free(&p);
}

/* Open loop: schedule is the same no matter how many slots are free,
 * and requests that find no slot are rejected and counted. */
static void test_open_loop_and_rejection(void)
{
    wl_t w;
    CHECK(wl_init_steady(&w, 5, 1000, 1, WL_ARRIVAL_FIXED, FSZ, BS) == 0);
    wl_req_t out[8];
    uint64_t now = 0, got = 0;
    size_t slots_pattern[] = { 0, 1, 8, 2, 0, 3 };
    int k = 0;
    while (!wl_done(&w)) {
        now += 3000000; /* 3 ms steps: about 3 arrivals per step */
        size_t n = wl_take_due(&w, now, slots_pattern[k++ % 6], out);
        for (size_t i = 0; i < n; i++) {
            CHECK(out[i].arrival_ns <= now);
            CHECK(out[i].arrival_ns == out[i].id * 1000000ull);
            CHECK(w.status[out[i].id] == WL_ACCEPTED);
        }
        got += n;
    }
    CHECK(got == w.accepted);
    CHECK(w.accepted + w.rejected == w.n);
    CHECK(w.rejected > 0);
    uint64_t rej = 0;
    for (size_t i = 0; i < w.n; i++)
        rej += w.status[i] == WL_REJECTED;
    CHECK(rej == w.rejected);
    wl_free(&w);
}

static void test_next_arrival(void)
{
    wl_t w;
    uint64_t t;
    CHECK(wl_init_steady(&w, 1, 10, 1, WL_ARRIVAL_FIXED, FSZ, BS) == 0);
    CHECK(wl_next_arrival(&w, &t) && t == 0);
    wl_req_t out[1];
    CHECK(wl_take_due(&w, 0, 1, out) == 1);
    CHECK(wl_next_arrival(&w, &t) && t == 100000000ull);
    CHECK(wl_take_due(&w, 99999999ull, 1, out) == 0); /* not yet due */
    wl_take_due(&w, ~0ull, 100, (wl_req_t[100]){0});
    CHECK(!wl_next_arrival(&w, &t));
    wl_free(&w);
}

static char *write_tmp(const char *text)
{
    static char path[64];
    strcpy(path, "/tmp/wltraceXXXXXX");
    int fd = mkstemp(path);
    if (fd < 0)
        return NULL;
    if (write(fd, text, strlen(text)) < 0) { close(fd); return NULL; }
    close(fd);
    return path;
}

static void test_trace_file(void)
{
    wl_t w;
    char *p = write_tmp(
        "# sparse then burst\n"
        "arrival_ns,offset\n"
        "0,0\n"
        "\n"
        "1000000,8192\n"
        "1000000,-1\n"
        "1000500,-1\n");
    CHECK(p != NULL);
    CHECK(wl_load_trace(&w, p, 3, FSZ, BS) == 0);
    CHECK(w.n == 4);
    CHECK(w.reqs[1].arrival_ns == 1000000 && w.reqs[1].offset == 8192);
    CHECK(w.reqs[2].offset % BS == 0 && w.reqs[3].offset + BS <= FSZ);

    /* round trip: save then load gives the same schedule */
    char out[] = "/tmp/wlroundXXXXXX";
    int fd = mkstemp(out);
    close(fd);
    CHECK(wl_write_trace(&w, out) == 0);
    wl_t w2;
    CHECK(wl_load_trace(&w2, out, 999, FSZ, BS) == 0);
    CHECK(w2.n == w.n && memcmp(w.reqs, w2.reqs, w.n * sizeof(wl_req_t)) == 0);
    wl_free(&w); wl_free(&w2);
    unlink(p); unlink(out);

    /* bad traces are rejected */
    const char *bad[] = { "5,0\n4,0\n",         /* time goes backwards */
                          "0,100\n",            /* unaligned offset */
                          "0,67108864\n",       /* past end of file */
                          "0;0\n" };            /* cannot parse */
    for (int i = 0; i < 4; i++) {
        p = write_tmp(bad[i]);
        CHECK(wl_load_trace(&w, p, 1, FSZ, BS) == -EINVAL);
        unlink(p);
    }
    CHECK(wl_load_trace(&w, "/nonexistent/x", 1, FSZ, BS) == -ENOENT);
}

int main(void)
{
    test_same_seed_same_sequence();
    test_fixed_stream();
    test_poisson_rate();
    test_offsets_independent_of_arrival_mode();
    test_open_loop_and_rejection();
    test_next_arrival();
    test_trace_file();
    printf("test_workload: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
