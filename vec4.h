
#ifndef _VEC4_H_
#define _VEC4_H_

#include <math.h>

class Vec4 {
public:
	Vec4() : x(0.0), y(0.0), z(0.0), w(0.0) {}
	Vec4(float a, float b, float c, float d) : x(a), y(b), z(c), w(d) {}
	Vec4(const Vec4 &v) : x(v.x), y(v.y), z(v.z), w(v.w) {}
	~Vec4() {}

	const Vec4& operator=(const Vec4 &v) { x = v.x; y = v.y; z = v.z; w = v.w; return *this; }

	void norm() {
		float f = mag();
		scale(1.0 / f);
	}
	float mag() const { return sqrt(x * x + y * y + z * z + w * w); }
	void scale(float f) {
		x *= f;
		y *= f;
		z *= f;
		w *= f;
	}

	float dot(const Vec4 &v) { return x * v.x + y * v.y + z * v.z + w * v.w; }
	float dot(float a, float b, float c, float d) { return x * a + y * b + z * c + w * d; }
	float angle(float a, float b, float c, float d) { return angle(Vec4(a,b,c,d)); }
	float angle(const Vec4 &v) {
		float bc = dot(v) / mag() / v.mag();
		if(bc >= 1.0) {
			return 0.0;
		}
		if(bc <= -1.0) {
			return M_PI;
		}
		return acos(bc);
	}

	float xv() const { return (fabs(x) > 0.001 ? x : 0.0); }
	float yv() const { return (fabs(y) > 0.001 ? y : 0.0); }
	float zv() const { return (fabs(z) > 0.001 ? z : 0.0); }
	float wv() const { return (fabs(w) > 0.001 ? w : 0.0); }
private:
	float x,y,z,w;
};

#endif
