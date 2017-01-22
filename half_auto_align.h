
#ifndef _HALF_AUTO_ALIGN_H_
#define _HALF_AUTO_ALIGN_H_

#include "args.h"
#include "FSO.h"

class HalfAutoAlign {
  public:
	HalfAutoAlign(int sock_, bool is_controller_);
	~HalfAutoAlign();
	
	void run(FSO* fso_, Args* args_);

	static HalfAutoAlign* listenFor(int listen_port, const std::string &rack_id, const std::string &fso_id);
	static HalfAutoAlign* connectTo(int send_port, const std::string &host_addr, const std::string &rack_id, const std::string &fso_id);
  private:
  	// Controller should have a Diode set up as a power meter
	void controllerRun();
	// Listener should have a GM
	void listenerRun();

	void send_msg(const std::string &msg);
	void recv_msg(std::string &msg);

	// Controller helper functions
	void give_fso(const std::string& this_rack_id, const std::string& this_fso_id);
	void set_to_this_link();
	void get_gm_vals(int &horizontal_gm_value, int &vertical_gm_value);
	void set_gm_vals(int horizontal_gm_value, int vertical_gm_value);
	void save_link();

	void end_half_auto_align();

	// Receiver helper functions
	void confirm_got_fso();
	void confirm_set_to_this_link();
	void give_gm_vals(int horizontal_gm_value, int vertical_gm_value);
	void confirm_set_gm_vals();
	void confirm_save_link();

	void confirm_end_half_auto_align();

	int sock;
	bool is_controller;

	FSO* fso;
	Args* args;
};

#endif
