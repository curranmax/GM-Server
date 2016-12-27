
#ifndef _AUTO_ALIGN_H_
#define _AUTO_ALIGN_H_

#include "FSO.h"
#include "args.h"

#include <string>

typedef std::pair<float,float> weight_type;

// AutoAligner should already have tcp connection and decided which side is the controller
class AutoAligner {
public:
	AutoAligner(int sock_,bool is_controller_);
	~AutoAligner();

	void run(Args* args_,FSO* fso_);

private:
	void setValues(Args* args_,FSO* fso_) { args = args_; fso = fso_; }
	
	void controller_run();
	void other_run();

	void simple_search(int &ct1, int &ct2, int &co1, int &co2, int max_iters);

	void send_msg(const std::string &msg);
	void recv_msg(std::string &msg);

	// Controller helper functions
	bool get_fso(std::string &other_rack_id, std::string &other_fso_id);
	bool get_gm(int &other_gm1,int &other_gm2);
	bool get_pwr(float &other_pwr);
	bool request_pwr();
	bool get_requested_pwr(float &power);

	bool give_fso(const std::string &this_rack_id,const std::string &this_fso_id);
	bool set_gm(int gm1,int gm2);
	bool save_gm();
	bool quit();

	weight_type calc_weight(const std::pair<float,float> &pwr_vals);
	bool better_weight(const weight_type &w1,const weight_type &w2);
	bool same_weight(const weight_type &w1,const weight_type &w2);

	FSO* fso;

	int sock;
	bool is_controller;

	int max_delta, step;
	float delay; // delay is in seconds

	Args* args;
};

// Helper functions to create AutoAligner
AutoAligner* listenFor(int listen_port,const std::string &rack_id,const std::string &fso_id);
AutoAligner* connectTo(int send_port,const std::string &host_addr,const std::string &rack_id,const std::string &fso_id);

#endif
