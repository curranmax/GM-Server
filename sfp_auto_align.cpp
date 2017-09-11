
#include "sfp_auto_align.h"

#include "logger.h"

#include <iostream>
#include <fstream>
#include <sstream>
#include <signal.h>
#include <math.h>
#include <string.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <chrono>
#include <unistd.h>

SFPAutoAligner::SFPAutoAligner(int sock_, SockType sock_type_) {
	sock = sock_;
	sock_type = sock_type_;

	get_rssi_mode = GetRSSIMode::SLEEP;
	sleep_milliseconds = 12;
	num_messages = 1;
}

SFPAutoAligner::~SFPAutoAligner() {
	close(sock);
}

void SFPAutoAligner::run(Args* args_, FSO* fso_, const std::string &other_rack_id, const std::string &other_fso_id) {
	setValues(args_, fso_);

	setMultiParam(100);
	// setSleepDuration(500);

	fso->setToLink(other_rack_id, other_fso_id);

	if(args->sfp_map_power) {
		mapRun();
	} else {
		int init_h_gm = fso->getHorizontalGMVal();
		int init_v_gm = fso->getVerticalGMVal();

		controllerRun();

		char raw_input[256];
		std::string save_tracking_link = "";
		std::cout << "Do you want to save the current gm settings(y) or revert to initial settings(n)?" << std::endl;
		std::cin.getline(raw_input, 256);
		save_tracking_link = raw_input;

		if(save_tracking_link == "n") {
			fso->setHorizontalGMVal(init_h_gm);
			fso->setVerticalGMVal(init_v_gm);
		}

		fso->saveCurrentSettings(other_rack_id, other_fso_id);
		fso->save();
		std::cout << "FSO saved!!!" << std::endl;
	}
}

bool sfp_controller_loop = false;
void sfpControllerHandler(int s) {
	sfp_controller_loop = false;
}

