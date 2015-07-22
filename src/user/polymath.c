#include "polymath.h"
#include <assert.h>

const long long fixed_point_scale = 1 << 20;

// internally, we use fixed point math (10 binary places) for x
long long evaluate_polynomial_fp(long long x, const long long *coefs, unsigned n) {
	long long result = 0;
	long long xp = 1 * fixed_point_scale;

	for (unsigned i = 0; i < n; i++) {
		result += (coefs[i] * xp) / fixed_point_scale;
		xp = (xp * x) / fixed_point_scale;
	}

	return result;
	/* int trunc = (int) result; */

	/* ASSERT(trunc == result); */

	/* return trunc; */
}

static long long evaluate_antiderivative_fp(long long x, const long long *coefs, unsigned n) {
	long long result = 0;
	long long xp = x;

	for (unsigned i = 0; i < n; i++) {
		result += (coefs[i] * xp) / fixed_point_scale / (i + 1);
		xp = (xp * x) / fixed_point_scale;
	}

	return result;
}

long long integrate_polynomial(long long lo, long long hi, const long long *coefs, unsigned n) {
	return (evaluate_antiderivative_fp(hi, coefs, n)
		- evaluate_antiderivative_fp(lo, coefs, n));
}

struct curve_scaling scale_deceleration_curve(long long v0, long long d, const long long *coefs, unsigned n, long long t1) {
	struct curve_scaling cs;
	ASSERT(coefs[0] != 0);
	// note: coefs[0] == f(0)
	cs.y_scale = v0 * fixed_point_scale / coefs[0];
	long long d_theoretical = evaluate_antiderivative_fp(t1, coefs, n);
	cs.x_scale = cs.y_scale * d_theoretical / d;

	return cs;
}
