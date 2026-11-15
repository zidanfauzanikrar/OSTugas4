/*
 * iolib.h - Pustaka helper Modul Inti (Tugas 4 - Filesystem dan I/O)
 *
 * Dipakai bersama oleh workload.c dan seluruh program varian.
 * Semua berisi fungsi bebas (tanpa main). Fungsi yang mengembalikan int
 * memakai konvensi: 0 = sukses, -1 = gagal (errno terisi).
 */
#ifndef IOLIB_H
#define IOLIB_H

#include <stddef.h>
#include <stdint.h>
#include <stdio.h>

#define IO_LOG_DIR           "logs"
#define IO_NS_PER_SEC        1000000000ULL
#define IO_PATH_MAX          512
#define IO_CSV_DEFAULT_HEADER "run_id,scenario,op_index,latency_ns,bytes"

/* ------------------------------------------------------------------ */
/* 1. Timer (CLOCK_MONOTONIC, satuan nanodetik)                        */
/* ------------------------------------------------------------------ */

/* Waktu monotonik saat ini dalam nanodetik (0 bila gagal). */
uint64_t io_now_ns(void);

/* Timestamp lokal "YYYYMMDD-HHMMSS" (mis. 20261114-090958). */
void io_timestamp(char *buf, size_t len);

/* ------------------------------------------------------------------ */
/* 2. Metrik OS                                                        */
/* ------------------------------------------------------------------ */

/* Nilai Dirty: dan Writeback: dari /proc/meminfo, satuan kB. */
typedef struct {
    unsigned long dirty_kb;
    unsigned long writeback_kb;
} io_meminfo_t;

/* Delapan field pertama dari /sys/block/<dev>/stat. */
typedef struct {
    unsigned long long read_ios;
    unsigned long long read_merges;
    unsigned long long read_sectors;
    unsigned long long read_ticks;   /* ms */
    unsigned long long write_ios;
    unsigned long long write_merges;
    unsigned long long write_sectors;
    unsigned long long write_ticks;  /* ms */
} io_blkstat_t;

int io_read_meminfo(io_meminfo_t *out);

/*
 * Baca statistik perangkat blok. dev boleh "loop0", "sda", "sda2",
 * atau "/dev/loop0". Partisi (sda2) juga didukung.
 */
int io_read_blkstat(const char *dev, io_blkstat_t *out);

/* delta = after - before (semua field). */
void io_blkstat_delta(const io_blkstat_t *before, const io_blkstat_t *after,
                      io_blkstat_t *delta);

/* Baca /proc/sys/vm/<name> (mis. "dirty_ratio"). */
int io_read_vm_sysctl(const char *name, long *out);

/* sync() lalu tulis 3 ke /proc/sys/vm/drop_caches. Butuh root. */
int io_drop_caches(void);

/* ------------------------------------------------------------------ */
/* 3. Statistik latensi                                                */
/* ------------------------------------------------------------------ */

typedef struct {
    size_t   count;
    double   avg;
    uint64_t min;
    uint64_t max;
    uint64_t p50;
    uint64_t p95;
    uint64_t p99;
} io_stats_t;

/* Hitung rata-rata, min, max, p50, p95, p99 (nearest-rank). n harus > 0. */
int io_compute_stats(const uint64_t *samples, size_t n, io_stats_t *out);

/* Throughput dalam MB/s (1 MB = 1048576 byte). 0 bila elapsed_ns == 0. */
double io_throughput_mbps(uint64_t bytes, uint64_t elapsed_ns);

/* Format angka dengan pemisah ribuan spasi: 13057936 -> "13 057 936". */
void io_fmt_num(unsigned long long v, char *buf, size_t len);

/* Cetak satu baris: "Latensi (ns)    : avg ... | min ... | ... | p99 ...". */
void io_print_stats(FILE *out, const io_stats_t *s);

/* ------------------------------------------------------------------ */
/* 4. Penulis CSV ke folder logs/                                      */
/* ------------------------------------------------------------------ */

/*
 * Buka logs/XX-[program]-[skenario]-[timestamp].csv (folder logs/ dibuat
 * bila belum ada) lalu tulis baris header.
 *   header   : NULL = IO_CSV_DEFAULT_HEADER
 *   path_out : (opsional) menerima path berkas yang dibuat
 * Mengembalikan FILE* (NULL bila gagal).
 */
FILE *io_csv_open(const char *program, const char *scenario,
                  const char *header, char *path_out, size_t path_len);

/* Tulis satu baris format default: run_id,scenario,op_index,latency_ns,bytes */
int io_csv_write_row(FILE *f, int run_id, const char *scenario,
                     size_t op_index, uint64_t latency_ns, size_t bytes);

int io_csv_close(FILE *f);

/* ------------------------------------------------------------------ */
/* 5. Spesifikasi lingkungan: logs/XX-env-[timestamp].txt              */
/* ------------------------------------------------------------------ */

/*
 * Simpan kernel, RAM, media (device, fs, ukuran), vm.dirty_ratio,
 * vm.dirty_background_ratio, read-ahead, dan keluaran lsblk.
 *   media_dir : direktori media uji (NULL = ".")
 *   path_out  : (opsional) menerima path berkas yang dibuat
 * Panggil sekali di awal setiap sesi program.
 */
int io_save_env(const char *media_dir, char *path_out, size_t path_len);

#endif /* IOLIB_H */