void SFPAutoAligner::controllerRun() {
	LOG("controllerRun start");

	// Build Table
	std::string in_file = args->sfp_map_in_file;
	std::ifstream ifstr(in_file, std::ifstream::in);
	std::map<std::pair<int, int>, float> rssi_map;

	LOG("getting data from: " + in_file);

	{
		// Reads the given file and fills the rssi_map.
		std::string line;
		int hd, vd;
		float rssi;
		int line_number = 0;
		while(std::getline(ifstr, line)) {
			if(line == "\n") {
				continue;
			}
			line_number += 1;
			if(line_number <= 2) {
				continue;
			}
			std::stringstream sstr(line);
			sstr >> hd >> vd >> rssi;

			// TODO exclude zero values
			rssi_map[std::make_pair(hd, vd)] = rssi;

			{
				std::stringstream sstr;
				sstr << "added entry to rssi_map: (" << hd << ", " << vd << ") -> " << rssi;
				VLOG(sstr.str(), 1);
			}
		}
	}

	float tracking_start = args->sfp_tracking_start;
	float tracking_stop = args->sfp_tracking_stop;
	float max_rssi = 0;

	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = sfpControllerHandler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;

	sigaction(SIGINT, &sigIntHandler, NULL);
	sfp_controller_loop = true;

	std::vector<std::pair<int, int> > search_locs;
	int num_search_locs = args->sfp_num_search_locs;
	int search_delta = args->sfp_search_delta;

	// Creates the search locations to use.
	if(num_search_locs == 3) {
		search_locs.push_back(std::make_pair(search_delta, 0));
		search_locs.push_back(std::make_pair(0, search_delta));
	} else if(num_search_locs == 5) {
		search_locs.push_back(std::make_pair(search_delta, 0));
		search_locs.push_back(std::make_pair(0, search_delta));
		search_locs.push_back(std::make_pair(-search_delta, 0));
		search_locs.push_back(std::make_pair(0, -search_delta));
	} else {
		std::cerr << "Invalid value for num_search_locs: " << num_search_locs << std::endl;
		return;
	}

	{
		// Logs search locations used.
		std::stringstream sstr;
		for(unsigned int i = 0; i < search_locs.size(); ++i) {
			sstr << "(" << search_locs[i].first << ", " << search_locs[i].second << ")";

			if(i < search_locs.size() - 1) {
				sstr << ", ";
			}
		}

		LOG("search locs are: {" + sstr.str() + "}");
	}

	float k_proportional = args->k_proportional;
	float k_integral = args->k_integral;
	float k_derivative = args->k_derivative;

	bool tracking = false;
	int h_gm, v_gm;

	int sum_h_err = 0, sum_v_err = 0, prev_h_err = 0, prev_v_err = 0;

	int num_iters = 0;

	std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();
	while(sfp_controller_loop) {
		num_iters += 1;

		h_gm = fso->getHorizontalGMVal();
		v_gm = fso->getVerticalGMVal();

		// Get current power
		float this_rssi = getRSSI(h_gm, v_gm);

		LOG("---------start tracking iter---------");
		{
			std::stringstream sstr;
			sstr << "GM at (" << h_gm << ", " << v_gm << ") with RSSI of " << this_rssi;
			LOG(sstr.str());
		}
		std::cout << "GM at (" << h_gm << ", " << v_gm << ") with RSSI of " << this_rssi;

		if(this_rssi > max_rssi) {
			max_rssi = this_rssi;
		}

		float relative_difference = (max_rssi - this_rssi) /  max_rssi;
		if(!tracking && relative_difference >= tracking_start) {
			LOG("tracking turned on");
			tracking = true;
		}
		if(tracking && relative_difference < tracking_stop) {
			LOG("tracking turned off");
			tracking = false;
		}

		if(tracking) {
			std::vector<std::pair<std::pair<int, int>, float> > search_rssis;
			// Query the necessary points
			for(unsigned int i = 0; i < search_locs.size(); ++i) {
				fso->setHorizontalGMVal(search_locs[i].first + h_gm);
				fso->setVerticalGMVal(search_locs[i].second + v_gm);

				{
					std::stringstream sstr;
					sstr << "search: GM set to (" << search_locs[i].first + h_gm << "[" << search_locs[i].first << "], " << search_locs[i].second + v_gm << "[" << search_locs[i].second << "])";
					LOG(sstr.str());
				}

				float search_rssi = getRSSI(search_locs[i].first + h_gm, search_locs[i].second + v_gm);

				{
					std::stringstream sstr;
					sstr << "search: GM at (" << search_locs[i].first + h_gm << "[" << search_locs[i].first << "], " << search_locs[i].second + v_gm << "[" << search_locs[i].second << "]) with RSSI of " << search_rssi;
					LOG(sstr.str());
				}

				search_rssis.push_back(std::make_pair(search_locs[i], search_rssi));
			}

			// Search the table for the correction
			int h_err, v_err;
			findError(this_rssi, search_rssis, rssi_map, h_err, v_err);

			{
				std::stringstream sstr;
				sstr << "computed error is (" << h_err << ", " << v_err << ")"; 
				LOG(sstr.str());
			}

			sum_h_err += h_err;
			sum_v_err += v_err;

			float hr = k_proportional * -h_err + k_integral * -sum_h_err + k_derivative * (prev_h_err - h_err);
			float vr = k_proportional * -v_err + k_integral * -sum_v_err + k_derivative * (prev_v_err - v_err);

			{
				std::stringstream sstr;
				sstr << "response is (" << hr << ", " << vr << ")";
				LOG(sstr.str());
			}

			fso->setHorizontalGMVal(h_gm + hr);
			fso->setVerticalGMVal(v_gm + vr);

			{
				std::stringstream sstr;
				sstr << "GM set to (" << h_gm + hr << ", " << v_gm + vr << ")";
				LOG(sstr.str());
			}

			std::cout << ", tracking moved (" << hr << ", " << vr << ")" << std::endl;
		} else {
			std::cout << ", no tracking" << std::endl;
		}
	}
	signal(SIGINT, SIG_DFL);

	std::chrono::time_point<std::chrono::system_clock> end = std::chrono::system_clock::now();
	std::chrono::duration<double> dur = end - start;
	float total_seconds = dur.count();
	float seconds_per_iter = total_seconds / float(num_iters);
	std::cout << "Avg Iteration time: " << seconds_per_iter * 1000.0 << " milliseconds per iter" << std::endl;

	LOG("controllerRun end");
}

