/* workload.c  (Task 1.5, owner: Medur) */
#include "workload.h"

#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---- seeded RNG (splitmix64): same seed gives same sequence ---- */
static uint64_t rng_next(uint64_t *s)
{
    uint64_t z = (*s += 0x9E3779B97F4A7C15ULL);
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

static double rng_unit(uint64_t *s) /* uniform in [0,1) */
{
    return (double)(rng_next(s) >> 11) * 0x1.0p-53;
}

/* Separate streams so changing the arrival mode never changes offsets */
#define OFFSET_STREAM  0xA5A5A5A5A5A5A5A5ULL
#define ARRIVAL_STREAM 0x3C3C3C3C3C3C3C3CULL

static uint64_t random_offset(uint64_t *s, uint64_t nblocks, uint32_t bs)
{
    return (rng_next(s) % nblocks) * (uint64_t)bs;
}

static int check_geometry(uint64_t file_size, uint32_t block_size)
{
    if (block_size == 0 || file_size < block_size)
        return -EINVAL;
    return 0;
}

static int grow(wl_t *wl, size_t *cap)
{
    size_t nc = *cap ? *cap * 2 : 1024;
    wl_req_t *r = realloc(wl->reqs, nc * sizeof(*r));
    if (!r)
        return -ENOMEM;
    wl->reqs = r;
    *cap = nc;
    return 0;
}

static int finish(wl_t *wl)
{
    wl->status = calloc(wl->n ? wl->n : 1, 1);
    if (!wl->status) {
        free(wl->reqs);
        wl->reqs = NULL;
        return -ENOMEM;
    }
    wl->next = 0;
    wl->accepted = 0;
    wl->rejected = 0;
    return 0;
}

int wl_init_steady(wl_t *wl, uint64_t seed, double rate_per_s,
                   double duration_s, wl_arrival_t arrival,
                   uint64_t file_size, uint32_t block_size)
{
    memset(wl, 0, sizeof(*wl));
    if (check_geometry(file_size, block_size) || rate_per_s <= 0 ||
        duration_s <= 0)
        return -EINVAL;

    wl->file_size = file_size;
    wl->block_size = block_size;

    uint64_t s_off = seed ^ OFFSET_STREAM;
    uint64_t s_arr = seed ^ ARRIVAL_STREAM;
    uint64_t nblocks = file_size / block_size;
    double end_ns = duration_s * 1e9;
    double gap_ns = 1e9 / rate_per_s;
    double t = 0.0;
    size_t cap = 0;

    for (;;) {
        if (arrival == WL_ARRIVAL_FIXED)
            t = (double)wl->n * gap_ns;
        else if (wl->n > 0) /* exponential gap: -ln(U) * mean */
            t += -log(1.0 - rng_unit(&s_arr)) * gap_ns;
        if (t >= end_ns)
            break;
        if (wl->n == cap && grow(wl, &cap)) {
            free(wl->reqs);
            wl->reqs = NULL;
            return -ENOMEM;
        }
        wl_req_t *r = &wl->reqs[wl->n];
        r->id = wl->n;
        r->arrival_ns = (uint64_t)llround(t);
        r->offset = random_offset(&s_off, nblocks, block_size);
        wl->n++;
    }
    return finish(wl);
}

int wl_load_trace(wl_t *wl, const char *path, uint64_t seed,
                  uint64_t file_size, uint32_t block_size)
{
    memset(wl, 0, sizeof(*wl));
    if (check_geometry(file_size, block_size))
        return -EINVAL;

    FILE *f = fopen(path, "r");
    if (!f)
        return -errno;

    wl->file_size = file_size;
    wl->block_size = block_size;

    uint64_t s_off = seed ^ OFFSET_STREAM;
    uint64_t nblocks = file_size / block_size;
    uint64_t last = 0;
    size_t cap = 0;
    char line[256];
    int lineno = 0, rc = 0;

    while (fgets(line, sizeof(line), f)) {
        lineno++;
        char *p = line;
        while (isspace((unsigned char)*p))
            p++;
        if (*p == '\0' || *p == '#' || isalpha((unsigned char)*p))
            continue; /* blank, comment or header */

        unsigned long long a;
        long long off;
        if (sscanf(p, "%llu , %lld", &a, &off) != 2) {
            fprintf(stderr, "trace %s:%d: cannot parse\n", path, lineno);
            rc = -EINVAL;
            break;
        }
        if (a < last) {
            fprintf(stderr, "trace %s:%d: arrival goes backwards\n",
                    path, lineno);
            rc = -EINVAL;
            break;
        }
        uint64_t o;
        if (off == -1) {
            o = random_offset(&s_off, nblocks, block_size);
        } else if (off < 0 || (uint64_t)off % block_size != 0 ||
                   (uint64_t)off + block_size > file_size) {
            fprintf(stderr, "trace %s:%d: bad offset %lld\n",
                    path, lineno, off);
            rc = -EINVAL;
            break;
        } else {
            o = (uint64_t)off;
        }
        if (wl->n == cap && (rc = grow(wl, &cap)))
            break;
        wl->reqs[wl->n] = (wl_req_t){ .id = wl->n, .arrival_ns = a,
                                      .offset = o };
        wl->n++;
        last = a;
    }
    fclose(f);
    if (rc) {
        free(wl->reqs);
        wl->reqs = NULL;
        wl->n = 0;
        return rc;
    }
    return finish(wl);
}

int wl_write_trace(const wl_t *wl, const char *path)
{
    FILE *f = fopen(path, "w");
    if (!f)
        return -errno;
    fprintf(f, "arrival_ns,offset\n");
    for (size_t i = 0; i < wl->n; i++)
        fprintf(f, "%llu,%llu\n",
                (unsigned long long)wl->reqs[i].arrival_ns,
                (unsigned long long)wl->reqs[i].offset);
    return fclose(f) ? -errno : 0;
}

size_t wl_take_due(wl_t *wl, uint64_t elapsed_ns, size_t free_slots,
                   wl_req_t *out)
{
    size_t k = 0;
    while (wl->next < wl->n &&
           wl->reqs[wl->next].arrival_ns <= elapsed_ns) {
        if (k < free_slots) {
            out[k++] = wl->reqs[wl->next];
            wl->status[wl->next] = WL_ACCEPTED;
            wl->accepted++;
        } else {
            wl->status[wl->next] = WL_REJECTED;
            wl->rejected++;
        }
        wl->next++;
    }
    return k;
}

bool wl_next_arrival(const wl_t *wl, uint64_t *arrival_ns)
{
    if (wl->next >= wl->n)
        return false;
    *arrival_ns = wl->reqs[wl->next].arrival_ns;
    return true;
}

bool wl_done(const wl_t *wl)
{
    return wl->next >= wl->n;
}

void wl_free(wl_t *wl)
{
    free(wl->reqs);
    free(wl->status);
    memset(wl, 0, sizeof(*wl));
}
