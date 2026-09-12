/*
 * renlib - a library of recursive routines callable from C. Provided to the
 * group. Do not modify.
 *
 * This file reads test files. It calls the assembly routines declared just
 * below and C's own qsort, checks that the results agree, times both, and
 * prints the ratio. It also drives the gcd and reentrancy demos from the
 * manual, and a hostile caller that checks the calling convention. Your
 * defense will use this copy. The messages it prints and the calls it
 * makes are the contract. Read this file before writing assembly.
 */

#include <limits.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#ifdef _WIN32
#include <windows.h>
#endif

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
 * depth is the number of gcd calls on the stack at the deepest point, the
 * first call counted as 1. gcd_asm(1071, 462) reaches 4. The assembly
 * defines it as a plain int in .bss and exports it. bench prints it after
 * the --gcd demo. This is the one global the activity permits. It is
 * diagnostic output, separate from any routine's result.
 */
extern int gcd_depth;

/*
 * sum_range_asm calls this once per invocation. bench guards it so that
 * only the first call nests a second sum_range_asm mid-flight, which is
 * how the reentrancy demo makes one call overlap another.
 */
void bench_callback(void);

/* ------------------------------------------------------------------ */
/* The rest is driver. Read it for the messages, not to change.       */
/* ------------------------------------------------------------------ */

/* Milliseconds on a high-resolution clock, portable across the two platforms. */
static double now_ms(void)
{
#ifdef _WIN32
    LARGE_INTEGER freq, count;
    QueryPerformanceFrequency(&freq);
    QueryPerformanceCounter(&count);
    return (double)count.QuadPart * 1000.0 / (double)freq.QuadPart;
#else
    struct timespec ts;
    clock_gettime(CLOCK_MONOTONIC, &ts);
    return (double)ts.tv_sec * 1000.0 + (double)ts.tv_nsec / 1e6;
#endif
}

/* a - b overflows when the two are far apart, so compare instead. */
static int cmp_int(const void *a, const void *b)
{
    int x = *(const int *)a;
    int y = *(const int *)b;
    return (x > y) - (x < y);
}

