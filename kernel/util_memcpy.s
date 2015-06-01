.text
.globl memcpy

@ naive C code for this looks like:

@ void* memcpy(void *dst, const void *src, size_t len) {
@     unsigned char *p = (unsigned char*) dst;
@     while (len--) {
@         *p++ = *(unsigned char*)src++;
@     }
@     return dst;
@ }

memcpy:
    @ deal with the zero-len case immediately,
    @ since this allows us to write do-loops
    movs r3, r0
    bxeq lr

    @ save the destination pointer, as required by the spec
    stmfd sp!, {r0}

    @ check to see if we need to fall back to byte-aligned mode
    @ either the pointers aren't both aligned to a word, or the
    @ length isn't divisible by 4
    orr r3, r0, r1
    orr r3, r3, r2
    ands r3, r3, #0x3
    bne memcpy_byte_aligned
@ implement 8 word case later
memcpy_8_word_aligned:
memcpy_word_aligned:
    ldr r3, [r1], #4
    subs r2, r2, #4
    str r3, [r0], #4
    bne memcpy_word_aligned
    ldmfd sp!, {r0}
    bx lr

memcpy_byte_aligned:
    ldrsb r3, [r1], #1
    subs r2, r2, #1
    strsb r3, [r0], #1
    bne memcpy_byte_aligned
    ldmfd sp!, {r0}
    bx lr
@ the naive implementation
