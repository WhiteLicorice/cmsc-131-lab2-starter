;
; util.asm - gcd_asm and sum_range_asm, both recursive, callable from C.
;
; This is your starting point: it assembles and links as-is, so the build
; works before you write any code. Right now gcd_asm returns 0 and
; sum_range_asm returns 0, so the benchmark's --gcd and --reentrancy demos
; fail. Your job is to replace both.
;
; The contracts, from bench.c:
;
;       int b                       [ebp+12]
;       int a                       [ebp+8]
;       int gcd_asm(int a, int b)   returns the greatest common divisor
;
;       int hi                      [ebp+12]
;       int lo                      [ebp+8]
;       int sum_range_asm(lo, hi)   returns the sum of lo..hi inclusive
;
; Both are C-facing and must obey cdecl: prologue, args at [ebp+8]+, return
; in eax, preserve ebx/esi/edi/ebp, clean arguments after every call.
;
; gcd_asm also sets the global `gcd_depth` to the recursion depth of its
; most recent call. bench.c declares it extern and prints it after --gcd:
;
;       int gcd_depth               ; in .bss, exported, one int
;
; The manual's requirements:
;
;   * Both genuinely recursive (Euclid's algorithm for gcd).
;   * sum_range_asm must be reentrant. It has to stay correct when a second
;     call begins before the first has finished. That means no .data or
;     .bss storage that changes during execution. Everything lives on the
;     stack and in registers. The static gcd_depth global is gcd's, not
;     sum_range's, and it is diagnostic rather than part of a result.
;   * sum_range_asm calls the C function bench_callback(void) once per
;     invocation, which is how bench makes one call overlap another. Declare
;     it extern and call it with cdecl. It is not in the manual's
;     pseudocode, but it is what the reentrancy demo runs on.
;   * No calls into the C standard library.
;
; Euclid:
;
;   gcd(a, b):
;       if b == 0: return a
;       return gcd(b, a % b)
;
; sum_range, the reentrant shape:
;
;   sum_range(lo, hi):
;       if lo > hi: return 0
;       return lo + sum_range(lo + 1, hi)
;

; Windows C decorates the names it exports with a leading underscore and
; Linux C does not, so the same source would otherwise need two spellings of
; every entry point. -d ELF_TYPE, which the shared Makefile fragment passes
; on Linux, selects the respelling here. It's the same trick asm_io.inc
; uses for _asm_main in the bootcamp blocks. Leave this block alone.
%ifdef ELF_TYPE
  %define _gcd_asm gcd_asm
  %define _sum_range_asm sum_range_asm
  %define _gcd_depth gcd_depth
  %define _bench_callback bench_callback
  section .note.GNU-stack noalloc noexec nowrite progbits
%endif

segment .bss
        global  _gcd_depth
_gcd_depth      resd    1

segment .text
        global  _gcd_asm
        extern  _bench_callback
_gcd_asm:
        enter   0,0
        pusha

        ;
        ; TODO: gcd. Track the depth: increment gcd_depth at entry and
        ; decrement at exit, or carry it in a register down the recursion.
        ; Either way bench.c reads it after the call returns.
        ;

        popa
        mov     eax, 0
        leave
        ret

        global  _sum_range_asm
_sum_range_asm:
        enter   0,0
        pusha

        ;
        ; TODO: sum_range. Call bench_callback once per invocation, then
        ; recurse on (lo + 1, hi) and add lo. The base case lo > hi is 0.
        ; Keep every intermediate on the stack. A global counter or
        ; accumulator here is what breaks reentrancy.
        ;

        popa
        mov     eax, 0
        leave
        ret
