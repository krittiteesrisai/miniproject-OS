/*
 * sysinfo.c - show the real Linux I/O scheduler for each block device
 *
 * Reads /sys/block/<dev>/queue/{scheduler,rotational} and /sys/block/<dev>/size
 * with opendir()/readdir() and open()/read().
 */
#include "disksim.h"
#include <dirent.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

static int read_sysfs(const char *path, char *buf, size_t len)
{
    int fd = open(path, O_RDONLY);
    if (fd < 0) return -1;
    ssize_t k = read(fd, buf, len - 1);
    close(fd);
    if (k < 0) return -1;
    buf[k] = '\0';
    char *nl = strchr(buf, '\n');
    if (nl) *nl = '\0';
    return 0;
}

int show_sysinfo(void)
{
    DIR *d = opendir("/sys/block");
    if (!d) { perror("/sys/block"); return -1; }

    printf("%-12s %-6s %10s  %s\n", "Device", "Type", "Size", "I/O scheduler ([active])");
    struct dirent *e;
    int shown = 0;
    while ((e = readdir(d)) != NULL) {
        const char *n = e->d_name;
        if (n[0] == '.' || !strncmp(n, "loop", 4) || !strncmp(n, "ram", 3) || !strncmp(n, "zram", 4))
            continue;

        char path[512], rot[16] = "?", sched[256] = "?", size[32] = "0";
        snprintf(path, sizeof path, "/sys/block/%s/queue/rotational", n);
        read_sysfs(path, rot, sizeof rot);
        snprintf(path, sizeof path, "/sys/block/%s/queue/scheduler", n);
        read_sysfs(path, sched, sizeof sched);
        snprintf(path, sizeof path, "/sys/block/%s/size", n);
        read_sysfs(path, size, sizeof size);

        double gb = strtod(size, NULL) * 512.0 / 1e9;
        const char *type = rot[0] == '1' ? "HDD" : rot[0] == '0' ? "SSD" : "?";
        printf("%-12s %-6s %8.1f GB  %s\n", n, type, gb, sched);
        shown++;
    }
    closedir(d);
    if (!shown) printf("(no block devices found)\n");
    printf("\nHDD benefits from seek-aware scheduling; SSD has no seek, so Linux often uses 'none'.\n");
    return 0;
}
