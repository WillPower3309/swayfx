#include <math.h>

#include "sway/fx_util/animation_utils.h"

double lerp (double a, double b, double t) {
	return a * (1.0 - t) + b * t;
}

double ease_out_cubic (double t) {
	double p = t - 1;
	return pow(p, 3) + 1;
}
