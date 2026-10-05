/*
 * visual.c - draw the head-movement chart in the terminal
 *
 * Terminal width comes from ioctl(TIOCGWINSZ); animation uses nanosleep().
 */
#include "disksim.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#define LEFT 14   /* width of the "  #  cyl  k |" label column */

int term_cols(void)
{
    struct winsize ws;
    if (isatty(STDOUT_FILENO) && ioctl(STDOUT_FILENO, TIOCGWINSZ, &ws) == 0 && ws.ws_col >= 40)
        return ws.ws_col;
    return 80;
}

int use_color(void)
{
    return isatty(STDOUT_FILENO) && getenv("NO_COLOR") == NULL;
}

static int col_of(int cyl, int size, int w)
{
    return size <= 1 ? 0 : (int)((long)cyl * (w - 1) / (size - 1));
}

static void sleep_ms(int ms)
{
    if (ms <= 0) return;
    struct timespec ts = { ms / 1000, (long)(ms % 1000) * 1000000L };
    while (nanosleep(&ts, &ts) == -1 && errno == EINTR) { }
}

static void draw_axis(int size, int w)
{
    char line[1024];
    memset(line, '-', (size_t)w);
    line[w] = '\0';
    printf("%*s+%s+\n", LEFT - 1, "", line);

    char lab[1024];
    memset(lab, ' ', (size_t)w);
    lab[w] = '\0';
    int marks[5] = { 0, (size - 1) / 4, (size - 1) / 2, 3 * (size - 1) / 4, size - 1 };
    for (int m = 0; m < 5; m++) {
        char t[16];
        int len = snprintf(t, sizeof t, "%d", marks[m]);
        int c = col_of(marks[m], size, w);
        if (c + len > w) c = w - len;
        memcpy(lab + c, t, (size_t)len);
    }
    printf("%*s %s\n", LEFT - 1, "", lab);
}

static void draw_row(const disk_cfg_t *cfg, int prev, const step_t *s, int idx, int w, int color)
{
    char row[1024];
    memset(row, ' ', (size_t)w);
    row[w] = '\0';

    int a = col_of(prev, cfg->disk_size, w);
    int b = col_of(s->cyl, cfg->disk_size, w);
    char fill = (s->kind == STEP_JUMP) ? '.' : '-';
    char mark = 'o';
    const char *clr = "\033[32m";                      /* green: service */

    switch (s->kind) {
    case STEP_START: mark = 'S'; clr = "\033[36m"; a = b; break;
    case STEP_EDGE:  mark = '|'; clr = "\033[33m"; break;
    case STEP_JUMP:  mark = '*'; clr = "\033[35m"; break;
    default: break;
    }

    int lo = a < b ? a : b, hi = a < b ? b : a;
    for (int c = lo; c <= hi; c++) row[c] = fill;
    if (s->kind != STEP_START) row[a] = '+';
    row[b] = mark;

    const char *k = s->kind == STEP_SERVICE ? "  " : s->kind == STEP_EDGE ? "E " :
                    s->kind == STEP_JUMP ? "J " : "  ";
    printf("%3d %5d %s|", idx, s->cyl, k);
    if (color) {
        /* colour only the marker so the path stays readable */
        fwrite(row, 1, (size_t)b, stdout);
        printf("%s%c\033[0m", clr, row[b]);
        fputs(row + b + 1, stdout);
    } else {
        fputs(row, stdout);
    }
    puts("|");
}

void draw_result(const result_t *r, const disk_cfg_t *cfg, int anim_ms)
{
    int cols  = term_cols();
    int w     = cols - LEFT - 2;
    int color = use_color();
    if (w > 1000) w = 1000;
    if (w < 20)   w = 20;

    printf("\n%s  (head=%d, dir=%s, disk=0..%d)\n", algo_name(r->algo), cfg->head,
           cfg->dir == DIR_UP ? "up" : "down", cfg->disk_size - 1);
    draw_axis(cfg->disk_size, w);

    int prev = r->steps[0].cyl;
    for (int i = 0; i < r->nsteps; i++) {
        draw_row(cfg, prev, &r->steps[i], i, w, color);
        prev = r->steps[i].cyl;
        fflush(stdout);
        sleep_ms(anim_ms);
    }
    draw_axis(cfg->disk_size, w);
    printf("legend: S start  o service  E|edge  J* jump\n\n");

    printf("Service order : ");
    int first = 1;
    for (int i = 0; i < r->nsteps; i++)
        if (r->steps[i].kind == STEP_SERVICE) {
            printf(first ? "%d" : " -> %d", r->steps[i].cyl);
            first = 0;
        }
    printf("\nRequests      : %d\n", r->serviced);
    printf("Total seek    : %ld cylinders%s\n", r->total_seek,
           (r->algo == ALG_CSCAN || r->algo == ALG_CLOOK)
               ? (cfg->count_jump ? " (return jump counted)" : " (return jump not counted)") : "");
    printf("Average seek  : %.2f cylinders/request\n",
           r->serviced ? (double)r->total_seek / r->serviced : 0.0);
    printf("Est. time     : %.2f ms (model: %.2f ms settle + %.3f ms/cyl + %.2f ms rotation)\n",
           r->est_time_ms, cfg->settle_ms, cfg->seek_per_cyl_ms, rot_latency_ms(cfg));
}
