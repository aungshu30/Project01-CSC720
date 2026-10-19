// trace_gen - write a seeded workload trace to CSV.
//
//   ./trace_gen --mode steady --rate 20000 --duration 10 --seed 1 --out steady.csv
//   ./trace_gen --mode bursty --sparse-rate 2000 --burst-rate 60000
//               --sparse-secs 5 --burst-secs 2 --seed 7 --out bursty.csv   (one line)
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <map>
#include <string>

#include "workload.h"

static void usage() {
    std::cerr <<
        "usage: trace_gen --mode steady|bursty --out FILE [options]\n"
        "  --seed N            (default 1)\n"
        "  --file-size BYTES   (default 1073741824)\n"
        "  --size BYTES        request size, multiple of 4096 (default 4096)\n"
        "  --dist poisson|fixed (default poisson)\n"
        "  steady: --rate R --duration S\n"
        "  bursty: --sparse-rate R --burst-rate R --sparse-secs S --burst-secs S\n";
}

int main(int argc, char** argv) {
    std::map<std::string, std::string> a;
    for (int i = 1; i + 1 < argc; i += 2) {
        if (std::strncmp(argv[i], "--", 2) != 0) { usage(); return 2; }
        a[argv[i] + 2] = argv[i + 1];
    }
    auto num = [&](const char* k, double d) { return a.count(k) ? std::atof(a[k].c_str()) : d; };
    auto need = [&](const char* k) {
        if (!a.count(k)) { std::cerr << "missing --" << k << "\n"; usage(); std::exit(2); }
    };

    need("mode"); need("out");
    wl::Config cfg;
    cfg.seed = static_cast<uint64_t>(num("seed", 1));
    cfg.file_size = static_cast<uint64_t>(num("file-size", 1073741824.0));
    cfg.req_size = static_cast<uint32_t>(num("size", 4096));
    cfg.poisson = !(a.count("dist") && a["dist"] == "fixed");

    try {
        std::vector<wl::Request> t;
        if (a["mode"] == "steady") {
            need("rate"); need("duration");
            t = wl::generate_steady(cfg, num("rate", 0), num("duration", 0));
        } else if (a["mode"] == "bursty") {
            need("sparse-rate"); need("burst-rate"); need("sparse-secs"); need("burst-secs");
            t = wl::generate_bursty(cfg, num("sparse-rate", 0), num("burst-rate", 0),
                                    num("sparse-secs", 0), num("burst-secs", 0));
        } else { usage(); return 2; }
        wl::write_csv(t, a["out"]);
        std::cout << "wrote " << t.size() << " requests to " << a["out"] << "\n";
    } catch (const std::exception& e) {
        std::cerr << "error: " << e.what() << "\n";
        return 1;
    }
    return 0;
}
