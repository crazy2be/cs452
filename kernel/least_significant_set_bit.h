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

    // This method of doing log base 2 is inspired by
    // https://graphics.stanford.edu/~seander/bithacks.html#IntegerLog,
    // but has been modified in expression to optimize for what operations
    // are cheapest on ARM assembly.
    // This only works since we know n2 to be a power of 2, thanks to
    // our little trick above.

	// At each step, logical right shift by i bits
	// If the result is non-zero, we know that the set bit is in at least
	// the i+1th position, so we add i to the count.
	// Then update n to the right-shifted value to repeat.
	// (This is effectively a binary search)
	// Each one of these optimizes to 3 ARM instructions, like
	//     lsrs scratch, n, #16
	//     addne log, log, #16
	//     movne n, scratch
    if ((scratch = n >> 16)) { log += 16; n = scratch; }
    if ((scratch = n >> 8))  { log += 8;  n = scratch; }
    if ((scratch = n >> 4))  { log += 4;  n = scratch; }

	// this is a bit of a hack
	// at this point, the number is one of 0x8, 0x4, 0x2, 0x1
	// for which we want to add 3, 2, 1 and 0 respectively
	// we can cheat a bit, to skip the last two rounds of iteration
	// for all but 0x8, n/2 is the right relation
    log += n/2;
	// subtract back n/8, which subtracts 1 for n=8, and leaves the other cases
	// alone this compensates for the above addition adding too much for n=8
    log -= n/8;

    return log;
}
