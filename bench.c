/*
 * renlib - a library of recursive routines callable from C. Provided to the
 * group. Do not modify.
 *
 * This file reads test files, calls the assembly routines below and C's own
 * qsort, verifies the results agree, times both, and prints the ratio. It
 * also drives the gcd and reentrancy demos from the manual. Your defense
 * will use this copy, so the messages it prints and the calls it makes are
 * the contract. Read this file before writing assembly.
 *
 * The four routines you implement are declared at the bottom.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "cdecl.h"

/* ------------------------------------------------------------------ */
/* The routines you implement.                                        */
/* ------------------------------------------------------------------ */

void PRE_CDECL qsort_asm(int *arr, int n) POST_CDECL;
int  PRE_CDECL bsearch_asm(int *arr, int n, int key) POST_CDECL;
int  PRE_CDECL gcd_asm(int a, int b) POST_CDECL;
int  PRE_CDECL sum_range_asm(int lo, int hi) POST_CDECL;

/*
 * gcd_asm records the recursion depth of its most recent call here. The
 * assembly sets it (it is a plain int in .bss, exported), and bench prints
 * it after the --gcd demo. This is the one global the activity permits.
 * It is diagnostic output, not part of any routine's result.
 */
extern int gcd_depth;

/*
 * sum_range_asm calls this once per invocation. bench guards it so that
 * only the first call actually nests a second sum_range_asm mid-flight,
 * which is how the reentrancy demo makes one call overlap another.
 */
void bench_callback(void);

/* ------------------------------------------------------------------ */
/* The rest is driver. Read it for the messages, not to change.       */
/* ------------------------------------------------------------------ */

/* Milliseconds of CPU time, portable across the two platforms. */
static double now_ms(void)
{
#ifdef _WIN32
    return (double)clock() * 1000.0 / CLOCKS_PER_SEC;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
#endif
}

static int cmp_int(const void *a, const void *b)
{
    return *(const int *)a - *(const int *)b;
}

static int read_ints(const char *path, int **out)
{
    FILE *f = fopen(path, "r");
    if (!f) {
        fprintf(stderr, "bench: cannot open %s\n", path);
        return -1;
    }

    int cap = 64, n = 0;
    int *arr = malloc(sizeof(int) * cap);
    if (!arr) {
        fclose(f);
        return -1;
    }

    int v;
    while (fscanf(f, "%d", &v) == 1) {
        if (n == cap) {
            cap *= 2;
            int *grown = realloc(arr, sizeof(int) * cap);
            if (!grown) {
                free(arr);
                fclose(f);
                return -1;
            }
            arr = grown;
        }
        arr[n++] = v;
    }
    fclose(f);
    *out = arr;
    return n;
}

static int is_ascending(const int *arr, int n)
{
    for (int i = 1; i < n; i++)
        if (arr[i] < arr[i - 1])
            return 0;
    return 1;
}

/* The callback guard: only the first call nests. */
static int callback_fired = 0;

void bench_callback(void)
{
    if (callback_fired)
        return;
    callback_fired = 1;

    /* A second sum_range_asm that begins before the first has finished. */
    int inner = sum_range_asm(1, 100);
    if (inner != 5050)
        printf("  REENTRANCY FAILURE: nested sum gave %d, wanted 5050\n", inner);
}

static int cmd_sort(const char *path)
{
    int *arr = NULL;
    int n = read_ints(path, &arr);
    if (n < 0)
        return 1;
    printf("Read %d integers.\n", n);

    int *mine = malloc(sizeof(int) * (n > 0 ? n : 1));
    int *theirs = malloc(sizeof(int) * (n > 0 ? n : 1));
    if (!mine || !theirs) {
        fprintf(stderr, "bench: out of memory\n");
        return 1;
    }
    memcpy(mine, arr, sizeof(int) * n);
    memcpy(theirs, arr, sizeof(int) * n);

    double t0 = now_ms();
    qsort_asm(mine, n);
    double t1 = now_ms();

    double t2 = now_ms();
    qsort(theirs, n, sizeof(int), cmp_int);
    double t3 = now_ms();

    printf("qsort_asm:   %s\n", is_ascending(mine, n) ? "sorted, verified ascending" : "NOT SORTED");
    printf("C qsort:     %s\n", is_ascending(theirs, n) ? "sorted, verified ascending" : "NOT SORTED");

    int match = (memcmp(mine, theirs, sizeof(int) * n) == 0);
    printf("Match:       %s\n", match ? "YES" : "NO");

    double mine_ms = t1 - t0;
    double theirs_ms = t3 - t2;
    double ratio = (theirs_ms > 0.0) ? mine_ms / theirs_ms : 0.0;
    printf("\nqsort_asm:   %.2f ms\n", mine_ms);
    printf("C qsort:     %.2f ms\n", theirs_ms);
    printf("Ratio:       %.2fx\n", ratio);

    free(arr);
    free(mine);
    free(theirs);
    return match ? 0 : 1;
}

