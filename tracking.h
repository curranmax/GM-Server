
#ifndef _TRACKING_H_
#define _TRACKING_H_

#include "args.h"
#include "FSO.h"

class TrackingSystem {
  public:
	TrackingSystem(int sock_, bool is_controller_);
	~TrackingSystem();

	void run(FSO* fso_, Args* args_);

	void setAsyncListener() { async_listener = true; }

	static TrackingSystem* listenFor(int listen_port, const std::string &rack_id, const std::string &fso_id);
	static TrackingSystem* connectTo(int send_port, const std::string &host_addr, const std::string &rack_id, const std::string &fso_id);

  private:
	void controllerRun();
	void listenerRun();

	float computeResponse(float p_volt, float n_volt);
	void mapVoltage();

	void asyncListenerRun();

	void send_msg(const std::string &msg); // const?
	void recv_msg(std::string &msg); // const?

	// Controller helper functions (sends a message, and parses the responses for appropriate data)
	void get_fso(std::string& other_rack, std::string& other_fso); // const?
	void get_diode_voltage(float& ph_volt, float& nh_volt, float& pv_volt, float& nv_volt); // const?

	void end_tracking();

	// Listener helper functions (construct a response message and sends it back to the controller)
	void give_fso(const std::string& this_rack, const std::string& this_fso);
	void give_diode_voltage(float ph_volt, float nh_volt, float pv_volt, float nv_volt);
	void confirm_end_tracking();

	int sock;
	bool is_controller;

	bool async_listener;
	
	FSO* fso;
	Args* args;
};



#endif
