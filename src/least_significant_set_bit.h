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
	const int n2 = n & -n; // MAGIC that makes only lowest bit set in result
    int log = 0;
    int i;

    // this method of doing log base 2 is from
    // https://graphics.stanford.edu/~seander/bithacks.html#IntegerLog,
    // credited to John Owens
    // this only works since we know n2 to be a power of 2
    static const unsigned masks[5] = {0xaaaaaaaa, 0xcccccccc, 0xf0f0f0f0,
        0xff00ff00, 0xffff0000};
    // TODO: it may make sense to unroll this loop, or have this whole function inlined
    for (i = 0; i < 5; i++) {
        log += ((masks[i] & n2) != 0) << i;
    }
    return log;
}