static int cmd_gcd(int a, int b)
{
    int g = gcd_asm(a, b);
    printf("gcd_asm(%d, %d) = %d\n", a, b, g);
    printf("Recursion depth reached: %d\n", gcd_depth);
    return (g == 21) ? 0 : 1;
}

/*
 * Search checks: load a file, sort a copy with qsort_asm, then probe
 * bsearch_asm with a key that is present (first, middle, last), absent
 * (below everything, above everything, in a gap), and the boundary sizes.
 */
static int cmd_search(const char *path)
{
    int *arr = NULL;
    int n = read_ints(path, &arr);
    if (n < 0)
        return 1;

    int *sorted = malloc(sizeof(int) * (n > 0 ? n : 1));
    if (!sorted) {
        fprintf(stderr, "bench: out of memory\n");
        return 1;
    }
    memcpy(sorted, arr, sizeof(int) * n);
    qsort_asm(sorted, n);

    int failures = 0;

    /* Keys present: first, middle, last. */
    int present_keys[3];
    if (n > 0) {
        present_keys[0] = sorted[0];
        present_keys[1] = sorted[n / 2];
        present_keys[2] = sorted[n - 1];
    }
    for (int i = 0; i < 3 && n > 0; i++) {
        int key = present_keys[i];
        int idx = bsearch_asm(sorted, n, key);
        if (idx < 0 || idx >= n || sorted[idx] != key) {
            printf("  FAIL  bsearch(%d) -> %d, wanted a present index\n", key, idx);
            failures++;
        }
    }

    /* Keys absent: below everything, above everything, and in a gap. */
    if (n > 0) {
        int lo_key = sorted[0] - 1;
        int hi_key = sorted[n - 1] + 1;
        if (bsearch_asm(sorted, n, lo_key) != -1) {
            printf("  FAIL  bsearch(%d) should be -1\n", lo_key);
            failures++;
        }
        if (bsearch_asm(sorted, n, hi_key) != -1) {
            printf("  FAIL  bsearch(%d) should be -1\n", hi_key);
            failures++;
        }
        if (n >= 2) {
            int gap = sorted[0] + 1;
            for (int i = 1; i < n; i++) {
                if (sorted[i] > gap) {
                    if (bsearch_asm(sorted, n, gap) != -1) {
                        printf("  FAIL  bsearch(%d) should be -1\n", gap);
                        failures++;
                    }
                    break;
                }
            }
        }
    }

    /* Boundary sizes: empty and single element. */
    if (n == 0) {
        if (bsearch_asm(sorted, 0, 42) != -1) {
            printf("  FAIL  bsearch on empty array should be -1\n");
            failures++;
        }
    }

    printf("bsearch_asm: %s\n", failures == 0 ? "all probes correct" : "FAILURES");
    free(arr);
    free(sorted);
    return failures ? 1 : 0;
}

static int cmd_reentrancy(void)
{
    callback_fired = 0;
    int outer = sum_range_asm(1, 1000);
    printf("sum_range_asm(1, 1000) = %d (want 500500)\n", outer);
    printf("nested sum_range_asm(1, 100) = %d (want 5050)\n", 5050);
    int ok = (outer == 500500);
    printf("Reentrancy:  %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

static void usage(void)
{
    fprintf(stderr,
        "usage:\n"
        "  bench --sort FILE\n"
        "  bench --search FILE\n"
        "  bench --gcd A B\n"
        "  bench --reentrancy\n");
    exit(2);
}

int main(int argc, char **argv)
{
    if (argc < 2)
        usage();
    if (!strcmp(argv[1], "--sort") && argc == 3)
        return cmd_sort(argv[2]);
    if (!strcmp(argv[1], "--search") && argc == 3)
        return cmd_search(argv[2]);
    if (!strcmp(argv[1], "--gcd") && argc == 4)
        return cmd_gcd(atoi(argv[2]), atoi(argv[3]));
    if (!strcmp(argv[1], "--reentrancy"))
        return cmd_reentrancy();
    usage();
    return 2;
}
