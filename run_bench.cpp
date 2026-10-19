// run_bench - run one policy over one trace and write per-request results.
//
//   ./run_bench --file /local/ssd/test.bin --trace steady.csv --policy wait --out run1.csv
//   ./run_bench --config bench.conf          (key=value lines; command line overrides the file)
//
// Config keys: file, trace, policy, depth, direct (1/0), verify (1/0), out
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>

#include "harness.h"
#include "policy_wait.h"

static std::vector<wl::Request> read_trace(const std::string& path) {
    std::ifstream f(path);
    if (!f) throw std::runtime_error("cannot open trace " + path);
    std::string line;
    std::getline(f, line);  // header: id,arrival_ns,offset,size
    std::vector<wl::Request> t;
    while (std::getline(f, line)) {
        if (line.empty()) continue;
        wl::Request r;
        unsigned long long id, a, o, s;
        if (std::sscanf(line.c_str(), "%llu,%llu,%llu,%llu", &id, &a, &o, &s) != 4)
            throw std::runtime_error("bad trace line: " + line);
        r.id = id; r.arrival_ns = a; r.offset = o; r.size = static_cast<uint32_t>(s);
        if (r.id != t.size()) throw std::runtime_error("trace ids must be 0,1,2,... in order");
        if (!t.empty() && r.arrival_ns < t.back().arrival_ns) throw std::runtime_error("trace arrivals go backwards");
        t.push_back(r);
    }
    return t;
}

static double pct(std::vector<uint64_t>& v, double p) {
    if (v.empty()) return 0;
    std::sort(v.begin(), v.end());
    size_t i = static_cast<size_t>(p * (v.size() - 1) + 0.5);
    return static_cast<double>(v[i]);
}

int main(int argc, char** argv) {
    std::map<std::string, std::string> cfg;
    // pass 1: find --config
    for (int i = 1; i + 1 < argc; i += 2)
        if (std::string(argv[i]) == "--config") {
            std::ifstream f(argv[i + 1]);
            if (!f) { std::cerr << "cannot open config " << argv[i + 1] << "\n"; return 2; }
            std::string line;
            while (std::getline(f, line)) {
                if (line.empty() || line[0] == '#') continue;
                auto eq = line.find('=');
                if (eq == std::string::npos) continue;
                cfg[line.substr(0, eq)] = line.substr(eq + 1);
            }
        }
    for (int i = 1; i + 1 < argc; i += 2) {
        std::string k = argv[i];
        if (k.rfind("--", 0) != 0) { std::cerr << "bad argument " << k << "\n"; return 2; }
        if (k != "--config") cfg[k.substr(2)] = argv[i + 1];
    }
    auto need = [&](const char* k) {
        if (!cfg.count(k)) { std::cerr << "missing --" << k << "\n"; std::exit(2); }
        return cfg[k];
    };

    try {
        HarnessOptions opt;
        opt.file = need("file");
        std::string tracef = need("trace");
        std::string policy = cfg.count("policy") ? cfg["policy"] : "wait";
        if (cfg.count("depth")) opt.depth = static_cast<unsigned>(std::atoi(cfg["depth"].c_str()));
        if (cfg.count("direct")) opt.direct = cfg["direct"] != "0";
        if (cfg.count("verify")) opt.verify = cfg["verify"] != "0";
        if (opt.depth == 0) throw std::runtime_error("depth must be at least 1");

        const Policy* pol = nullptr;
        if (policy == "wait") pol = &kPolicyWaitImmediately;
        else throw std::runtime_error("unknown policy '" + policy + "' (only 'wait' exists so far)");

        auto trace = read_trace(tracef);
        RunResult r = run_harness(opt, trace, *pol);

        if (cfg.count("out")) {
            std::ofstream o(cfg["out"]);
            o << "id,offset,size,arrival_ns,submit_ns,complete_ns,result,verify_ok\n";
            for (auto& x : r.records)
                o << x.id << ',' << x.offset << ',' << x.size << ',' << x.arrival_ns << ',' << x.submit_ns << ','
                  << x.complete_ns << ',' << x.result << ',' << int(x.verify_ok) << '\n';
        }

        std::vector<uint64_t> lat, resp;
        for (auto& x : r.records) {
            lat.push_back(x.complete_ns - x.submit_ns);    // device + completion latency
            resp.push_back(x.complete_ns - x.arrival_ns);  // includes any wait for a free slot
        }
        const double secs = r.wall_ns / 1e9;
        std::printf("policy            : %s\n", pol->name);
        std::printf("requests          : %zu\n", r.records.size());
        std::printf("errors            : %llu\n", (unsigned long long)r.errors);
        std::printf("short reads       : %llu\n", (unsigned long long)r.short_reads);
        std::printf("verify failures   : %llu\n", (unsigned long long)r.verify_failures);
        std::printf("wall time         : %.3f s\n", secs);
        std::printf("reads per second  : %.0f\n", r.records.size() / secs);
        std::printf("latency p50 / p99 : %.1f / %.1f us  (submit->complete)\n", pct(lat, 0.50) / 1e3, pct(lat, 0.99) / 1e3);
        std::printf("response p50 / p99: %.1f / %.1f us  (arrival->complete)\n", pct(resp, 0.50) / 1e3, pct(resp, 0.99) / 1e3);
        std::printf("CPU time per read : %.2f us\n", r.records.empty() ? 0.0 : r.cpu_ns / 1e3 / r.records.size());
        std::printf("policy wait calls : %llu\n", (unsigned long long)r.waits);
        return (r.errors || r.verify_failures) ? 1 : 0;
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
}
