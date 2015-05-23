#pragma once

/** @file */

/**
 * Returns the index of the least significant set bit in n.
 *
 * If no bits are set in `n`, the result is considered to be undefined.
 * In practice, 0 is returned, but this collides with the return value
 * for `n=0x1`.
 * Callers should check for n being zero before calling this function.
 *
 * This function is intended to be used to find the highest priority
 * for a set of tasks in the ready queue, by maintaining a mask where
 * the ith bit is 1 if there is at least one ready task of priority i.
 *
 * @param n: the bit field to find the LSB from.
 * There must be at least one bit set in `n`.
 *
 * @return The index of the least significant set bit in `n`, where 0 represents the LSB,
 * and 31 represents the MSB.
 */
static inline int least_significant_set_bit(int n) {
    int log = 0;
    int scratch;

	n = n & -n; // MAGIC that makes only lowest bit set in result

    // this method of doing log base 2 is from
    // https://graphics.stanford.edu/~seander/bithacks.html#IntegerLog,
    // credited to John Owens
    // this implementation has been modified somewhat to optimize for
    // what arm assembly is capable of doing
    // this only works since we know n2 to be a power of 2

    // this has been hand-optimized to reduce the number of instructions
    __asm__ (
        // at each step, logical right shift by i bits
        // if the result is non-zero, we know that the set bit is in at least the i+1th
        // position, so we add i to the count
        // then update n to the right-shifted value to repeat
        "lsrs %1, %2, #16\n\t"
        "addne %0, %0, #16\n\t"
        "movne %2, %1\n\t"

        "lsrs %1, %2, #8\n\t"
        "addne %0, %0, #8\n\t"
        "movne %2, %1\n\t"

        "lsrs %1, %2, #4\n\t"
        "addne %0, %0, #4\n\t"
        "movne %2, %1\n\t"

        "lsrs %1, %2, #2\n\t"
        "addne %0, %0, #2\n\t"
        "movne %2, %1\n\t"

        "lsrs %1, %2, #1\n\t"
        "addne %0, %0, #1"

        : "+r"(log), "=r"(scratch), "+r"(n)
    );

    // equivalent C code left here to document what it is that I'm doing
    /*
    if (0xffff0000 & n2) log += 16;
    if (0xff00ff00 & n2) log += 8;
    if (0xf0f0f0f0 & n2) log += 4;
    if (0xcccccccc & n2) log += 2;
    if (0xaaaaaaaa & n2) log += 1;
    */

    return log;
}
