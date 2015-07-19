#include "polymath.h"
#include <assert.h>

const long long fixed_point_scale = 1 << 20;

// internally, we use fixed point math (10 binary places) for x
long long evaluate_polynomial_fp(long long x, long long *coefs, unsigned n) {
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

static long long evaluate_antiderivative_fp(long long x, long long *coefs, unsigned n) {
	long long result = 0;
	long long xp = x;

	for (unsigned i = 0; i < n; i++) {
		result += (coefs[i] * xp) / fixed_point_scale / (i + 1);
		xp = (xp * x) / fixed_point_scale;
	}

	return result;
}

int integrate_polynomial(long long lo, long long hi, long long *coefs, unsigned n) {
	return (evaluate_antiderivative_fp(hi, coefs, n)
		- evaluate_antiderivative_fp(lo, coefs, n));
}

struct curve_scaling scale_deceleration_curve(long long v0, long long d, long long *coefs, unsigned n, long long t1) {
	struct curve_scaling cs;
	ASSERT(coefs[0] != 0);
	// note: coefs[0] == f(0)
	cs.y_scale = v0 * fixed_point_scale / coefs[0];
	long long d_theoretical = evaluate_antiderivative_fp(t1, coefs, n);
	cs.x_scale = cs.y_scale * d_theoretical / d;

	/* // We now want to produce an x_scale such that: */
	/* // WRONG integrate_polynomial(0, x_scale * t1_generic, coefs, n) * y_scale == d */
	/* // This is a bit more complicated, and we use newton's method for this. */

	/* // Recall that newton's method uses the function, and its derivative. */
	/* // Since we're integrating over the original function, the derivative */
	/* // of that is the original function. */

	/* cs.x_scale = 1 * fixed_point_scale; */
	/* const int tolerance = 1000; */
	/* int iterations_left = 100; */
	/* long long err; */
	/* do { */
	/* 	long long scaled_coefs[n]; */
	/* 	long long alpha = 1 * fixed_point_scale; */
	/* 	for (unsigned i = 0; i < n; i++) { */
	/* 		scaled_coefs[i] = coefs[i] * alpha / fixed_point_scale; */
	/* 		alpha = alpha * cs.x_scale / fixed_point_scale; */
	/* 	} */

	/* 	// Newton's method: x_1 = x_0 - f(x) / f'(x) */
	/* 	// long long delta_d = evaluate_antiderivative_fp(t1, coefs, n) - (d * fixed_point_scale / cs.y_scale); */
	/* 	long long scaled_d = d * coefs[0] / v0; */
	/* 	long long t1p = t1 * fixed_point_scale / cs.x_scale; */
	/* 	long long projected_d = evaluate_antiderivative_fp(t1p, scaled_coefs, n); */
	/* 	long long delta_d = projected_d - scaled_d; */
	/* 	long long derivative = evaluate_polynomial_fp(t1p, scaled_coefs, n); */
	/* 	long long x_scale_prime = cs.x_scale - delta_d / derivative; */

	/* 	err = x_scale_prime - cs.x_scale; */
	/* 	if (err < 0) err = -err; */

	/* 	printf("x0 = %d, x1 = %d, t1p = %d, projected_d = %d, scaled_d = %d, delta_d = %d, derivative = %d, err = %d" EOL, */
	/* 		(int) cs.x_scale / (1 << 10), (int) x_scale_prime / (1 << 10), (int) t1p / (1 << 10), (int) projected_d / (1 << 10), (int) scaled_d / (1 << 10), */
	/* 		(int) delta_d / (1 << 10), (int) derivative / (1 << 10), (int) err / (1 << 10)); */

	/* 	cs.x_scale = x_scale_prime; */
	/* } while (err > tolerance && --iterations_left > 0); */

	return cs;
}
