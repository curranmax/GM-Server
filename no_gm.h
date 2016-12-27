
#ifndef _NO_GM_H_
#define _NO_GM_H_

class NoGM : public GM{
public:
	NoGM() : GM(-1,"") {}
	~NoGM() {}
	
	bool setValue(int v) { return false; }

	bool connectDevice() { return false; }
	bool disconnectDevice() { return true; }

	bool isNull() const { return true; }
};

#endif
