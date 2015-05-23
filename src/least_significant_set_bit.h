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
static inline unsigned least_significant_set_bit(unsigned n) {
    unsigned log = 0;
    unsigned scratch;

	n = n & -n; // MAGIC that makes only lowest bit set in result

    // This method of doing log base 2 is from
    // https://graphics.stanford.edu/~seander/bithacks.html#IntegerLog,
    // credited to John Owens.
    // This implementation has been modified somewhat to optimize for
    // what arm assembly is capable of doing.
    // This only works since we know n to be a power of 2.
    // The original method repeatedly ANDs the value with magic constants.
    // However, ARM instructions can only use a very limited range of constants
    // as immediate values, so we use iterative logical right shift as an alternative.

    __asm__ (
        // At each step, we know the set bit is in the lowest 2i bits of n
        // We logical right shift by i bits
        // If the result is non-zero, we know that n >= 2^i,
        // so we use the identity `log_2(n) = i + log_2(n / 2^i)`
        // We add i to the log, then update n to the right-shifted value to repeat.
        // After each step, we know that n < 2^i, and we repeat the algorithm
        // to calculate the log iteratively.
        "lsrs %1, %2, #16\n\t"
        "addne %0, %0, #16\n\t"
        "movne %2, %1\n\t"

        // Repeat the algorithm for shifts of 8 and 4 bit
        "lsrs %1, %2, #8\n\t"
        "addne %0, %0, #8\n\t"
        "movne %2, %1\n\t"

        "lsrs %1, %2, #4\n\t"
        "addne %0, %0, #4\n\t"
        "movne %2, %1\n\t"

        // This is a bit of a hack.
        // At this point, the number is one of 0x8, 0x4, 0x2, 0x1
        // for which we want to add 3, 2, 1 and 0 respectively
        // We can cheat a bit, to skip the last round of iteration.
        // It turns out that log_2 for these 4 cases is equal to n/2 - n/8.
        "add %0, %0, %2, lsr #1\n\t"
        "sub %0, %0, %2, lsr #3"

        : "+r"(log), "=r"(scratch), "+r"(n)
    );

    return log;
}
