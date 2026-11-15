/*
 * iolib.c - Pustaka helper Modul Inti (Tugas 4 - Filesystem dan I/O)
 */
#define _GNU_SOURCE
#include "iolib.h"
#include "banner.h"

#include <ctype.h>
#include <errno.h>
#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/sysmacros.h>
#include <sys/types.h>
#include <sys/utsname.h>

/* ------------------------------------------------------------------ */
/* Timer                                                               */
/* ------------------------------------------------------------------ */

uint64_t io_now_ns(void)
{
    struct timespec ts;

    if (clock_gettime(CLOCK_MONOTONIC, &ts) != 0)
        return 0;
    return (uint64_t)ts.tv_sec * IO_NS_PER_SEC + (uint64_t)ts.tv_nsec;
}

void io_timestamp(char *buf, size_t len)
{
    time_t t = time(NULL);
    struct tm tm;

    if (!buf || len == 0)
        return;
    if (localtime_r(&t, &tm) == NULL ||
        strftime(buf, len, "%Y%m%d-%H%M%S", &tm) == 0)
        snprintf(buf, len, "00000000-000000");
}

/* ------------------------------------------------------------------ */
/* Metrik OS                                                           */
/* ------------------------------------------------------------------ */

static int meminfo_field(const char *name, unsigned long *out)
{
    FILE *f = fopen("/proc/meminfo", "r");
    char line[256];
    size_t nlen = strlen(name);
    int rc = -1;

    if (!f)
        return -1;
    while (fgets(line, sizeof line, f)) {
        if (strncmp(line, name, nlen) == 0 && line[nlen] == ':') {
            unsigned long v;

            if (sscanf(line + nlen + 1, " %lu", &v) == 1) {
                *out = v;
                rc = 0;
            }
            break;
        }
    }
    fclose(f);
    if (rc != 0)
        errno = ENODATA;
    return rc;
}

int io_read_meminfo(io_meminfo_t *out)
{
    if (!out) {
        errno = EINVAL;
        return -1;
    }
    if (meminfo_field("Dirty", &out->dirty_kb) != 0)
        return -1;
    if (meminfo_field("Writeback", &out->writeback_kb) != 0)
        return -1;
    return 0;
}

int io_read_blkstat(const char *dev, io_blkstat_t *out)
{
    char path[IO_PATH_MAX];
    unsigned long long v[8];
    FILE *f;
    int n;

    if (!dev || !out) {
        errno = EINVAL;
        return -1;
    }
    if (strncmp(dev, "/dev/", 5) == 0)
        dev += 5;
    if (*dev == '\0' || strchr(dev, '/') != NULL || strlen(dev) > 64) {
        errno = EINVAL;
        return -1;
    }

    snprintf(path, sizeof path, "/sys/block/%s/stat", dev);
    f = fopen(path, "r");
    if (!f) {
        /* Partisi (sda2) tidak ada di /sys/block, tetapi ada di class/block. */
        snprintf(path, sizeof path, "/sys/class/block/%s/stat", dev);
        f = fopen(path, "r");
    }
    if (!f)
        return -1;

    n = fscanf(f, "%llu %llu %llu %llu %llu %llu %llu %llu",
               &v[0], &v[1], &v[2], &v[3], &v[4], &v[5], &v[6], &v[7]);
    fclose(f);
    if (n != 8) {
        errno = EIO;
        return -1;
    }

    out->read_ios      = v[0];
    out->read_merges   = v[1];
    out->read_sectors  = v[2];
    out->read_ticks    = v[3];
    out->write_ios     = v[4];
    out->write_merges  = v[5];
    out->write_sectors = v[6];
    out->write_ticks   = v[7];
    return 0;
}

void io_blkstat_delta(const io_blkstat_t *before, const io_blkstat_t *after,
                      io_blkstat_t *delta)
{
    delta->read_ios      = after->read_ios      - before->read_ios;
    delta->read_merges   = after->read_merges   - before->read_merges;
    delta->read_sectors  = after->read_sectors  - before->read_sectors;
    delta->read_ticks    = after->read_ticks    - before->read_ticks;
    delta->write_ios     = after->write_ios     - before->write_ios;
    delta->write_merges  = after->write_merges  - before->write_merges;
    delta->write_sectors = after->write_sectors - before->write_sectors;
    delta->write_ticks   = after->write_ticks   - before->write_ticks;
}

