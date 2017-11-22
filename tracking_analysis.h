
#ifndef _TRACKING_ANALYSIS_H_
#define _TRACKING_ANALYSIS_H_

#include <vector>

class GMVal {
public:
	GMVal(int h_gm_, int v_gm_) : h_gm(h_gm_), v_gm(v_gm_) {}
	~GMVal() {}

	GMVal(const GMVal &other_gm_val) : h_gm(other_gm_val.h_gm), v_gm(other_gm_val.v_gm) {}
	const GMVal& operator=(const GMVal &other_gm_val) {
		h_gm = other_gm_val.h_gm;
		v_gm = other_gm_val.v_gm;

		return *this;
	}

	int h_gm;
	int v_gm;
};

struct GMValComp {
	bool operator()(const GMVal &lhs, const GMVal &rhs) const {
		return lhs.h_gm < rhs.h_gm || (lhs.h_gm == rhs.h_gm && lhs.v_gm < rhs.v_gm);
	}
};

class TrackingAnalysis {
public:
	TrackingAnalysis() {}
	~TrackingAnalysis() {}

	void addVal(const GMVal& gm_val) { vals.push_back(gm_val); }

	// Finds the average change in the magnitude between iterations
	float avgIterChange() const;
private:
	std::vector<GMVal> vals;
};

#endif