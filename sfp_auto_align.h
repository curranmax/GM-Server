
#ifndef _SFP_AUTO_ALIGN_H_
#define _SFP_AUTO_ALIGN_H_

#include "FSO.h"
#include "args.h"
#include "tracking_analysis.h"

#include <string>
#include <netinet/in.h>

class PowerTuple {
public:
	PowerTuple() : powers(), relative_power(0.0) {}

	void addValue(float v) { powers.push_back(v); }
	void addValue(float v, int option) {
		// option == 1 is full relative.
		// option == 2 is center absolute, rest are relative.
		// Any other value is full absolute.
		if(powers.size() == 0) {
			relative_power = v;
		}

		if(option == 1 || (option == 2 && powers.size() >= 1)) {
			v = v - relative_power;
		}
		addValue(v);
	}

	float squaredEuclideanDistance(const PowerTuple& other_tuple) const;

	int size() const { return powers.size(); }

	bool allZero() const {
		for(int i = 0; i < size(); ++i) {
			if(abs(powers[i]) > 0.0000001) {
				return false;
			}
		}
		return true;
	}

	std::vector<float> powers;
	float relative_power;
};

typedef std::map<GMVal, float, GMValComp> PowerMap;
typedef std::map<GMVal, PowerTuple, GMValComp> PowerTupleMap;

// SFPAutoAligner should already have a udp connection and decided which side is the controller
// TODO change the name of this class to SFPTracking or something, and implement a true SFP auto alignment (or half-auto).
class SFPAutoAligner {
public:
	enum SockType{UDP, TCP};

	// GetPowerMode defines 
	enum GetPowerMode{SLEEP, MULTI};

	SFPAutoAligner(int sock_, SockType sock_type_);
	~SFPAutoAligner();

	void run(Args* args_, FSO* fso_, const std::string &other_rack_id, const std::string &other_fso_id);
	void alignRun(Args* args_, FSO* fso_, const std::string &other_rack_id, const std::string &other_fso_id);

	void setForeignSockAddr(const struct sockaddr_in &foreign_host_) { foreign_host = foreign_host_; }

	void setSleepDuration(int sleep_milliseconds_) { get_power_mode = GetPowerMode::SLEEP; sleep_milliseconds = sleep_milliseconds_; }
	void setMultiParam(int max_num_messages_, int max_num_changes_, int num_message_average_) {
		get_power_mode = GetPowerMode::MULTI;
		max_num_messages = max_num_messages_;
		max_num_changes = max_num_changes_;
		num_message_average = num_message_average_;
	}

	// Helper functions to create SFPAutoAligner
	static SFPAutoAligner* connectTo(int send_port, SFPAutoAligner::SockType sock_type, const std::string &host_addr, const std::string &rack_id, const std::string &fso_id);

private:
	void setValues(Args* args_, FSO* fso_) { args = args_; fso = fso_; }
	
	void controllerRun();
	void controllerRunConstantUpdate();
	void mapRun();
	void mapRunWithSearch();
	void switchRun();

	void fillSearchLocs(std::vector<GMVal>* search_locs, int num_search_locs, int search_delta);
	
	void findError(const PowerTuple &this_tuple,
					const PowerTupleMap &power_map,
					int &h_err, int &v_err);

	void listenerRun();

	float getPower(int h_gm, int v_gm, bool no_update);
	void endTracking();

	void givePowerVoltage(float volt);

	void sendMsg(const std::string &msg);
	void recvMsg(std::string &msg);

	FSO* fso;

	int sock;
	SockType sock_type;
	struct sockaddr_in foreign_host; // The address to send UDP packets to.

	GetPowerMode get_power_mode;
	int sleep_milliseconds;
	int max_num_messages;
	int max_num_changes;
	int num_message_average;

	Args* args;
};

#endif
