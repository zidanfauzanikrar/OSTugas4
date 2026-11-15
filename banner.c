/*
 * banner.c - Identitas kelompok dan menu opsi (Tugas 4 - Filesystem dan I/O)
 */
#include "banner.h"

const char *banner_group_id(void)
{
    return GROUP_ID;
}

const char *banner_class_id(void)
{
    return KELAS_ID;
}

void banner_print_header(FILE *out)
{
    fprintf(out, "=== Kelompok %s | %s ===\n\n", GROUP_ID, BANNER_TITLE);
}

void banner_print_usage(FILE *out, const char *prog, const char *synopsis,
                        const char *const *options)
{
    fprintf(out, "Penggunaan: %s", prog ? prog : "program");
    if (synopsis && *synopsis)
        fprintf(out, " %s", synopsis);
    fputc('\n', out);

    if (options && options[0]) {
        fprintf(out, "Opsi:\n");
        for (size_t i = 0; options[i] != NULL; i++)
            fprintf(out, "  %s\n", options[i]);
    }
}