// TODO other error estimation method is to use triangulation.
void SFPAutoAligner::findError(float center_rssi,
				const std::vector<std::pair<std::pair<int, int>, float> > &search_rssis,
				const std::map<std::pair<int, int>, float> &rssi_map,
				int &h_err, int &v_err) {
	h_err = 0;
	v_err = 0;

	bool first = true;
	float best_diff = 0.0;

	{
		std::stringstream sstr;
		sstr << "start findError: (0,0)->" << center_rssi;
		for(unsigned int i = 0; i < search_rssis.size(); ++i) {
			sstr << ", (" << search_rssis[i].first.first << "," << search_rssis[i].first.second << ")->" << search_rssis[i].second;
		}
		LOG(sstr.str());
	}

	for(std::map<std::pair<int, int>, float>::const_iterator itr = rssi_map.cbegin(); itr != rssi_map.cend(); ++itr) {
		float this_diff = pow(center_rssi - itr->second, 2);
		for(std::vector<std::pair<std::pair<int, int>, float> >::const_iterator search_itr = search_rssis.cbegin(); search_itr != search_rssis.cend(); ++search_itr) {
			std::pair<int, int> lookup_loc = std::make_pair(itr->first.first + search_itr->first.first, itr->first.second + search_itr->first.second);

			float lookup_rssi = 0.0;
			if(rssi_map.count(lookup_loc) > 0) {
				lookup_rssi = rssi_map.at(lookup_loc);
			}

			this_diff += pow(search_itr->second - lookup_rssi, 2);
		}

		if(first) {
			first = false;
			h_err = itr->first.first;
			v_err = itr->first.second;

			best_diff = this_diff;
		} else {
			if(this_diff < best_diff ||
					(this_diff == best_diff &&
						(pow(itr->first.first, 2) + pow(itr->first.second, 2) < pow(h_err, 2) + pow(v_err, 2)))) {
				h_err = itr->first.first;
				v_err = itr->first.second;

				best_diff = this_diff;
			}
		}
	}

	{
		std::stringstream sstr;
		sstr << "end findError: err = (" << h_err << ", " << v_err << "), and best_diff = " << best_diff;
		LOG(sstr.str());
	}
}

void SFPAutoAligner::mapRun() {
	std::cout << "SFPAutoAligner::mapRun start" << std::endl;

	int h_gm_init = fso->getHorizontalGMVal();
	int v_gm_init = fso->getVerticalGMVal();

	int map_range = args->sfp_map_range;
	int map_step = args->sfp_map_step;
	
	std::string out_file = args->sfp_map_out_file;
	std::ofstream ofstr(out_file, std::ofstream::out);

	ofstr << "PARAMS map_range=" << map_range << " map_step=" << map_step << std::endl;  
	ofstr << "H_Delta V_Delta RSSI" << std::endl;

	for(int h_delta = -map_range; h_delta <= map_range; h_delta += map_step) {
		for(int v_delta = -map_range; v_delta <= map_range; v_delta += map_step) {
			fso->setHorizontalGMVal(h_delta + h_gm_init);
			fso->setVerticalGMVal(v_delta + v_gm_init);

			std::cout << "GM:(" << h_delta << ", " << v_delta << ")";
			
			// Get RSSI
			float rssi = getRSSI(h_delta + h_gm_init, v_delta + v_gm_init);

			std::cout << "\t --> RSSI = " << rssi << std::endl;

			// Save it
			ofstr << h_delta << " " << v_delta << " " << rssi << std::endl;
		}
	}
	ofstr.close();
	fso->setHorizontalGMVal(h_gm_init);
	fso->setVerticalGMVal(v_gm_init);

	std::cout << "SFPAutoAligner::mapRun end" << std::endl;
}

