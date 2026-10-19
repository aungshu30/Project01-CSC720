# Project 01 - Pasha's Milestone 1 code

Tasks covered here: 1.1 shared interfaces, 1.3 io_uring harness, 1.4 policy 1 (wait immediately).
Also included (Medur's pieces, already written so you can test end to end): workload generator
(`trace_gen`), test-file maker (`mkfile`), pattern check (`src/pattern.h`).

## Files
    src/common.h       1.1  Record (CSV columns), Policy interface, clock
    src/harness.h      1.3  one ring, fixed outstanding limit, O_DIRECT, one shared submit path
    src/policy_wait.h  1.4  policy 1: io_uring_wait_cqe (+ timed wait for the next arrival)
    src/run_bench.cpp       command line: run one policy over one trace, write CSV, print summary
    tests/test_harness.sh   end-to-end checks (incl. corrupted-file detection)

## Build
    sudo apt install liburing-dev        # if you have sudo; then just:  make
    # no sudo? build liburing in your home folder:
    git clone https://github.com/axboe/liburing.git ~/liburing && cd ~/liburing
    ./configure --prefix=$HOME/liburing-inst && make -j4 && make install
    cd <this folder> && make LIBURING=$HOME/liburing-inst

## Test (needs a folder on a LOCAL disk that supports O_DIRECT)
    make test                      # add LIBURING=... if needed
    TESTDIR=/local/ssd/tmp_test make test

## Run
    ./mkfile /local/ssd/testfile.bin 1024
    ./trace_gen --mode steady --rate 20000 --duration 10 --seed 1 --file-size 1073741824 --out steady.csv
    ./run_bench --file /local/ssd/testfile.bin --trace steady.csv --policy wait --depth 32 --out run1.csv

## Adding the other policies
Each policy is one function with the signature in `common.h`
(`int wait(ring, &cqe, timeout_ns)`) plus a `Policy` entry; add its name in `run_bench.cpp`.
The harness never changes, so only the waiting differs between policies.

## Notes
- `file-size` for trace_gen must equal the test file size, or reads fall outside the file.
- Harness flags: no IOPOLL, no SQPOLL, no DEFER_TASKRUN.
- Latency columns: submit->complete is the device time; arrival->complete also includes any
  time spent waiting for a free slot when the outstanding limit is full.
- Keep the test file on a local disk (not NFS/home folder) and do not trust laptop/VM timings.
