/*
 * banner.h - Identitas kelompok dan menu opsi (Tugas 4 - Filesystem dan I/O)
 *
 * Nomor kelompok (XX) dan kelas diatur lewat Makefile:
 *     make KELOMPOK=01 KELAS=A
 * Nilai default ada di bawah ini bila program dikompilasi tanpa Makefile.
 */
#ifndef BANNER_H
#define BANNER_H

#include <stdio.h>

#ifndef GROUP_ID
#define GROUP_ID "08"
#endif

#ifndef KELAS_ID
#define KELAS_ID "A"
#endif

#define BANNER_TITLE "Tugas 4 - Filesystem dan I/O"

/* Nomor kelompok (string "XX"), dipakai juga untuk penamaan berkas log. */
const char *banner_group_id(void);

/* Kode kelas, dipakai untuk penamaan zip: Tugas4_<KELAS><KELOMPOK>.zip */
const char *banner_class_id(void);

/* Cetak: "=== Kelompok XX | Tugas 4 - Filesystem dan I/O ===" + baris kosong. */
void banner_print_header(FILE *out);

/*
 * Cetak menu opsi program.
 *   prog     : nama program (mis. "workload")
 *   synopsis : ringkasan argumen (mis. "--dir DIR --size MB [opsi]"), boleh NULL
 *   options  : array string berakhir NULL, satu string per opsi,
 *              mis. { "--dir DIR    lokasi berkas uji", NULL }
 */
void banner_print_usage(FILE *out, const char *prog, const char *synopsis,
                        const char *const *options);

#endif /* BANNER_H */