float SFPAutoAligner::getRSSI(int h_gm, int v_gm) {
	// TODO confirm "good" RSSI values:
	// 		0) Log values so they can be inspected later.
	// 		1) Sleep for longer.
	// 		2) Read k values and return the first edge value.
	// 		3) Constantly read values in separate thread. Update a shared variable when changes.
	if(get_rssi_mode == GetRSSIMode::SLEEP) {
		LOG("getRSSI: start sleep");
		usleep(1000 * sleep_milliseconds);
		LOG("getRSSI: end sleep");
	}

	std::string get_rssi_msg = "X_out";
	if(args->sfp_test_server) {
		std::stringstream sstr;
		sstr << "get_rssi " << h_gm << " " << v_gm;

		get_rssi_msg = sstr.str();
	}

	float rssi= 0.0;

	if(get_rssi_mode == GetRSSIMode::SLEEP) {
		sendMsg(get_rssi_msg);

		std::string rssi_str;
		recvMsg(rssi_str);

		std::stringstream sstr(rssi_str);
		sstr >> rssi;
	} else if(get_rssi_mode == GetRSSIMode::MULTI) {
		bool first = true;
		float prev_rssi = 0.0;
		int max_num_changes = 10;
		int num_changes = 0;


		std::stringstream rcv_rssis;
		rcv_rssis << "received rssis: {";

		for(int i = 0; i < num_messages; i++) {
			prev_rssi = rssi;

			sendMsg(get_rssi_msg);

			std::string rssi_str;
			recvMsg(rssi_str);

			std::stringstream sstr(rssi_str);
			sstr >> rssi;

			if(i != 0) {
				rcv_rssis << ", ";
			}
			rcv_rssis << rssi;

			if(!first && prev_rssi != rssi) {
				num_changes++;
				if(num_changes >= max_num_changes) {
					break;
				}
			}

			if(first) {
				first = false;
			}
		}

		rcv_rssis << "}";
		LOG(rcv_rssis.str());
	}

	std::string rssi_str;
	recvMsg(rssi_str);

	std::stringstream sstr(rssi_str);
	sstr >> rssi;

	{
		std::stringstream sstr;
		sstr << "fetched RSSI is: " << rssi;
		LOG(sstr.str());
	}
	if(rssi <= args->sfp_rssi_zero_value) {
		rssi = 0.0;
	}


	return rssi;
}

void SFPAutoAligner::sendMsg(const std::string &msg) {
	if(sock_type == SockType::TCP) {
		send(sock, msg.c_str(), msg.size(), 0);
	} else if(sock_type == SockType::UDP) {

		{
			// Debugging print statements for sending and receiving messages
			char ip_str[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &foreign_host.sin_addr, ip_str, INET_ADDRSTRLEN);

			int port = ntohs(foreign_host.sin_port);

			std::stringstream sstr;
			sstr << "sending \"" << msg << "\" to " << ip_str << ":" << port;
			VLOG(sstr.str(), 2);
		}

		write(sock, msg.c_str(), msg.size());

		// sendto(sock, msg.c_str(), msg.size(), 0, (struct sockaddr*)&foreign_host, sizeof(foreign_host));
	} else {
		std::cerr << "Unknown SockType: " << sock_type << std::endl;
	}
}
	
