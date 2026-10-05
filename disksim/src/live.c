/*
 * live.c - multi-process I/O scheduling simulation
 *
 *   client processes  --(POSIX shared memory ring buffer)-->  scheduler (parent)
 *
 * - shm_open() + ftruncate() + mmap()  : shared request queue
 * - sem_init(pshared=1) in shared mem : mutex / items / spaces (bounded buffer)
 * - fork() / waitpid() / kill()        : client processes
 * - sigaction(SIGINT)                  : Ctrl+C stops cleanly and removes the shm object
 * - nanosleep()                        : disk service time from the timing model
 * - clock_gettime(CLOCK_MONOTONIC)     : arrival / wait times (shared across processes)
 */
#include "disksim.h"
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <semaphore.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <time.h>
#include <unistd.h>

#define QCAP 32

typedef struct {
    sem_t     mutex, items, spaces;
    int       qhead, qtail;
    int       next_id;
    int       done_clients;
    double    t0;
    request_t buf[QCAP];
} shmq_t;

static volatile sig_atomic_t g_stop = 0;
static void on_sigint(int sig) { (void)sig; g_stop = 1; }

static double now_ms(void)
{
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1e3 + (double)ts.tv_nsec / 1e6;
}

static void sleep_ms(double ms)
{
    if (ms <= 0) return;
    struct timespec ts = { (time_t)(ms / 1000.0), (long)(fmod(ms, 1000.0) * 1e6) };
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR && !g_stop) { }
}

static void client_main(shmq_t *q, int k, int disk_size, unsigned seed)
{
    signal(SIGINT, SIG_IGN);            /* the parent decides when to stop */
    srand(seed);

    for (int j = 0; j < k; j++) {
        sleep_ms(100 + rand() % 400);   /* think time between requests */
        request_t r = { .cyl = rand() % disk_size, .src = getpid() };

        sem_wait(&q->spaces);
        sem_wait(&q->mutex);
        r.id = q->next_id++;
        r.arrival_ms = now_ms() - q->t0;
        q->buf[q->qtail] = r;
        q->qtail = (q->qtail + 1) % QCAP;
        sem_post(&q->mutex);
        sem_post(&q->items);
    }

    sem_wait(&q->mutex);
    q->done_clients++;
    sem_post(&q->mutex);
    _exit(0);
}

static void print_queue(const request_t *p, int n)
{
    printf("queue[%d]:", n);
    for (int i = 0; i < n && i < 12; i++) printf(" %d", p[i].cyl);
    if (n > 12) printf(" ...");
    putchar('\n');
}

