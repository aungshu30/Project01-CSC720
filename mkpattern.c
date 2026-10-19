/*
 * mkpattern.c: create the test file with the known data pattern.
 * Give this to Pasha for task 1.2 so the file matches verify_read().
 *
 * Usage: ./mkpattern <path> <size_MiB> [seed_hex]
 */
#include "verify.h"

#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

int main(int argc, char **argv)
{
    if (argc < 3) {
        fprintf(stderr, "usage: %s <path> <size_MiB> [seed_hex]\n", argv[0]);
        return 2;
    }
    uint64_t size = strtoull(argv[2], NULL, 10) * 1024ULL * 1024ULL;
    uint64_t seed = argc > 3 ? strtoull(argv[3], NULL, 16)
                             : PATTERN_DEFAULT_SEED;

    int fd = open(argv[1], O_WRONLY | O_CREAT | O_TRUNC, 0644);
    if (fd < 0) {
        perror("open");
        return 1;
    }
    const size_t chunk = 1 << 20;
    unsigned char *buf = malloc(chunk);
    if (!buf) {
        perror("malloc");
        return 1;
    }
    for (uint64_t off = 0; off < size; off += chunk) {
        size_t len = size - off < chunk ? (size_t)(size - off) : chunk;
        pattern_fill(seed, buf, off, len);
        if (write(fd, buf, len) != (ssize_t)len) {
            perror("write");
            return 1;
        }
    }
    if (fsync(fd) || close(fd)) {
        perror("fsync/close");
        return 1;
    }
    printf("wrote %s: %llu bytes, seed %016llx\n", argv[1],
           (unsigned long long)size, (unsigned long long)seed);
    free(buf);
    return 0;
}
