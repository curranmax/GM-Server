
#include "debug_gm.h"

#include <unistd.h>

bool DebugGM::setValue(int v) {
	int this_value = _getValue();
	if(v < MIN_GM_VALUE){
		v = MIN_GM_VALUE;
	}
	if(v > MAX_GM_VALUE){
		v = MAX_GM_VALUE;
	}
	bool rv = (this_value != v) && _getConnected();
	if(rv) {
		_setValue(v);
		// std::cout << getHeader() << " value changed to " << v << std::endl;
	} else {
		// std::cout << getHeader() << " value still " << this_value << std::endl;
	}
	usleep(1000);
	return rv;
}
