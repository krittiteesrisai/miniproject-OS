/*
 * compare.c - run every algorithm in its own child process
 *
 * fork() one child per algorithm; each child writes a small summary into a
 * shared pipe (writes <= PIPE_BUF are atomic, so messages never interleave).
 * The parent collects them in arrival order, then reaps children with waitpid().
 */
#include "disksim.h"
#include <errno.h>
#include <fcntl.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/wait.h>
#include <unistd.h>

typedef struct {
    int    algo;
    pid_t  pid;
    long   total_seek;
    int    serviced;
    double est_ms;
} summary_t;

_Static_assert(sizeof(summary_t) <= PIPE_BUF, "summary must fit in one atomic pipe write");

static int read_full(int fd, void *buf, size_t len)
{
    size_t got = 0;
    while (got < len) {
        ssize_t k = read(fd, (char *)buf + got, len - got);
        if (k < 0) { if (errno == EINTR) continue; return -1; }
        if (k == 0) return 0;
        got += (size_t)k;
    }
    return 1;
}

int run_compare(const request_t *reqs, int n, const disk_cfg_t *cfg, const char *csv)
{
    int fd[2];
    pid_t pids[ALG_COUNT];

    if (pipe(fd) < 0) { perror("pipe"); return -1; }
    fflush(stdout);   /* avoid duplicated stdio buffers in children */

    for (int a = 0; a < ALG_COUNT; a++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); return -1; }
        if (pid == 0) {
            close(fd[0]);
            result_t *r = malloc(sizeof *r);
            if (!r) _exit(1);
            run_batch((algo_t)a, reqs, n, cfg, r);
            summary_t s = { a, getpid(), r->total_seek, r->serviced, r->est_time_ms };
            if (write(fd[1], &s, sizeof s) != (ssize_t)sizeof s) _exit(1);
            _exit(0);
        }
        pids[a] = pid;
    }
    close(fd[1]);

    printf("Parent pid %d forked %d children, reading results from pipe:\n", getpid(), ALG_COUNT);
    summary_t res[ALG_COUNT];
    int have[ALG_COUNT] = { 0 };
    summary_t s;
    int got = 0;
    while (got < ALG_COUNT && read_full(fd[0], &s, sizeof s) == 1) {
        if (s.algo < 0 || s.algo >= ALG_COUNT) continue;
        res[s.algo] = s;
        have[s.algo] = 1;
        got++;
        printf("  <- child %-7d finished %-7s total seek %ld\n", s.pid, algo_name(s.algo), s.total_seek);
    }
    close(fd[0]);

    for (int a = 0; a < ALG_COUNT; a++) {
        int status;
        while (waitpid(pids[a], &status, 0) < 0 && errno == EINTR) { }
        if (!WIFEXITED(status) || WEXITSTATUS(status) != 0)
            fprintf(stderr, "child %d (%s) failed\n", pids[a], algo_name(a));
    }
    if (got != ALG_COUNT) { fprintf(stderr, "only %d/%d results received\n", got, ALG_COUNT); return -1; }

    long best = res[0].total_seek, worst = res[0].total_seek;
    for (int a = 1; a < ALG_COUNT; a++) {
        if (res[a].total_seek < best)  best  = res[a].total_seek;
        if (res[a].total_seek > worst) worst = res[a].total_seek;
    }

    int barw = term_cols() - 62;
    if (barw < 10) barw = 10;
    int color = use_color();

    printf("\n%d requests, head=%d, dir=%s, disk=0..%d\n\n", n, cfg->head,
           cfg->dir == DIR_UP ? "up" : "down", cfg->disk_size - 1);
    printf("%-8s %11s %10s %12s  %s\n", "Algo", "Total seek", "Avg seek", "Est. time", "");
    for (int a = 0; a < ALG_COUNT; a++) {
        if (!have[a]) continue;
        int len = worst ? (int)(res[a].total_seek * barw / worst) : 0;
        int is_best = res[a].total_seek == best;
        printf("%-8s %11ld %10.2f %9.2f ms  ", algo_name(a), res[a].total_seek,
               res[a].serviced ? (double)res[a].total_seek / res[a].serviced : 0.0, res[a].est_ms);
        if (color && is_best) printf("\033[32m");
        for (int i = 0; i < len; i++) putchar('#');
        if (color && is_best) printf("\033[0m");
        printf(is_best ? " <- best\n" : "\n");
    }

    if (csv) {
        int out = open(csv, O_WRONLY | O_CREAT | O_TRUNC, 0644);
        if (out < 0) { perror(csv); return -1; }
        dprintf(out, "algorithm,total_seek,avg_seek,est_time_ms,requests,head,direction,disk_size\n");
        for (int a = 0; a < ALG_COUNT; a++)
            dprintf(out, "%s,%ld,%.2f,%.2f,%d,%d,%s,%d\n", algo_name(a), res[a].total_seek,
                    res[a].serviced ? (double)res[a].total_seek / res[a].serviced : 0.0,
                    res[a].est_ms, res[a].serviced, cfg->head,
                    cfg->dir == DIR_UP ? "up" : "down", cfg->disk_size);
        close(out);
        printf("\nSaved CSV to %s\n", csv);
    }
    return 0;
}