int io_read_vm_sysctl(const char *name, long *out)
{
    char path[IO_PATH_MAX];
    char buf[64];
    char *end;
    FILE *f;

    if (!name || !out || strchr(name, '/') != NULL) {
        errno = EINVAL;
        return -1;
    }
    snprintf(path, sizeof path, "/proc/sys/vm/%s", name);
    f = fopen(path, "r");
    if (!f)
        return -1;
    if (!fgets(buf, sizeof buf, f)) {
        fclose(f);
        errno = EIO;
        return -1;
    }
    fclose(f);

    errno = 0;
    *out = strtol(buf, &end, 10);
    if (end == buf || errno != 0) {
        errno = EINVAL;
        return -1;
    }
    return 0;
}

int io_drop_caches(void)
{
    FILE *f;

    sync();
    f = fopen("/proc/sys/vm/drop_caches", "w");
    if (!f)
        return -1;
    if (fputs("3\n", f) == EOF) {
        int saved = errno;

        fclose(f);
        errno = saved;
        return -1;
    }
    return fclose(f) == 0 ? 0 : -1;
}

/* ------------------------------------------------------------------ */
/* Statistik                                                           */
/* ------------------------------------------------------------------ */

static int cmp_u64(const void *a, const void *b)
{
    uint64_t x = *(const uint64_t *)a;
    uint64_t y = *(const uint64_t *)b;

    return (x > y) - (x < y);
}

/* Nearest-rank: indeks ke ceil(n * p / 100), basis 1. */
static uint64_t percentile(const uint64_t *sorted, size_t n, unsigned p)
{
    size_t rank = (n * p + 99) / 100;

    if (rank == 0)
        rank = 1;
    if (rank > n)
        rank = n;
    return sorted[rank - 1];
}

int io_compute_stats(const uint64_t *samples, size_t n, io_stats_t *out)
{
    uint64_t *sorted;
    uint64_t sum = 0;

    if (!samples || !out || n == 0) {
        errno = EINVAL;
        return -1;
    }
    sorted = malloc(n * sizeof *sorted);
    if (!sorted)
        return -1;
    memcpy(sorted, samples, n * sizeof *sorted);
    qsort(sorted, n, sizeof *sorted, cmp_u64);

    for (size_t i = 0; i < n; i++)
        sum += sorted[i];

    out->count = n;
    out->avg   = (double)sum / (double)n;
    out->min   = sorted[0];
    out->max   = sorted[n - 1];
    out->p50   = percentile(sorted, n, 50);
    out->p95   = percentile(sorted, n, 95);
    out->p99   = percentile(sorted, n, 99);

    free(sorted);
    return 0;
}

double io_throughput_mbps(uint64_t bytes, uint64_t elapsed_ns)
{
    if (elapsed_ns == 0)
        return 0.0;
    return ((double)bytes / 1048576.0) /
           ((double)elapsed_ns / (double)IO_NS_PER_SEC);
}

void io_fmt_num(unsigned long long v, char *buf, size_t len)
{
    char tmp[32];
    int n;
    size_t o = 0;

    if (!buf || len == 0)
        return;
    n = snprintf(tmp, sizeof tmp, "%llu", v);
    for (int i = 0; i < n && o + 2 < len; i++) {
        if (i > 0 && (n - i) % 3 == 0)
            buf[o++] = ' ';
        buf[o++] = tmp[i];
    }
    buf[o] = '\0';
}

void io_print_stats(FILE *out, const io_stats_t *s)
{
    char avg[32], mn[32], mx[32], p50[32], p95[32], p99[32];

    io_fmt_num((unsigned long long)(s->avg + 0.5), avg, sizeof avg);
    io_fmt_num(s->min, mn, sizeof mn);
    io_fmt_num(s->max, mx, sizeof mx);
    io_fmt_num(s->p50, p50, sizeof p50);
    io_fmt_num(s->p95, p95, sizeof p95);
    io_fmt_num(s->p99, p99, sizeof p99);

    fprintf(out,
            "Latensi (ns)    : avg %s | min %s | max %s | p50 %s | p95 %s | p99 %s\n",
            avg, mn, mx, p50, p95, p99);
}