int run_live(algo_t algo, int clients, int per_client, double scale,
             const disk_cfg_t *cfg, unsigned seed)
{
    if (clients * per_client > MAX_REQ) {
        fprintf(stderr, "clients x requests must be <= %d\n", MAX_REQ);
        return -1;
    }

    char name[64];
    snprintf(name, sizeof name, "/disksim_%d", getpid());

    int fd = shm_open(name, O_CREAT | O_EXCL | O_RDWR, 0600);
    if (fd < 0) { perror("shm_open"); return -1; }
    if (ftruncate(fd, sizeof(shmq_t)) < 0) { perror("ftruncate"); shm_unlink(name); return -1; }
    shmq_t *q = mmap(NULL, sizeof(shmq_t), PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    close(fd);
    if (q == MAP_FAILED) { perror("mmap"); shm_unlink(name); return -1; }

    memset(q, 0, sizeof *q);
    sem_init(&q->mutex, 1, 1);
    sem_init(&q->items, 1, 0);
    sem_init(&q->spaces, 1, QCAP);
    q->t0 = now_ms();

    struct sigaction sa;
    memset(&sa, 0, sizeof sa);
    sa.sa_handler = on_sigint;
    sigemptyset(&sa.sa_mask);
    sigaction(SIGINT, &sa, NULL);

    printf("Scheduler pid %d, algorithm %s, shared memory /dev/shm%s\n",
           getpid(), algo_name(algo), name);
    printf("Starting %d client processes x %d requests (Ctrl+C to stop)\n\n", clients, per_client);
    fflush(stdout);

    pid_t *pids = calloc((size_t)clients, sizeof *pids);
    for (int c = 0; c < clients; c++) {
        pid_t pid = fork();
        if (pid < 0) { perror("fork"); g_stop = 1; break; }
        if (pid == 0) client_main(q, per_client, cfg->disk_size, seed + (unsigned)c * 7919u);
        pids[c] = pid;
        printf("  client %d started (pid %d)\n", c, pid);
    }
    putchar('\n');

    static request_t pend[MAX_REQ];
    static result_t  res;
    int np = 0;
    step_t pre[4];
    int npre;
    double sum_wait = 0, max_wait = 0;
    int max_wait_id = -1;

    memset(&res, 0, sizeof res);
    res.algo = algo;
    sched_state_t st = { cfg->head, cfg->dir };
    res.steps[res.nsteps++] = (step_t){ st.head, STEP_START, -1 };

    while (!g_stop) {
        /* 1. move everything that has arrived into the pending list */
        while (sem_trywait(&q->items) == 0) {
            sem_wait(&q->mutex);
            request_t r = q->buf[q->qhead];
            q->qhead = (q->qhead + 1) % QCAP;
            sem_post(&q->mutex);
            sem_post(&q->spaces);
            if (np < MAX_REQ) pend[np++] = r;
            printf("[%7.0f ms] + req#%-3d cyl %-4d from pid %d (issued at %.0f ms)\n",
                   now_ms() - q->t0, r.id, r.cyl, r.src, r.arrival_ms);
        }

        /* 2. nothing pending: finished, or wait for the next arrival */
        if (np == 0) {
            sem_wait(&q->mutex);
            int done = q->done_clients;
            sem_post(&q->mutex);
            if (done == clients) {
                if (sem_trywait(&q->items) == 0) { sem_post(&q->items); continue; }
                break;
            }
            struct timespec ts;
            clock_gettime(CLOCK_REALTIME, &ts);
            ts.tv_nsec += 100 * 1000000L;
            if (ts.tv_nsec >= 1000000000L) { ts.tv_sec++; ts.tv_nsec -= 1000000000L; }
            if (sem_timedwait(&q->items, &ts) == 0) sem_post(&q->items);
            continue;
        }

        /* 3. let the algorithm choose, then "perform" the I/O */
        int i = pick_next(algo, &st, pend, np, cfg, pre, &npre);
        for (int k = 0; k < npre; k++) {
            long d = result_advance(&res, &st, cfg, pre[k]);
            sleep_ms(move_time_ms(cfg, d) * scale);
            printf("             %s to %d\n", pre[k].kind == STEP_JUMP ? "jump" : "sweep", pre[k].cyl);
        }
        request_t r = pend[i];
        long d = result_advance(&res, &st, cfg, (step_t){ r.cyl, STEP_SERVICE, r.id });
        sleep_ms((move_time_ms(cfg, d) + rot_latency_ms(cfg)) * scale);

        double t = now_ms() - q->t0;
        double wait = t - r.arrival_ms;   /* response time: issued -> done */
        sum_wait += wait;
        if (wait > max_wait) { max_wait = wait; max_wait_id = r.id; }

        memmove(&pend[i], &pend[i + 1], (size_t)(np - i - 1) * sizeof *pend);
        np--;

        printf("[%7.0f ms] > req#%-3d cyl %-4d seek %-4ld resp %6.0f ms  ", t, r.id, r.cyl, d, wait);
        print_queue(pend, np);
        fflush(stdout);
    }

    if (g_stop) {
        printf("\nSIGINT received, stopping clients...\n");
        for (int c = 0; c < clients; c++) if (pids[c] > 0) kill(pids[c], SIGTERM);
    }
    for (int c = 0; c < clients; c++) {
        if (pids[c] <= 0) continue;
        int status;
        while (waitpid(pids[c], &status, 0) < 0 && errno == EINTR) { }
    }

    sem_destroy(&q->mutex);
    sem_destroy(&q->items);
    sem_destroy(&q->spaces);
    munmap(q, sizeof *q);
    shm_unlink(name);
    free(pids);

    if (res.serviced > 0) {
        draw_result(&res, cfg, 0);
        printf("Avg response  : %.0f ms\n", sum_wait / res.serviced);
        printf("Max response  : %.0f ms (req#%d)\n", max_wait, max_wait_id);
    }
    return 0;
}
