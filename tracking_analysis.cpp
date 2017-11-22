
#include "tracking_analysis.h"

#include <cmath>
#include <math.h>

float TrackingAnalysis::avgIterChange() const {
	float sum_change = 0.0;
	for(unsigned int i = 0; i < vals.size() - 1; ++i) {
		 sum_change += sqrt(pow(vals[i + 1].h_gm - vals[i].h_gm, 2.0) + pow(vals[i + 1].v_gm - vals[i].v_gm, 2.0));
	}

	return sum_change / float(vals.size() - 1);
}