/* ------------------------------------------------------------------ */
/* CSV                                                                 */
/* ------------------------------------------------------------------ */

static int ensure_log_dir(void)
{
    if (mkdir(IO_LOG_DIR, 0755) == 0 || errno == EEXIST)
        return 0;
    return -1;
}

static void sanitize(const char *in, char *out, size_t len)
{
    size_t i = 0;

    for (; in && *in && i + 1 < len; in++, i++) {
        unsigned char c = (unsigned char)*in;

        out[i] = (isalnum(c) || c == '-' || c == '_' || c == '.') ? (char)c : '_';
    }
    out[i] = '\0';
}

FILE *io_csv_open(const char *program, const char *scenario,
                  const char *header, char *path_out, size_t path_len)
{
    char prog[64], scen[64], ts[32], path[IO_PATH_MAX];
    int n;
    FILE *f;

    if (ensure_log_dir() != 0)
        return NULL;

    sanitize(program ? program : "prog", prog, sizeof prog);
    sanitize(scenario ? scenario : "run", scen, sizeof scen);
    io_timestamp(ts, sizeof ts);

    n = snprintf(path, sizeof path, "%s/%s-%s-%s-%s.csv",
                 IO_LOG_DIR, banner_group_id(), prog, scen, ts);
    if (n < 0 || (size_t)n >= sizeof path) {
        errno = ENAMETOOLONG;
        return NULL;
    }

    f = fopen(path, "w");
    if (!f)
        return NULL;
    fprintf(f, "%s\n", header ? header : IO_CSV_DEFAULT_HEADER);

    if (path_out && path_len > 0)
        snprintf(path_out, path_len, "%s", path);
    return f;
}

int io_csv_write_row(FILE *f, int run_id, const char *scenario,
                     size_t op_index, uint64_t latency_ns, size_t bytes)
{
    int n = fprintf(f, "%d,%s,%zu,%llu,%zu\n", run_id,
                    scenario ? scenario : "",
                    op_index, (unsigned long long)latency_ns, bytes);

    return n < 0 ? -1 : 0;
}

int io_csv_close(FILE *f)
{
    return f ? fclose(f) : 0;
}

/* ------------------------------------------------------------------ */
/* Spesifikasi lingkungan                                              */
/* ------------------------------------------------------------------ */

typedef struct {
    char source[256];  /* mis. /dev/sda2 */
    char fstype[64];   /* mis. ext4 */
    char dev[64];      /* nama di sysfs, mis. sda2 / loop0 */
} media_info_t;

static int read_first_line(const char *path, char *buf, size_t len)
{
    FILE *f = fopen(path, "r");

    if (!f)
        return -1;
    if (!fgets(buf, (int)len, f)) {
        fclose(f);
        return -1;
    }
    fclose(f);
    buf[strcspn(buf, "\n")] = '\0';
    return 0;
}

static void find_media(const char *dir, media_info_t *m)
{
    struct stat st;
    unsigned mj, mn;
    char link[IO_PATH_MAX], target[IO_PATH_MAX];
    ssize_t n;

    snprintf(m->source, sizeof m->source, "-");
    snprintf(m->fstype, sizeof m->fstype, "-");
    snprintf(m->dev, sizeof m->dev, "-");

    if (stat(dir, &st) != 0)
        return;
    mj = major(st.st_dev);
    mn = minor(st.st_dev);

    /* Cari fstype dan sumber di mountinfo lewat pasangan major:minor. */
    FILE *f = fopen("/proc/self/mountinfo", "r");

    if (f) {
        char line[1024];

        while (fgets(line, sizeof line, f)) {
            unsigned lmj, lmn;
            char mp[256];
            char *sep;

            if (sscanf(line, "%*d %*d %u:%u %*s %255s", &lmj, &lmn, mp) != 3)
                continue;
            if (lmj != mj || lmn != mn)
                continue;
            sep = strstr(line, " - ");
            if (!sep)
                continue;
            if (sscanf(sep + 3, "%63s %255s", m->fstype, m->source) < 2)
                continue;
            break;
        }
        fclose(f);
    }

    /* Nama perangkat di sysfs: ujung symlink /sys/dev/block/MAJ:MIN. */
    snprintf(link, sizeof link, "/sys/dev/block/%u:%u", mj, mn);
    n = readlink(link, target, sizeof target - 1);
    if (n > 0) {
        const char *base;

        target[n] = '\0';
        base = strrchr(target, '/');
        base = base ? base + 1 : target;
        snprintf(m->dev, sizeof m->dev, "%.63s", base);
    } else if (strncmp(m->source, "/dev/", 5) == 0) {
        snprintf(m->dev, sizeof m->dev, "%.63s", m->source + 5);
    }
}

