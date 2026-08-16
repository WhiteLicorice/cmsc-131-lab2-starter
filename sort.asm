;
; sort.asm - qsort_asm, a recursive quicksort callable from C.
;
; This is your starting point: it assembles and links as-is, so the build
; works before you write any code. Right now it does nothing, which makes
; the benchmark print "NOT SORTED" and "Match: NO". Your job is to replace
; that with the sort described below.
;
; The contract, from bench.c:
;
;       int n                     [ebp+12]
;       int *arr                  [ebp+8]
;
; Sort n 32-bit signed integers in place, ascending. This is the C-facing
; routine. It must obey cdecl (prologue, args at [ebp+8]+, return in eax,
; preserve ebx/esi/edi/ebp, clean arguments after every call it makes). The
; manual lets you use any convention you document for the internal recursive
; routine, but the outer one is fixed.
;
; Requirements from the manual:
;
;   * Genuinely recursive. An iterative sort with an explicit stack array
;     does not satisfy the activity.
;   * Lomuto partition with the last element as pivot (the manual's scheme).
;   * No calls into the C standard library.
;   * The recursion must not drift the stack. Clean up arguments after each
;     call, or a deep recursion runs into memory it does not own.
;
; The manual's pseudocode:
;
;   qsort(arr, lo, hi):
;       if lo >= hi: return
;       p = partition(arr, lo, hi)
;       qsort(arr, lo, p - 1)
;       qsort(arr, p + 1, hi)
;
;   partition(arr, lo, hi):
;       pivot = arr[hi]
;       i = lo - 1
;       for j = lo to hi - 1:
;           if arr[j] <= pivot:
;               i = i + 1
;               swap arr[i], arr[j]
;       swap arr[i + 1], arr[hi]
;       return i + 1
;

; Windows C decorates the names it exports with a leading underscore and
; Linux C does not, so the same source would otherwise need two spellings of
; every entry point. -d ELF_TYPE, which the shared Makefile fragment passes
; on Linux, selects the respelling here. It's the same trick asm_io.inc
; uses for _asm_main in the bootcamp blocks. Leave this block alone.
%ifdef ELF_TYPE
  %define _qsort_asm qsort_asm
  section .note.GNU-stack noalloc noexec nowrite progbits
%endif

segment .text
        global  _qsort_asm
_qsort_asm:
        enter   0,0
        pusha

        ;
        ; TODO: sort.
        ;
        ; Remember the array elements are four bytes, so index with a *4
        ; scale. The first thing to handle is the empty and single-element
        ; arrays: n <= 1 sorts itself.
        ;

        popa
        mov     eax, 0
        leave
        ret
