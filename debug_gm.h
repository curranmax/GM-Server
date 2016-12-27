
#ifndef _DEBUG_GM_H_
#define _DEBUG_GM_H_

#include <gm.h>

class DebugGM : public GM {
  public:
	DebugGM(int channel,const std::string &serial_number) : GM(channel, serial_number) {}
	~DebugGM() { disconnectDevice(); }

	// Changes analog output (i.e. changes angle of mirror)
	bool setValue(int v);

	// helper function to Connects to device
	bool connectDevice() { if(!_getConnected()) { std::cout << getHeader() << " connected" << std::endl; } _setConnected(true); return true; }
	bool disconnectDevice() { if(_getConnected()) { std::cout << getHeader() << " disconnected" << std::endl; } _setConnected(false); return true; }

  private:
	std::string getHeader() const { return "GM " + std::to_string(_getChannel()); }	
	
};

#endif
