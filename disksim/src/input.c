/*
 * input.c - load request lists (file via open/read, CLI list, random)
 */
#include "disksim.h"
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

/* Parse integers separated by anything; '#' starts a comment. */
static int parse_ints(const char *buf, size_t len, request_t *out, int max)
{
    int n = 0, in_num = 0, in_comment = 0;
    long v = 0;

    for (size_t i = 0; i <= len; i++) {
        char c = (i < len) ? buf[i] : '\n';
        if (in_comment) { if (c == '\n') in_comment = 0; continue; }
        if (c >= '0' && c <= '9') {
            v = v * 10 + (c - '0');
            in_num = 1;
            if (v > 1000000000L) { fprintf(stderr, "number too large\n"); return -1; }
            continue;
        }
        if (in_num) {
            if (n >= max) { fprintf(stderr, "too many requests (max %d)\n", max); return -1; }
            out[n] = (request_t){ .cyl = (int)v, .id = n, .src = 0, .arrival_ms = 0 };
            n++;
            v = 0; in_num = 0;
        }
        if (c == '#') in_comment = 1;
    }
    return n;
}

int load_file(const char *path, request_t *out, int max)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) { perror(path); return -1; }

    size_t cap = 4096, len = 0;
    char *buf = malloc(cap);
    if (!buf) { close(fd); return -1; }

    for (;;) {
        if (len == cap) {
            char *nb = realloc(buf, cap *= 2);
            if (!nb) { free(buf); close(fd); return -1; }
            buf = nb;
        }
        ssize_t k = read(fd, buf + len, cap - len);
        if (k < 0) {
            if (errno == EINTR) continue;
            perror("read"); free(buf); close(fd); return -1;
        }
        if (k == 0) break;
        len += (size_t)k;
    }
    close(fd);

    int n = parse_ints(buf, len, out, max);
    free(buf);
    return n;
}

int load_list(const char *s, request_t *out, int max)
{
    return parse_ints(s, strlen(s), out, max);
}

int gen_random(request_t *out, int n, int disk_size, unsigned seed)
{
    srand(seed);
    for (int i = 0; i < n; i++)
        out[i] = (request_t){ .cyl = rand() % disk_size, .id = i };
    return n;
}