static int read_readahead(const char *dev, char *buf, size_t len)
{
    char path[IO_PATH_MAX];

    snprintf(path, sizeof path, "/sys/class/block/%s/queue/read_ahead_kb", dev);
    if (read_first_line(path, buf, len) == 0)
        return 0;
    /* Partisi: parameter antrean ada di perangkat induknya. */
    snprintf(path, sizeof path, "/sys/class/block/%s/../queue/read_ahead_kb", dev);
    return read_first_line(path, buf, len);
}

int io_save_env(const char *media_dir, char *path_out, size_t path_len)
{
    char ts[32], path[IO_PATH_MAX], buf[128];
    struct utsname un;
    struct statvfs sv;
    media_info_t mi;
    unsigned long mem_kb = 0;
    double size_g = 0.0;
    long v;
    FILE *f;

    if (!media_dir || !*media_dir)
        media_dir = ".";
    if (ensure_log_dir() != 0)
        return -1;

    io_timestamp(ts, sizeof ts);
    snprintf(path, sizeof path, "%s/%s-env-%s.txt", IO_LOG_DIR,
             banner_group_id(), ts);
    f = fopen(path, "w");
    if (!f)
        return -1;

    find_media(media_dir, &mi);
    if (statvfs(media_dir, &sv) == 0)
        size_g = (double)sv.f_blocks * (double)sv.f_frsize / 1073741824.0;

    fprintf(f, "Kelompok : %s\n", banner_group_id());
    fprintf(f, "Waktu : %s\n", ts);

    if (uname(&un) == 0)
        fprintf(f, "Kernel : %s\n", un.release);
    else
        fprintf(f, "Kernel : -\n");

    if (meminfo_field("MemTotal", &mem_kb) == 0)
        fprintf(f, "RAM : %.1f GB\n", (double)mem_kb / 1048576.0);
    else
        fprintf(f, "RAM : -\n");

    fprintf(f, "Media : %s (%s %s %.1fG) [dev=%s]\n",
            media_dir, mi.source, mi.fstype, size_g, mi.dev);

    if (io_read_vm_sysctl("dirty_ratio", &v) == 0)
        fprintf(f, "vm.dirty_ratio : %ld\n", v);
    else
        fprintf(f, "vm.dirty_ratio : -\n");
    if (io_read_vm_sysctl("dirty_background_ratio", &v) == 0)
        fprintf(f, "vm.dirty_background_ratio : %ld\n", v);
    else
        fprintf(f, "vm.dirty_background_ratio : -\n");

    if (strcmp(mi.dev, "-") != 0 && read_readahead(mi.dev, buf, sizeof buf) == 0)
        fprintf(f, "Read-ahead : %s kB\n", buf);
    else
        fprintf(f, "Read-ahead : -\n");

    fprintf(f, "lsblk :\n");
    fflush(f);
    {
        FILE *p = popen("lsblk 2>/dev/null", "r");
        char line[512];
        int lines = 0;

        if (p) {
            while (fgets(line, sizeof line, p)) {
                fprintf(f, "  %s", line);
                lines++;
            }
            pclose(p);
        }
        if (lines == 0)
            fprintf(f, "  (lsblk tidak tersedia)\n");
    }

    if (fclose(f) != 0)
        return -1;
    if (path_out && path_len > 0)
        snprintf(path_out, path_len, "%s", path);
    return 0;
}