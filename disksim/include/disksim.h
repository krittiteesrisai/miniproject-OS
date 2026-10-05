/*
 * disksim.h - Disk Scheduling Simulator (shared definitions)
 */
#ifndef DISKSIM_H
#define DISKSIM_H

#include <sys/types.h>

#define MAX_REQ   1024
#define MAX_STEPS (MAX_REQ * 3 + 8)

typedef enum {
    ALG_FCFS = 0, ALG_SSTF, ALG_SCAN, ALG_CSCAN, ALG_LOOK, ALG_CLOOK, ALG_COUNT
} algo_t;

typedef enum { DIR_DOWN = -1, DIR_UP = 1 } dir_t;

/* One I/O request (a cylinder the head must visit). */
typedef struct {
    int    cyl;
    int    id;
    pid_t  src;          /* process that issued it (live mode) */
    double arrival_ms;   /* time since start (live mode)       */
} request_t;

typedef enum { STEP_START, STEP_SERVICE, STEP_EDGE, STEP_JUMP } step_kind_t;

/* One movement of the head. */
typedef struct {
    int         cyl;
    step_kind_t kind;
    int         req_id;
} step_t;

/* Disk geometry and timing model. */
typedef struct {
    int    disk_size;       /* cylinders 0 .. disk_size-1        */
    int    head;            /* starting head position            */
    dir_t  dir;             /* starting direction                */
    int    count_jump;      /* count C-SCAN/C-LOOK return jump   */
    double settle_ms;       /* fixed cost of any seek            */
    double seek_per_cyl_ms; /* cost per cylinder travelled       */
    int    rpm;             /* for average rotational latency    */
} disk_cfg_t;

typedef struct {
    algo_t algo;
    int    nsteps;
    step_t steps[MAX_STEPS];
    long   total_seek;
    int    serviced;
    double est_time_ms;
} result_t;

typedef struct { int head; dir_t dir; } sched_state_t;

/* algorithms.c */
const char *algo_name(algo_t a);
int    algo_parse(const char *s, algo_t *out);
int    pick_next(algo_t algo, sched_state_t *st, const request_t *pend, int n,
                 const disk_cfg_t *cfg, step_t *pre, int *npre);
long   result_advance(result_t *r, sched_state_t *st, const disk_cfg_t *cfg, step_t s);
void   run_batch(algo_t algo, const request_t *reqs, int n,
                 const disk_cfg_t *cfg, result_t *r);
double move_time_ms(const disk_cfg_t *cfg, long dist);
double rot_latency_ms(const disk_cfg_t *cfg);

/* input.c */
int load_file(const char *path, request_t *out, int max);
int load_list(const char *s, request_t *out, int max);
int gen_random(request_t *out, int n, int disk_size, unsigned seed);

/* visual.c */
int  term_cols(void);
int  use_color(void);
void draw_result(const result_t *r, const disk_cfg_t *cfg, int anim_ms);

/* compare.c */
int run_compare(const request_t *reqs, int n, const disk_cfg_t *cfg, const char *csv);

/* live.c */
int run_live(algo_t algo, int clients, int per_client, double scale,
             const disk_cfg_t *cfg, unsigned seed);

/* sysinfo.c */
int show_sysinfo(void);

#endif
