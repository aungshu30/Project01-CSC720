// mkfile - create the test file with the known block pattern.
//   ./mkfile /path/on/LOCAL/ssd/testfile.bin 1024     (size in MiB)
// Put it on a local SSD (not NFS), otherwise the results are meaningless.
#include <cstdio>
#include <cstdlib>
#include <vector>

#include "pattern.h"

int main(int argc, char** argv) {
    if (argc != 3) { std::fprintf(stderr, "usage: mkfile PATH SIZE_MIB\n"); return 2; }
    unsigned long long mib = std::strtoull(argv[2], nullptr, 10);
    FILE* f = std::fopen(argv[1], "wb");
    if (!f) { std::perror("fopen"); return 1; }

    const size_t chunk = 1 << 20;  // write 1 MiB at a time
    std::vector<unsigned char> buf(chunk);
    for (unsigned long long m = 0; m < mib; m++) {
        pat::fill(buf.data(), m * chunk, chunk);
        if (std::fwrite(buf.data(), 1, chunk, f) != chunk) { std::perror("fwrite"); return 1; }
    }
    std::fclose(f);
    std::printf("wrote %llu MiB to %s\n", mib, argv[1]);
    return 0;
}
