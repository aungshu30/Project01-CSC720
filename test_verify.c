/* Unit tests for byte verification and exactly-once tracking (Task 1.6) */
#include "../verify.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int fails = 0, checks = 0;
#define CHECK(c) do { checks++; if (!(c)) { fails++; \
    fprintf(stderr, "FAIL %s:%d: %s\n", __FILE__, __LINE__, #c); } } while (0)

#define BS   4096
#define SEED PATTERN_DEFAULT_SEED

static unsigned char buf[BS];

static void test_correct_bytes(void)
{
    pattern_fill(SEED, buf, 3 * BS, BS);
    verify_result_t r = verify_read(SEED, buf, 3 * BS, BS, BS);
    CHECK(r.status == VERIFY_OK);
}

static void test_flipped_byte(void)
{
    pattern_fill(SEED, buf, 0, BS);
    buf[1000] ^= 0x01;
    verify_result_t r = verify_read(SEED, buf, 0, BS, BS);
    CHECK(r.status == VERIFY_MISMATCH);
    CHECK(r.bad_offset == 1000 / 8 * 8);
}

static void test_wrong_offset_detected(void)
{
    pattern_fill(SEED, buf, 5 * BS, BS);          /* data of block 5 */
    verify_result_t r = verify_read(SEED, buf, 6 * BS, BS, BS); /* asked 6 */
    CHECK(r.status == VERIFY_MISMATCH);
    CHECK(r.bad_offset == 6 * BS);
}

static void test_wrong_seed_detected(void)
{
    pattern_fill(SEED + 1, buf, 0, BS);
    CHECK(verify_read(SEED, buf, 0, BS, BS).status == VERIFY_MISMATCH);
}

static void test_short_reads(void)
{
    pattern_fill(SEED, buf, 0, BS);
    verify_result_t r = verify_read(SEED, buf, 0, BS, 2048);
    CHECK(r.status == VERIFY_SHORT_READ && r.got_bytes == 2048);
    CHECK(verify_read(SEED, buf, 0, BS, 0).status == VERIFY_SHORT_READ);
}

static void test_errors(void)
{
    verify_result_t r = verify_read(SEED, buf, 0, BS, -EIO);
    CHECK(r.status == VERIFY_IO_ERROR && r.err == EIO);
    r = verify_read(SEED, buf, 0, BS, -EINVAL); /* e.g. bad O_DIRECT align */
    CHECK(r.status == VERIFY_IO_ERROR && r.err == EINVAL);
    CHECK(verify_read(SEED, buf, 0, BS, BS + 8).status == VERIFY_BAD_ARGS);
    CHECK(verify_read(SEED, buf, 3, BS, BS).status == VERIFY_BAD_ARGS);
    CHECK(verify_read(SEED, NULL, 0, BS, BS).status == VERIFY_BAD_ARGS);
}

/* Real file: full block reads pass, a read past EOF is a short read */
static void test_real_file(void)
{
    char path[] = "/tmp/vfileXXXXXX";
    int fd = mkstemp(path);
    CHECK(fd >= 0);
    size_t fsz = 2 * BS + BS / 2; /* 2.5 blocks: last block is partial */
    unsigned char *data = malloc(fsz);
    pattern_fill(SEED, data, 0, fsz);
    CHECK(write(fd, data, fsz) == (ssize_t)fsz);
    free(data);

    ssize_t n = pread(fd, buf, BS, BS);
    CHECK(verify_read(SEED, buf, BS, BS, (int)n).status == VERIFY_OK);

    n = pread(fd, buf, BS, 2 * BS);
    verify_result_t r = verify_read(SEED, buf, 2 * BS, BS, (int)n);
    CHECK(r.status == VERIFY_SHORT_READ && r.got_bytes == BS / 2);

    close(fd);
    unlink(path);
}

static void test_exactly_once(void)
{
    ct_t ct;
    CHECK(ct_init(&ct, 4) == 0);
    CHECK(ct_submit(&ct, 0) == CT_OK);
    CHECK(ct_submit(&ct, 1) == CT_OK);
    CHECK(ct_submit(&ct, 1) == CT_DUPLICATE_SUBMIT);
    CHECK(ct_in_flight(&ct) == 2);
    CHECK(ct_complete(&ct, 0) == CT_OK);
    CHECK(ct_complete(&ct, 0) == CT_DUPLICATE_COMPLETION);
    CHECK(ct_complete(&ct, 2) == CT_NOT_SUBMITTED);
    CHECK(ct_complete(&ct, 9) == CT_UNKNOWN_ID);
    CHECK(ct_submit(&ct, 9) == CT_UNKNOWN_ID);
    /* shutdown with a read still in flight is visible */
    CHECK(ct_in_flight(&ct) == 1);
    CHECK(ct_complete(&ct, 1) == CT_OK);
    CHECK(ct_in_flight(&ct) == 0 && ct.completed == 2);
    ct_free(&ct);
}

int main(void)
{
    test_correct_bytes();
    test_flipped_byte();
    test_wrong_offset_detected();
    test_wrong_seed_detected();
    test_short_reads();
    test_errors();
    test_real_file();
    test_exactly_once();
    printf("test_verify: %d checks, %d failed\n", checks, fails);
    return fails ? 1 : 0;
}
