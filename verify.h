/*
 * verify.h  (Task 1.6, owner: Medur)
 *
 * 1) Known data pattern for the test file. Every 8-byte word depends on
 *    its file position, so data read from the wrong offset is caught.
 *    Pasha's test file (task 1.2) must be created with pattern_fill()
 *    (or the mkpattern tool) using the SAME seed.
 * 2) verify_read(): checks one completed read (error, short read, bytes).
 * 3) Completion tracker: each request must complete exactly once.
 */
#ifndef VERIFY_H
#define VERIFY_H

#include <stdint.h>
#include <stddef.h>
#include <stdio.h>

#define PATTERN_DEFAULT_SEED 0x5EEDF11E5EEDF11EULL

uint64_t pattern_word(uint64_t seed, uint64_t file_offset);
/* file_offset and len must be multiples of 8 */
void pattern_fill(uint64_t seed, void *buf, uint64_t file_offset,
                  size_t len);

typedef enum {
    VERIFY_OK = 0,
    VERIFY_IO_ERROR,    /* cqe->res < 0 */
    VERIFY_SHORT_READ,  /* 0 <= cqe->res < expected length */
    VERIFY_MISMATCH,    /* right length, wrong bytes */
    VERIFY_BAD_ARGS     /* unaligned args or res > expected */
} verify_status_t;

typedef struct {
    verify_status_t status;
    int      err;        /* errno value for VERIFY_IO_ERROR */
    int64_t  got_bytes;  /* cqe->res */
    uint64_t bad_offset; /* first wrong word (VERIFY_MISMATCH) */
    uint64_t expected;
    uint64_t actual;
} verify_result_t;

/* cqe_res is the io_uring completion result (bytes or -errno). */
verify_result_t verify_read(uint64_t seed, const void *buf,
                            uint64_t file_offset, size_t expected_len,
                            int cqe_res);
const char *verify_status_str(verify_status_t s);
void verify_print(FILE *out, uint64_t req_id, const verify_result_t *r);

/* ---- exactly-once completion tracker ---- */
typedef enum {
    CT_OK = 0,
    CT_UNKNOWN_ID,           /* id >= n */
    CT_DUPLICATE_SUBMIT,     /* submitted twice */
    CT_NOT_SUBMITTED,        /* completion for something never sent */
    CT_DUPLICATE_COMPLETION  /* completed twice */
} ct_status_t;

typedef struct {
    uint8_t *state;      /* 0 = new, 1 = in flight, 2 = completed */
    size_t   n;
    uint64_t in_flight;
    uint64_t completed;
} ct_t;

int  ct_init(ct_t *ct, size_t n);
ct_status_t ct_submit(ct_t *ct, uint64_t id);
ct_status_t ct_complete(ct_t *ct, uint64_t id);
const char *ct_status_str(ct_status_t s);
/* At shutdown: reads still in flight (should be 0 after draining). */
uint64_t ct_in_flight(const ct_t *ct);
void ct_free(ct_t *ct);

#endif
