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
;   * Recursive. An iterative sort with an explicit stack array does not
;     satisfy the activity.
;   * Lomuto partition with the last element as pivot (the manual's scheme).
;   * No calls into the C standard library.
;   * Clean the arguments off the stack after each call. The next pop
;     otherwise loads an argument into a register you promised to
;     preserve, and the outer call's array base is gone.
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

; Windows C puts a leading underscore on every exported name. Linux C does
; not. The Makefile passes -d ELF_TYPE on Linux. This block then respells
; the names below to match. asm_io.inc does the same for _asm_main in the
; bootcamp blocks. Leave this block alone.
%ifdef ELF_TYPE
  %define _qsort_asm qsort_asm
  section .note.GNU-stack noalloc noexec nowrite progbits
%endif

segment .text
        global  _qsort_asm
_qsort_asm:
        enter   0,0
        pusha

        mov     eax, [ebp+12]           ; n
        cmp     eax, 2
        jl      .done                   ; n <= 1 (signed): already sorted
        dec     eax                     ; hi = n-1
        push    eax
        push    dword 0                 ; lo = 0
        push    dword [ebp+8]           ; arr
        call    qs_rec
        add     esp, 12                 ; clean the three args
.done:
        popa
        mov     eax, 0
        leave
        ret

; qs_rec(arr, lo, hi)   [ebp+8]=arr [ebp+12]=lo [ebp+16]=hi
; ebx=arr  ecx=i+1 (next slot for a value <= pivot)  edx=pivot
; esi=j, then the pivot index p (callee-saved, so it survives the calls)
qs_rec:
        push    ebp
        mov     ebp, esp
        push    ebx
        push    esi
        push    edi

        mov     ebx, [ebp+8]
        mov     ecx, [ebp+12]           ; i+1 = lo
        mov     esi, ecx                ; j = lo
        cmp     ecx, [ebp+16]
        jge     .ret                    ; lo >= hi: base case
        mov     eax, [ebp+16]
        mov     edx, [ebx+eax*4]        ; pivot = arr[hi]
.loop:
        cmp     esi, [ebp+16]
        jge     .place
        mov     eax, [ebx+esi*4]
        cmp     eax, edx                ; cmp, not sub: no overflow at INT_MIN/MAX
        jg      .next                   ; signed jump: arr[j] > pivot stays put
        mov     edi, [ebx+ecx*4]        ; swap arr[i+1], arr[j]
        mov     [ebx+ecx*4], eax
        mov     [ebx+esi*4], edi
        inc     ecx
.next:
        inc     esi
        jmp     .loop
.place:
        mov     esi, [ebp+16]           ; swap arr[i+1], arr[hi]
        mov     eax, [ebx+ecx*4]
        mov     [ebx+esi*4], eax
        mov     [ebx+ecx*4], edx
        mov     esi, ecx                ; p = i+1
        lea     eax, [esi-1]
        push    eax                     ; qs_rec(arr, lo, p-1)
        push    dword [ebp+12]
        push    ebx
        call    qs_rec
        add     esp, 12
        push    dword [ebp+16]          ; qs_rec(arr, p+1, hi)
        lea     eax, [esi+1]
        push    eax
        push    ebx
        call    qs_rec
        add     esp, 12
.ret:
        pop     edi
        pop     esi
        pop     ebx
        pop     ebp
        ret
