/* verify.c  (Task 1.6, owner: Medur) */
#include "verify.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>

static inline uint64_t mix64(uint64_t z)
{
    z = (z ^ (z >> 30)) * 0xBF58476D1CE4E5B9ULL;
    z = (z ^ (z >> 27)) * 0x94D049BB133111EBULL;
    return z ^ (z >> 31);
}

uint64_t pattern_word(uint64_t seed, uint64_t file_offset)
{
    return mix64(seed + (file_offset >> 3) * 0x9E3779B97F4A7C15ULL);
}

void pattern_fill(uint64_t seed, void *buf, uint64_t file_offset,
                  size_t len)
{
    unsigned char *p = buf;
    for (size_t i = 0; i + 8 <= len; i += 8) {
        uint64_t w = pattern_word(seed, file_offset + i);
        memcpy(p + i, &w, 8);
    }
}

verify_result_t verify_read(uint64_t seed, const void *buf,
                            uint64_t file_offset, size_t expected_len,
                            int cqe_res)
{
    verify_result_t r = { .status = VERIFY_OK, .got_bytes = cqe_res };

    if (cqe_res < 0) {
        r.status = VERIFY_IO_ERROR;
        r.err = -cqe_res;
        return r;
    }
    if ((size_t)cqe_res < expected_len) {
        r.status = VERIFY_SHORT_READ;
        return r;
    }
    if ((size_t)cqe_res > expected_len || file_offset % 8 ||
        expected_len % 8 || !buf) {
        r.status = VERIFY_BAD_ARGS;
        return r;
    }

    const unsigned char *p = buf;
    for (size_t i = 0; i < expected_len; i += 8) {
        uint64_t got, want = pattern_word(seed, file_offset + i);
        memcpy(&got, p + i, 8);
        if (got != want) {
            r.status = VERIFY_MISMATCH;
            r.bad_offset = file_offset + i;
            r.expected = want;
            r.actual = got;
            return r;
        }
    }
    return r;
}

const char *verify_status_str(verify_status_t s)
{
    switch (s) {
    case VERIFY_OK:         return "ok";
    case VERIFY_IO_ERROR:   return "io_error";
    case VERIFY_SHORT_READ: return "short_read";
    case VERIFY_MISMATCH:   return "mismatch";
    case VERIFY_BAD_ARGS:   return "bad_args";
    }
    return "unknown";
}

void verify_print(FILE *out, uint64_t req_id, const verify_result_t *r)
{
    switch (r->status) {
    case VERIFY_OK:
        break;
    case VERIFY_IO_ERROR:
        fprintf(out, "req %llu: I/O error: %s\n",
                (unsigned long long)req_id, strerror(r->err));
        break;
    case VERIFY_SHORT_READ:
        fprintf(out, "req %llu: short read, got %lld bytes\n",
                (unsigned long long)req_id, (long long)r->got_bytes);
        break;
    case VERIFY_MISMATCH:
        fprintf(out, "req %llu: wrong data at offset %llu "
                "(expected %016llx, got %016llx)\n",
                (unsigned long long)req_id,
                (unsigned long long)r->bad_offset,
                (unsigned long long)r->expected,
                (unsigned long long)r->actual);
        break;
    case VERIFY_BAD_ARGS:
        fprintf(out, "req %llu: bad arguments or oversized result %lld\n",
                (unsigned long long)req_id, (long long)r->got_bytes);
        break;
    }
}

/* ---- exactly-once tracker ---- */
enum { ST_NEW = 0, ST_IN_FLIGHT = 1, ST_DONE = 2 };

int ct_init(ct_t *ct, size_t n)
{
    memset(ct, 0, sizeof(*ct));
    ct->state = calloc(n ? n : 1, 1);
    if (!ct->state)
        return -ENOMEM;
    ct->n = n;
    return 0;
}

ct_status_t ct_submit(ct_t *ct, uint64_t id)
{
    if (id >= ct->n)
        return CT_UNKNOWN_ID;
    if (ct->state[id] != ST_NEW)
        return CT_DUPLICATE_SUBMIT;
    ct->state[id] = ST_IN_FLIGHT;
    ct->in_flight++;
    return CT_OK;
}

ct_status_t ct_complete(ct_t *ct, uint64_t id)
{
    if (id >= ct->n)
        return CT_UNKNOWN_ID;
    if (ct->state[id] == ST_NEW)
        return CT_NOT_SUBMITTED;
    if (ct->state[id] == ST_DONE)
        return CT_DUPLICATE_COMPLETION;
    ct->state[id] = ST_DONE;
    ct->in_flight--;
    ct->completed++;
    return CT_OK;
}

const char *ct_status_str(ct_status_t s)
{
    switch (s) {
    case CT_OK:                   return "ok";
    case CT_UNKNOWN_ID:           return "unknown_id";
    case CT_DUPLICATE_SUBMIT:     return "duplicate_submit";
    case CT_NOT_SUBMITTED:        return "not_submitted";
    case CT_DUPLICATE_COMPLETION: return "duplicate_completion";
    }
    return "unknown";
}

uint64_t ct_in_flight(const ct_t *ct)
{
    return ct->in_flight;
}

void ct_free(ct_t *ct)
{
    free(ct->state);
    memset(ct, 0, sizeof(*ct));
}
