#pragma once

// a polynomial is represented as an array of coefficients
// { a, b, c } represents the polynomial a + b x + b x^2

extern const long long fixed_point_scale;

int integrate_polynomial(long long lo, long long hi, long long *coefs, unsigned n);

struct curve_scaling {
	long long x_scale;
	long long y_scale;
};

struct curve_scaling scale_deceleration_curve(long long v0, long long d, long long *coefs,
		unsigned n, long long t1_generic);

long long evaluate_polynomial_fp(long long x, long long *coefs, unsigned n);