void SFPAutoAligner::recvMsg(std::string &msg) {
	msg = "";
	if(sock_type == SockType::TCP) {
		int buf_len = 256;
		char buf[buf_len];

		bool recv_loop = true;
		while(recv_loop) {
			int recv_len = recv(sock, buf, buf_len - 1, 0);
			recv_loop = (recv_len == buf_len - 1 && buf[recv_len - 1] != '\0');
			buf[recv_len] = '\0';
			msg += buf;
		}
	} else if(sock_type == SockType::UDP) {
		struct sockaddr_in recv_src;
		socklen_t l = sizeof(recv_src);

		int buf_len = 256;
		char buf[buf_len];

		bool recv_loop = true;
		while(recv_loop) {
			int recv_len = recvfrom(sock, buf, buf_len - 1, 0, (struct sockaddr*)&recv_src, &l);
			
			// TODO check that recv_src == foreign_host

			recv_loop = (recv_len == buf_len - 1 && buf[recv_len - 1] != '\0');
			buf[recv_len] = '\0';
			msg += buf;
		}

		{
			// Debugging print statements for sending and receiving messages
			char ip_str[INET_ADDRSTRLEN];
			inet_ntop(AF_INET, &recv_src.sin_addr, ip_str, INET_ADDRSTRLEN);

			int port = ntohs(recv_src.sin_port);

			std::stringstream sstr;
			sstr << "received \"" << msg << "\" from " << ip_str << ":" << port;
			VLOG(sstr.str(),2);
		}
	} else {
		std::cerr << "Unknown SockType: " << sock_type << std::endl;
	}
}

SFPAutoAligner* SFPAutoAligner::connectTo(int send_port, SFPAutoAligner::SockType sock_type, const std::string &host_addr, const std::string &rack_id, const std::string &fso_id) {
	if (sock_type == SFPAutoAligner::SockType::TCP) {
		// Know addresses before
		struct addrinfo hints, *serv_info, *p;
		int sock;

		memset(&hints,0,sizeof(hints));
		hints.ai_family = AF_UNSPEC;
		hints.ai_socktype = SOCK_STREAM;

		std::string send_port_str = std::to_string(send_port);
		int rv = getaddrinfo(host_addr.c_str(),send_port_str.c_str(),&hints,&serv_info);
		if(rv != 0) {
			std::cerr << "Error in getaddrinfo: " << gai_strerror(rv) << std::endl;
			return NULL;
		}

		for(p = serv_info; p != NULL; p = p->ai_next) {
			// Make socket
			sock = socket(p->ai_family,p->ai_socktype,p->ai_protocol);
			if(sock == -1) {
				std::cerr << "Error with creating socket" << std::endl;
				continue;
			}
			rv = connect(sock,p->ai_addr,p->ai_addrlen);
			if(rv == -1) {
				close(sock);
				std::cerr << "Error when connecting" << std::endl;
				continue;
			}
			break;
		}

		if(p == NULL) {
			std::cerr << "Could not connect to foreign host" << std::endl;
			return NULL;
		}

		freeaddrinfo(serv_info);

		// Send something
		std::string msg = rack_id + " " + fso_id;
		send(sock,msg.c_str(),msg.size(),0);
		std::cout << "Sent: " << msg << std::endl;
		// Recv something
		char buf[100];
		int recv_len = recv(sock,buf,99,0);
		buf[recv_len] = '\0';
		std::string recv_msg = buf;
		std::cout << "Recv: " << recv_msg << std::endl;
		bool fso_match = (recv_msg == "accept");

		if(fso_match) {
			return new SFPAutoAligner(sock, SFPAutoAligner::SockType::TCP);
		} else {
			close(sock);
			return NULL;
		}
	} else if (sock_type == SFPAutoAligner::SockType::UDP) {
		std::cout << "Connecting via UDP: " << host_addr << ":" << send_port << std::endl;

		int sock = socket(AF_INET, SOCK_DGRAM, 0);
		struct sockaddr_in foreign_host;
		memset(&foreign_host, 0, sizeof(foreign_host));

		foreign_host.sin_family = AF_INET;
		foreign_host.sin_port = htons(send_port);
		foreign_host.sin_addr.s_addr = inet_addr(host_addr.c_str());

		connect(sock, (struct sockaddr*) &foreign_host, sizeof(foreign_host));

		// TODO create a simple init protocol

		SFPAutoAligner* saa = new SFPAutoAligner(sock, SFPAutoAligner::SockType::UDP);
		saa->setForeignSockAddr(foreign_host);

		std::cout << "Connected via UDP" << std::endl;
		return saa;
	} else {
		std::cerr << "Invalid SockType given: " << sock_type << std::endl;
		return nullptr;
	}
}