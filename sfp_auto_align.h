
#ifndef _SFP_AUTO_ALIGN_H_
#define _SFP_AUTO_ALIGN_H_

#include "FSO.h"
#include "args.h"

#include <string>

// SFPAutoAligner should already have a udp connection and decided which side is the controller
class SFPAutoAligner {
public:
	enum SockType{UDP, TCP};

	// GetRSSIMode defines 
	enum GetRSSIMode{SLEEP, MULTI};

	SFPAutoAligner(int sock_, SockType sock_type_);
	~SFPAutoAligner();

	void run(Args* args_, FSO* fso_, const std::string &other_rack_id, const std::string &other_fso_id);

	void setForeignSockAddr(const struct sockaddr_in &foreign_host_) { foreign_host = foreign_host_; }

	void setSleepDuration(int sleep_milliseconds_) { get_rssi_mode = GetRSSIMode::SLEEP; sleep_milliseconds = sleep_milliseconds_; }
	void setMultiParam(int num_messages_) { get_rssi_mode = GetRSSIMode::MULTI; num_messages = num_messages_; }

	// Helper functions to create SFPAutoAligner
	static SFPAutoAligner* connectTo(int send_port, SFPAutoAligner::SockType sock_type, const std::string &host_addr, const std::string &rack_id, const std::string &fso_id);

private:
	void setValues(Args* args_, FSO* fso_) { args = args_; fso = fso_; }
	
	void controllerRun();
	void mapRun();

	void findError(float center_rssi, const std::vector<std::pair<std::pair<int, int>, float> > &search_rssis, const std::map<std::pair<int, int>, float> &rssi_map, int &h_err, int &v_err);

	float getRSSI(int h_gm, int v_gm);

	void sendMsg(const std::string &msg);
	void recvMsg(std::string &msg);

	FSO* fso;

	int sock;
	SockType sock_type;
	struct sockaddr_in foreign_host; // The address to send UDP packets to.

	GetRSSIMode get_rssi_mode;
	int sleep_milliseconds;
	int num_messages;

	Args* args;
};



#endif
