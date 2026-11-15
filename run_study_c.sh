#!/bin/bash

# Create folder for Study C results
mkdir -p results/study_c

TEST_FILE="/tmp/study_c_test.bin"
TRACE="results/study_c/changing_trace.csv"
CPUS="0,1"

# Step 1: Measure solo baseline of the CPU task (10 seconds alone on CPUs 0,1)
echo "Running solo baseline for competing CPU task..."
taskset -c $CPUS ./study_c_task > results/study_c/study_c_solo_baseline.txt &
SOLO_PID=$!
sleep 10
kill -INT $SOLO_PID
wait $SOLO_PID
cat results/study_c/study_c_solo_baseline.txt

# Step 2: Run Study C across policies (3 repetitions each)
# Currently 'wait' is active in run_bench; add poll, fixed, adaptive once teammates merge them
for policy in wait poll fixed adaptive
do
    for rep in 1 2 3
    do
        echo "=== Study C | Policy: $policy | Repetition: $rep ==="

        # Start CPU task in background on the same CPU set
        taskset -c $CPUS ./study_c_task > results/study_c/compute_${policy}_rep${rep}.txt &
        CPU_PID=$!

        # Run storage benchmark on the changing trace on the same CPU set
        taskset -c $CPUS ./run_bench --file $TEST_FILE --trace $TRACE --policy $policy --direct 1 --depth 16 --out results/study_c/storage_${policy}_rep${rep}.csv

        # Stop the CPU task after the storage trace finishes
        kill -INT $CPU_PID
        wait $CPU_PID
    done
done

echo "All Study C runs finished! Check results/study_c/"