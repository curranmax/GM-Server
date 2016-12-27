
#ifndef _NO_DIODE_H_
#define _NO_DIODE_H_

#include <diode.h>

class NoDiode : public Diode{
public:
	NoDiode() : Diode(-1, "", -1, -1) {}
	~NoDiode() {}
	
	float getValue() { return 0.0; }

	bool connectDevice() { return false; }
	bool disconnectDevice() { return true; }

	bool isNull() const { return true; }

	void blinkDaqLed(int num_blinks) {}
};

#endif
