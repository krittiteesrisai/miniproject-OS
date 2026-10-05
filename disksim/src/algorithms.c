/*
 * algorithms.c - FCFS, SSTF, SCAN, C-SCAN, LOOK, C-LOOK
 *
 * Every algorithm is written as an "online" picker: given the requests that
 * are pending right now, choose the next one.  Batch mode (all requests known
 * up front) and live mode (requests arrive from other processes) share it.
 */
#include "disksim.h"
#include <ctype.h>
#include <stdlib.h>
#include <string.h>

static const char *NAMES[ALG_COUNT] = { "FCFS", "SSTF", "SCAN", "C-SCAN", "LOOK", "C-LOOK" };
static const char *KEYS[ALG_COUNT]  = { "fcfs", "sstf", "scan", "cscan", "look", "clook" };

const char *algo_name(algo_t a)
{
    return (a >= 0 && a < ALG_COUNT) ? NAMES[a] : "?";
}

int algo_parse(const char *s, algo_t *out)
{
    char buf[16];
    size_t j = 0;
    for (size_t i = 0; s[i] && j < sizeof buf - 1; i++)
        if (s[i] != '-' && s[i] != '_')
            buf[j++] = (char)tolower((unsigned char)s[i]);
    buf[j] = '\0';
    for (int a = 0; a < ALG_COUNT; a++)
        if (strcmp(buf, KEYS[a]) == 0) { *out = (algo_t)a; return 0; }
    return -1;
}

/* Closest request at or beyond `from` in direction `dir`, or -1. */
static int nearest_in_dir(const request_t *p, int n, int from, dir_t dir)
{
    int best = -1, bd = 0;
    for (int i = 0; i < n; i++) {
        int d = (p[i].cyl - from) * (int)dir;
        if (d >= 0 && (best < 0 || d < bd)) { best = i; bd = d; }
    }
    return best;
}

static int extreme(const request_t *p, int n, int want_min)
{
    int best = 0;
    for (int i = 1; i < n; i++)
        if (want_min ? p[i].cyl < p[best].cyl : p[i].cyl > p[best].cyl) best = i;
    return best;
}

/*
 * Returns the index in `pend` to service next.  Any movement the head must
 * make first (sweep to the edge, return jump) is written to `pre`.
 */
int pick_next(algo_t algo, sched_state_t *st, const request_t *p, int n,
              const disk_cfg_t *cfg, step_t *pre, int *npre)
{
    *npre = 0;
    if (n <= 0) return -1;

    int top = cfg->disk_size - 1;
    int i;

    switch (algo) {
    case ALG_FCFS:
        return 0;                                   /* arrival order */

    case ALG_SSTF: {
        int best = 0;
        for (i = 1; i < n; i++)
            if (abs(p[i].cyl - st->head) < abs(p[best].cyl - st->head)) best = i;
        return best;
    }

    case ALG_LOOK:
        i = nearest_in_dir(p, n, st->head, st->dir);
        if (i < 0) {                                /* reverse at last request */
            st->dir = (dir_t)(-st->dir);
            i = nearest_in_dir(p, n, st->head, st->dir);
        }
        return i;

    case ALG_SCAN:
        i = nearest_in_dir(p, n, st->head, st->dir);
        if (i < 0) {                                /* travel to the edge, then reverse */
            int edge = (st->dir == DIR_UP) ? top : 0;
            if (st->head != edge) pre[(*npre)++] = (step_t){ edge, STEP_EDGE, -1 };
            st->dir = (dir_t)(-st->dir);
            i = nearest_in_dir(p, n, edge, st->dir);
        }
        return i;

    case ALG_CSCAN:
        i = nearest_in_dir(p, n, st->head, st->dir);
        if (i < 0) {                                /* edge, jump to other edge, same dir */
            int e1 = (st->dir == DIR_UP) ? top : 0;
            int e2 = (st->dir == DIR_UP) ? 0 : top;
            if (st->head != e1) pre[(*npre)++] = (step_t){ e1, STEP_EDGE, -1 };
            pre[(*npre)++] = (step_t){ e2, STEP_JUMP, -1 };
            i = nearest_in_dir(p, n, e2, st->dir);
        }
        return i;

    case ALG_CLOOK:
        i = nearest_in_dir(p, n, st->head, st->dir);
        if (i < 0) {                                /* jump straight to farthest request */
            i = extreme(p, n, st->dir == DIR_UP);
            pre[(*npre)++] = (step_t){ p[i].cyl, STEP_JUMP, -1 };
        }
        return i;

    default:
        return -1;
    }
}

double move_time_ms(const disk_cfg_t *cfg, long dist)
{
    return dist > 0 ? cfg->settle_ms + cfg->seek_per_cyl_ms * (double)dist : 0.0;
}

double rot_latency_ms(const disk_cfg_t *cfg)
{
    return cfg->rpm > 0 ? 30000.0 / cfg->rpm : 0.0;  /* half a revolution */
}

/* Move the head, record the step, return the distance counted. */
long result_advance(result_t *r, sched_state_t *st, const disk_cfg_t *cfg, step_t s)
{
    long d = labs((long)s.cyl - st->head);
    if (s.kind == STEP_JUMP && !cfg->count_jump) d = 0;

    r->total_seek  += d;
    r->est_time_ms += move_time_ms(cfg, d);
    if (s.kind == STEP_SERVICE) {
        r->serviced++;
        r->est_time_ms += rot_latency_ms(cfg);
    }
    st->head = s.cyl;
    if (r->nsteps < MAX_STEPS) r->steps[r->nsteps++] = s;
    return d;
}

void run_batch(algo_t algo, const request_t *reqs, int n,
               const disk_cfg_t *cfg, result_t *r)
{
    static request_t pend[MAX_REQ];
    step_t pre[4];
    int npre, np = n;

    memcpy(pend, reqs, (size_t)n * sizeof *pend);
    memset(r, 0, sizeof *r);
    r->algo = algo;

    sched_state_t st = { cfg->head, cfg->dir };
    r->steps[r->nsteps++] = (step_t){ st.head, STEP_START, -1 };

    while (np > 0) {
        int i = pick_next(algo, &st, pend, np, cfg, pre, &npre);
        for (int k = 0; k < npre; k++) result_advance(r, &st, cfg, pre[k]);
        result_advance(r, &st, cfg, (step_t){ pend[i].cyl, STEP_SERVICE, pend[i].id });
        memmove(&pend[i], &pend[i + 1], (size_t)(np - i - 1) * sizeof *pend);
        np--;
    }
}
