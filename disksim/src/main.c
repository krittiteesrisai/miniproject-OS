/*
 * main.c - command line front end
 */
#include "disksim.h"
#include <getopt.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

static const char *DEFAULT_LIST = "98,183,37,122,14,124,65,67";   /* textbook example */

static void usage(const char *p)
{
    fprintf(stderr,
        "Disk Scheduling Simulator\n\n"
        "Usage:\n"
        "  %s run     -a ALG [input] [disk]   show one algorithm step by step\n"
        "  %s compare [input] [disk]          run all 6 algorithms in parallel (fork + pipe)\n"
        "  %s live    -a ALG [live] [disk]    client processes send requests via shared memory\n"
        "  %s info                            show real I/O schedulers from /sys/block\n\n"
        "ALG: fcfs sstf scan cscan look clook\n\n"
        "input:  -r LIST   e.g. -r 98,183,37,122   (default: textbook example)\n"
        "        -f FILE   numbers separated by spaces/commas, '#' comments\n"
        "        -n N      N random requests (-S SEED)\n"
        "disk:   -H HEAD   start position (default 53)\n"
        "        -D SIZE   number of cylinders (default 200)\n"
        "        -d DIR    up | down (default up)\n"
        "        -J        do not count the C-SCAN/C-LOOK return jump\n"
        "output: -A MS     animate, MS milliseconds per step (run)\n"
        "        -o FILE   save CSV (compare)\n"
        "        -q        print total seek only (run)\n"
        "live:   -c N      client processes (default 3)\n"
        "        -k N      requests per client (default 6)\n"
        "        -t X      slow disk time down X times so it can be watched (default 20)\n",
        p, p, p, p);
}

int main(int argc, char **argv)
{
    if (argc < 2 || !strcmp(argv[1], "-h") || !strcmp(argv[1], "--help")) { usage(argv[0]); return 1; }
    const char *cmd = argv[1];

    if (!strcmp(cmd, "info")) return show_sysinfo() == 0 ? 0 : 1;

    disk_cfg_t cfg = {
        .disk_size = 200, .head = 53, .dir = DIR_UP, .count_jump = 1,
        .settle_ms = 1.0, .seek_per_cyl_ms = 0.05, .rpm = 7200,
    };
    algo_t algo = ALG_FCFS;
    int have_algo = 0, rand_n = 0, anim = 0, quiet = 0, clients = 3, per_client = 6;
    unsigned seed = (unsigned)time(NULL);
    double scale = 20.0;
    const char *list = NULL, *file = NULL, *csv = NULL;

    int opt;
    optind = 1;
    while ((opt = getopt(argc - 1, argv + 1, "a:r:f:n:S:H:D:d:JA:o:qc:k:t:h")) != -1) {
        switch (opt) {
        case 'a':
            if (algo_parse(optarg, &algo) < 0) { fprintf(stderr, "unknown algorithm: %s\n", optarg); return 1; }
            have_algo = 1; break;
        case 'r': list = optarg; break;
        case 'f': file = optarg; break;
        case 'n': rand_n = atoi(optarg); break;
        case 'S': seed = (unsigned)strtoul(optarg, NULL, 10); break;
        case 'H': cfg.head = atoi(optarg); break;
        case 'D': cfg.disk_size = atoi(optarg); break;
        case 'd':
            if (!strcmp(optarg, "up")) cfg.dir = DIR_UP;
            else if (!strcmp(optarg, "down")) cfg.dir = DIR_DOWN;
            else { fprintf(stderr, "direction must be up or down\n"); return 1; }
            break;
        case 'J': cfg.count_jump = 0; break;
        case 'A': anim = atoi(optarg); break;
        case 'o': csv = optarg; break;
        case 'q': quiet = 1; break;
        case 'c': clients = atoi(optarg); break;
        case 'k': per_client = atoi(optarg); break;
        case 't': scale = atof(optarg); break;
        default: usage(argv[0]); return 1;
        }
    }

    if (cfg.disk_size < 2 || cfg.head < 0 || cfg.head >= cfg.disk_size) {
        fprintf(stderr, "head must be within 0..%d\n", cfg.disk_size - 1);
        return 1;
    }

    if (!strcmp(cmd, "live")) {
        if (!have_algo) { fprintf(stderr, "live needs -a ALG\n"); return 1; }
        if (clients < 1 || per_client < 1) { fprintf(stderr, "-c and -k must be >= 1\n"); return 1; }
        return run_live(algo, clients, per_client, scale, &cfg, seed) == 0 ? 0 : 1;
    }

    static request_t reqs[MAX_REQ];
    int n;
    if (file)            n = load_file(file, reqs, MAX_REQ);
    else if (rand_n > 0) n = gen_random(reqs, rand_n > MAX_REQ ? MAX_REQ : rand_n, cfg.disk_size, seed);
    else                 n = load_list(list ? list : DEFAULT_LIST, reqs, MAX_REQ);
    if (n <= 0) { fprintf(stderr, "no requests\n"); return 1; }

    for (int i = 0; i < n; i++)
        if (reqs[i].cyl >= cfg.disk_size) {
            fprintf(stderr, "request %d is outside the disk (0..%d)\n", reqs[i].cyl, cfg.disk_size - 1);
            return 1;
        }

    if (!strcmp(cmd, "run")) {
        if (!have_algo) { fprintf(stderr, "run needs -a ALG\n"); return 1; }
        static result_t r;
        run_batch(algo, reqs, n, &cfg, &r);
        if (quiet) printf("%ld\n", r.total_seek);
        else draw_result(&r, &cfg, anim);
        return 0;
    }
    if (!strcmp(cmd, "compare")) return run_compare(reqs, n, &cfg, csv) == 0 ? 0 : 1;

    usage(argv[0]);
    return 1;
}
