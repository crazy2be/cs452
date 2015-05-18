#pragma once

// returns the index of the least significant set bit in n, counting from zero
// if there are no set bits in n, returns zero (which is the same return value
// as if n has the lsb set, so callers should check for n being zero before
// calling this function)
static inline int highest_priority(int n) {
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
