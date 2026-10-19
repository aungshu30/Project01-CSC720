CXX      ?= g++
# If liburing is installed system-wide (sudo apt install liburing-dev) nothing else is needed.
# If you built it in your home folder:  make LIBURING=$HOME/liburing-inst
LIBURING ?=
ifneq ($(LIBURING),)
LURING_CF := -I$(LIBURING)/include
LURING_LF := -L$(LIBURING)/lib -Wl,-rpath,$(LIBURING)/lib
endif
CXXFLAGS ?= -std=c++17 -O2 -Wall -Wextra -I.
CXXFLAGS += $(LURING_CF)
LDLIBS   := $(LURING_LF) -luring

all: trace_gen mkfile test_workload run_bench

trace_gen: trace_gen.cpp workload.h
	$(CXX) $(CXXFLAGS) -o $@ trace_gen.cpp

mkfile: mkfile.cpp pattern.h
	$(CXX) $(CXXFLAGS) -o $@ mkfile.cpp

test_workload: test_workload.cpp workload.h pattern.h
	$(CXX) $(CXXFLAGS) -o $@ test_workload.cpp

run_bench: run_bench.cpp harness.h common.h policy_wait.h workload.h pattern.h
	$(CXX) $(CXXFLAGS) -o $@ run_bench.cpp $(LDLIBS)

test: test_workload run_bench trace_gen mkfile
	./test_workload
	sh test_harness.sh

clean:
	rm -f trace_gen mkfile test_workload run_bench

.PHONY: all test clean
