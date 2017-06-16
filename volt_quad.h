
#ifndef _VOLT_QUAD_H_
#define _VOLT_QUAD_H_

#include <math.h>
#include <cmath>

class VoltQuad {
  public:
	VoltQuad() : hp(0.0), hn(0.0), vp(0.0), vn(0.0), set_neg_to_zero(true) {}
	VoltQuad(float hp_, float hn_, float vp_, float vn_)
			: hp(hp_), hn(hn_), vp(vp_), vn(vn_), set_neg_to_zero(true) {}
	
	VoltQuad(const VoltQuad& vq) {
		hp = vq.hp;
		hn = vq.hn;
		vp = vq.vp;
		vn = vq.vn;
	}

	const VoltQuad& operator=(const VoltQuad& vq) {
		hp = vq.hp;
		hn = vq.hn;
		vp = vq.vp;
		vn = vq.vn;

		return *this;
	}

	~VoltQuad() {}

	float dist(const VoltQuad& vq) const {
		return sqrt(pow(h_p() - vq.h_p(), 2) + pow(h_n() - vq.h_n(), 2) + pow(v_p() - vq.v_p(), 2) + pow(v_n() - vq.v_n(), 2));
	}

	float h_p() const { return getValue(hp); }
	float h_n() const { return getValue(hn); }
	float v_p() const { return getValue(vp); }
	float v_n() const { return getValue(vn); }

  private:
	float getValue(float v) const {
		if(set_neg_to_zero && v < 0.035) {
			return 0.0;
		}
		return v;
	}

	float hp, hn, vp, vn;
	bool set_neg_to_zero;
};

#endif