/* Euclid in C, the oracle for --gcd. */
static int gcd_ref(int a, int b)
{
    while (b != 0) {
        int t = a % b;
        a = b;
        b = t;
    }
    return a;
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
static int nested_result = 0;

void bench_callback(void)
{
    if (callback_fired)
        return;
    callback_fired = 1;

    /* A second sum_range_asm that begins before the first has finished. */
    nested_result = sum_range_asm(1, 100);
}

/*
 * Sorting a thousand ints once takes tens of microseconds, below what the
 * clock shows reliably. Each sort therefore runs REPS times from a fresh
 * copy inside one timed span. The time printed is the mean per sort. The
 * copy costs about a microsecond and is charged to both sides.
 */
enum { REPS = 200 };

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

    double t0 = now_ms();
    for (int r = 0; r < REPS; r++) {
        memcpy(mine, arr, sizeof(int) * n);
        qsort_asm(mine, n);
    }
    double t1 = now_ms();

    double t2 = now_ms();
    for (int r = 0; r < REPS; r++) {
        memcpy(theirs, arr, sizeof(int) * n);
        qsort(theirs, n, sizeof(int), cmp_int);
    }
    double t3 = now_ms();

    printf("qsort_asm:   %s\n", is_ascending(mine, n) ? "sorted, verified ascending" : "NOT SORTED");
    printf("C qsort:     %s\n", is_ascending(theirs, n) ? "sorted, verified ascending" : "NOT SORTED");

    int match = (memcmp(mine, theirs, sizeof(int) * n) == 0);
    printf("Match:       %s\n", match ? "YES" : "NO");

    double mine_ms = (t1 - t0) / REPS;
    double theirs_ms = (t3 - t2) / REPS;
    double ratio = (theirs_ms > 0.0) ? mine_ms / theirs_ms : 0.0;
    printf("\nqsort_asm:   %.3f ms per sort (mean of %d runs)\n", mine_ms, REPS);
    printf("C qsort:     %.3f ms per sort (mean of %d runs)\n", theirs_ms, REPS);
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
    return (g == gcd_ref(a, b)) ? 0 : 1;
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

    /*
     * Keys absent: below everything, above everything, and in a gap.
     * INT_MIN - 1 and INT_MAX + 1 wrap around to present values, so a
     * file that reaches an extreme skips that probe.
     */
    if (n > 0) {
        if (sorted[0] != INT_MIN) {
            int lo_key = sorted[0] - 1;
            if (bsearch_asm(sorted, n, lo_key) != -1) {
                printf("  FAIL  bsearch(%d) should be -1\n", lo_key);
                failures++;
            }
        }
        if (sorted[n - 1] != INT_MAX) {
            int hi_key = sorted[n - 1] + 1;
            if (bsearch_asm(sorted, n, hi_key) != -1) {
                printf("  FAIL  bsearch(%d) should be -1\n", hi_key);
                failures++;
            }
        }
        if (n >= 2 && sorted[0] != INT_MAX) {
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
    nested_result = 0;
    int outer = sum_range_asm(1, 1000);
    printf("sum_range_asm(1, 1000) = %d (want 500500)\n", outer);
    if (callback_fired)
        printf("nested sum_range_asm(1, 100) = %d (want 5050)\n", nested_result);
    else
        printf("nested sum_range_asm(1, 100) never ran: sum_range_asm did not call bench_callback\n");
    int ok = callback_fired && outer == 500500 && nested_result == 5050;
    printf("Reentrancy:  %s\n", ok ? "PASS" : "FAIL");
    return ok ? 0 : 1;
}

/*
 * The hostile caller. It loads a sentinel into each callee-saved register,
 * calls one routine through a pointer, and reads the registers back. cdecl
 * says ebx, esi, and edi must come back unchanged. Three arguments are
 * pushed for every routine. A cdecl callee ignores the extras. The caller
 * removes them. The stack pointer is checked the same way. A
 * routine that pops one too many, or takes its arguments off the stack
 * itself, moves esp. A routine that clobbers ebp crashes this probe, which
 * also counts as a failure.
 *
 * The offsets in the assembly below are the struct's: fn at 0, the
 * arguments at 4, 8, 12, the registers at 16, 20, 24, the delta at 28.
 */
struct hostile {
    void *fn;
    int a0, a1, a2;
    unsigned ebx, esi, edi;
    int esp_delta;
};

static void hostile_call(struct hostile *h)
{
    __asm__ volatile(
        "push %%ebx\n\t"
        "push %%esi\n\t"
        "push %%edi\n\t"
        "push %%ebp\n\t"
        "mov  %%esp, %%ebp\n\t"
        "push %%eax\n\t"                /* [ebp-4] = h, survives the call */
        "mov  %%eax, %%ecx\n\t"
        "mov  $0xB0B0B0B0, %%ebx\n\t"
        "mov  $0x51515151, %%esi\n\t"
        "mov  $0xD1D1D1D1, %%edi\n\t"
        "push 12(%%ecx)\n\t"
        "push 8(%%ecx)\n\t"
        "push 4(%%ecx)\n\t"
        "call *(%%ecx)\n\t"
        "add  $12, %%esp\n\t"
        "mov  -4(%%ebp), %%ecx\n\t"
        "mov  %%ebx, 16(%%ecx)\n\t"
        "mov  %%esi, 20(%%ecx)\n\t"
        "mov  %%edi, 24(%%ecx)\n\t"
        "mov  %%ebp, %%eax\n\t"
        "sub  %%esp, %%eax\n\t"
        "sub  $4, %%eax\n\t"            /* 0 when esp is back where it was */
        "mov  %%eax, 28(%%ecx)\n\t"
        "mov  %%ebp, %%esp\n\t"
        "pop  %%ebp\n\t"
        "pop  %%edi\n\t"
        "pop  %%esi\n\t"
        "pop  %%ebx\n\t"
        : "+a"(h)
        :
        : "ecx", "edx", "memory", "cc");
}

static int hostile_probe(const char *name, void *fn, int a0, int a1, int a2)
{
    struct hostile h = { fn, a0, a1, a2, 0, 0, 0, 0 };
    hostile_call(&h);

    int bad = 0;
    printf("hostile %-15s", name);
    if (h.ebx != 0xB0B0B0B0u) { printf(" CLOBBERED ebx"); bad++; }
    if (h.esi != 0x51515151u) { printf(" CLOBBERED esi"); bad++; }
    if (h.edi != 0xD1D1D1D1u) { printf(" CLOBBERED edi"); bad++; }
    if (h.esp_delta != 0)     { printf(" stack off by %d", h.esp_delta); bad++; }
    if (bad == 0)
        printf(" ebx esi edi preserved, stack balanced");
    printf("\n");
    return bad ? 1 : 0;
}

static int cmd_hostile(void)
{
    int small[5] = { 3, -1, 4, 1, -5 };
    int failures = 0;

    /* Keep sum_range_asm's callback from nesting a second call here. */
    callback_fired = 1;

    failures += hostile_probe("qsort_asm:", (void *)qsort_asm, (int)(intptr_t)small, 5, 0);
    failures += hostile_probe("bsearch_asm:", (void *)bsearch_asm, (int)(intptr_t)small, 5, small[2]);
    failures += hostile_probe("gcd_asm:", (void *)gcd_asm, 1071, 462, 0);
    failures += hostile_probe("sum_range_asm:", (void *)sum_range_asm, 1, 10, 0);

    printf("Hostile caller: %s\n", failures == 0 ? "PASS" : "FAIL");
    return failures ? 1 : 0;
}

static void usage(void)
{
    fprintf(stderr,
        "usage:\n"
        "  bench --sort FILE\n"
        "  bench --search FILE\n"
        "  bench --gcd A B\n"
        "  bench --reentrancy\n"
        "  bench --hostile\n");
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
    if (!strcmp(argv[1], "--hostile"))
        return cmd_hostile();
    usage();
    return 2;
}
