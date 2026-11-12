/* DLSS runtime discovery with a harmless test shared library. SPDX-License-Identifier: MIT */
#ifdef _MSC_VER
#define _CRT_SECURE_NO_WARNINGS /* MSVC /W4 /WX: fopen is fine here (C4996) */
#endif
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "uscale_test.h"

uint32_t uscale_dlss_runtime_in_dir(const char *directory);
uint32_t uscale_dlss_runtime_available(uint32_t vendor_id, uint32_t is_moltenvk,
                                       const char *directory);
#if !defined(_WIN32)
void uscale_dlss_path_list(char *list, size_t cap, const char *exe_dir, const char *ld_path);
uint32_t uscale_dlss_scan_path_list(const char *list);
#endif

static int copy_file(const char *source, const char *dest)
{
    FILE *in = fopen(source, "rb");
    FILE *out;
    int c, ok = 1;
    if (in == NULL) return 0;
    out = fopen(dest, "wb");
    if (out == NULL) { fclose(in); return 0; }
    while ((c = fgetc(in)) != EOF) if (fputc(c, out) == EOF) { ok = 0; break; }
    if (ferror(in)) ok = 0;
    fclose(in);
    if (fclose(out) != 0) ok = 0;
    return ok;
}

int main(int argc, char **argv)
{
    char path[4096];
    int n;
    CHECK_INT(argc, 3);
    if (argc == 3) {
#if defined(_WIN32)
        n = snprintf(path, sizeof(path), "%s\\nvngx_dlss.dll", argv[2]);
#else
        n = snprintf(path, sizeof(path), "%s/libnvidia-ngx-dlss.so.310.9.1", argv[2]);
#endif
        if (n > 0 && (size_t)n < sizeof(path)) remove(path);
        CHECK_INT(uscale_dlss_runtime_available(0x10DEu, 0, argv[2]), 0);
        CHECK_INT(uscale_dlss_runtime_available(0x10DEu, 1, argv[2]), 0);
        CHECK_INT(uscale_dlss_runtime_available(0x1002u, 0, argv[2]), 0);
        CHECK(n > 0 && (size_t)n < sizeof(path));
        if (n > 0 && (size_t)n < sizeof(path)) CHECK(copy_file(argv[1], path));
        CHECK_INT(uscale_dlss_runtime_available(0x10DEu, 0, argv[2]), 1);
        CHECK_INT(uscale_dlss_runtime_available(0x10DEu, 1, argv[2]), 0);
#if !defined(_WIN32)
        {   /* The search list: exe folder, LD_LIBRARY_PATH entries, standard folders. */
            char list[8192];
            uscale_dlss_path_list(list, sizeof(list), "/exe", "/a::");
            CHECK(strncmp(list, "/exe:/a:", 8) == 0 && strstr(list, ":/usr/lib:") != NULL);
            uscale_dlss_path_list(list, sizeof(list), NULL, NULL);
            CHECK(strncmp(list, "/usr/lib:", 9) == 0);
            uscale_dlss_path_list(list, 12, "/exe", "/too/long/for/it");
            CHECK(strcmp(list, "/exe") == 0);
            /* LD_LIBRARY_PATH hit (second entry, empty entry ignored), exe-folder hit, miss. */
            snprintf(list, sizeof(list), "/no/such::%s", argv[2]);
            CHECK_INT(uscale_dlss_scan_path_list(list), 1);
            snprintf(list, sizeof(list), "%s:/no/such", argv[2]);
            CHECK_INT(uscale_dlss_scan_path_list(list), 1);
            CHECK_INT(uscale_dlss_scan_path_list("/no/such::/also/missing:"), 0);
            CHECK_INT(uscale_dlss_scan_path_list(""), 0);
        }
#endif
        if (n > 0 && (size_t)n < sizeof(path)) remove(path);
        CHECK_INT(uscale_dlss_runtime_available(0x10DEu, 0, argv[2]), 0);
    }
    USCALE_TEST_END;
}
