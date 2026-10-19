CXX      ?= g++
# If liburing is installed system-wide (sudo apt install liburing-dev) nothing else is needed.
# If you built it in your home folder:  make LIBURING=$HOME/liburing-inst
LIBURING ?=
ifneq ($(LIBURING),)
LURING_CF := -I$(LIBURING)/include
LURING_LF := -L$(LIBURING)/lib -Wl,-rpath,$(LIBURING)/lib
endif
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -Isrc
CXXFLAGS += $(LURING_CF)
LDLIBS   := $(LURING_LF) -luring

all: trace_gen mkfile test_workload run_bench

trace_gen: src/trace_gen.cpp src/workload.h
	$(CXX) $(CXXFLAGS) -o $@ src/trace_gen.cpp

mkfile: src/mkfile.cpp src/pattern.h
	$(CXX) $(CXXFLAGS) -o $@ src/mkfile.cpp

test_workload: tests/test_workload.cpp src/workload.h src/pattern.h
	$(CXX) $(CXXFLAGS) -o $@ tests/test_workload.cpp

run_bench: src/run_bench.cpp src/harness.h src/common.h src/policy_wait.h src/workload.h src/pattern.h
	$(CXX) $(CXXFLAGS) -o $@ src/run_bench.cpp $(LDLIBS)

test: test_workload run_bench trace_gen mkfile
	./test_workload
	sh tests/test_harness.sh

clean:
	rm -f trace_gen mkfile test_workload run_bench *.csv

.PHONY: all test clean
