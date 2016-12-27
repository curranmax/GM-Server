
#include "auto_align.h"
#include "vec4.h"

#include <iostream>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <netdb.h>
#include <sstream>
#include <asm/types.h>
#include <fstream>
#include <math.h>
#include <vector>
#include <chrono>

#define GM_MIN 0x0
#define GM_MAX 0xffff

#define SSTR_REPLACE(sstr,v) { sstr.str(v); sstr.clear(); }

AutoAligner::AutoAligner(int sock_,bool is_controller_) {
	sock = sock_;
	is_controller = is_controller_;

	max_delta = 25;
	step = 25;
	delay = 0.0;
}

AutoAligner::~AutoAligner() {
	close(sock);
}

void AutoAligner::run(Args* args_,FSO* fso_) {
	std::cout << "START AutoAlign RUN" << std::endl;
	setValues(args_,fso_);
	if(is_controller) {
		controller_run();
	} else {
		other_run();
	}
	std::cout << "END AutoAlign RUN" << std::endl;
}

bool AutoAligner::get_fso(std::string &other_rack_id, std::string &other_fso_id) {
	std::string rmsg,token;

	send_msg("get_fso");
	recv_msg(rmsg);

	std::stringstream sstr(rmsg);
	sstr >> token;
	if(token != "fso_is") {
		return false;
	}

	other_rack_id = "";
	other_fso_id = "";

	sstr >> other_rack_id >> other_fso_id;
	return true;
}

bool AutoAligner::get_gm(int &other_gm1,int &other_gm2) {
	std::string rmsg,token;

	send_msg("get_gm");
	recv_msg(rmsg);
	
	std::stringstream sstr(rmsg);
	sstr >> token;
	if(token != "gm_is") {
		return false;
	}
	other_gm1 = -1;
	other_gm2 = -1;
	sstr >> other_gm1 >> other_gm2;
	return true;
}

bool AutoAligner::get_pwr(float &other_pwr) {
	std::string rmsg,token;

	send_msg("get_pwr");
	recv_msg(rmsg);

	std::stringstream sstr(rmsg);
	sstr >> token;
	if(token != "pwr_is") {
		return false;
	}

	other_pwr = 0.0;
	sstr >> other_pwr;
	return true;
}

bool AutoAligner::request_pwr() {
	send_msg("get_pwr");
	return true;
}

bool AutoAligner::get_requested_pwr(float &other_pwr) {
	std::string rmsg,token;

	recv_msg(rmsg);

	std::stringstream sstr(rmsg);
	sstr >> token;
	if(token != "pwr_is") {
		return false;
	}

	other_pwr = 0.0;
	sstr >> other_pwr;
	return true;
}

bool AutoAligner::give_fso(const std::string &this_rack_id,const std::string &this_fso_id) {
	std::string rmsg;

	send_msg("give_fso " + this_rack_id + " " + this_fso_id);
	recv_msg(rmsg);
	if(rmsg != "got_fso") {
		return false;
	}
	return true;
}

bool AutoAligner::set_gm(int gm1,int gm2) {
	std::string rmsg;

	send_msg("set_gm " + std::to_string(gm1) + " " + std::to_string(gm2));
	recv_msg(rmsg);
	return rmsg == "gm_set";
}

bool AutoAligner::save_gm() {
	std::string rmsg;

	send_msg("save_gm");
	recv_msg(rmsg);
	return rmsg == "gm_saved";
}

bool AutoAligner::quit() {
	if(save_gm()) {
		return false;
	}
	send_msg("quit");
	return true;
}

// Helper functions and definitions for controller_run
typedef std::pair<std::pair<int,int>,std::pair<int,int> > gm_settings;
typedef std::map<gm_settings,std::pair<float,float> > pwr_map;

gm_settings makeGMSettings(int v11,int v12,int v21,int v22) {
	return std::make_pair(std::make_pair(v11,v12),std::make_pair(v21,v22));
}

weight_type AutoAligner::calc_weight(const std::pair<float,float> &pwr_vals) {

	// returns pair (v1,v2) such that v1 <= v2
	float v1 = pwr_vals.first,v2 = pwr_vals.second;
	if(v1 > v2) {
		v1 = pwr_vals.second;
		v2 = pwr_vals.first;
	}
	return std::make_pair(v1,v2);
}

bool AutoAligner::better_weight(const weight_type &w1,const weight_type &w2) {
	return (w1.first > w2.first) || (w1.first == w2.first && w1.second > w2.second);
}

bool AutoAligner::same_weight(const weight_type &w1,const weight_type &w2) {
	return w1.first == w2.first && w1.second == w2.second;
}

void generateDirections(std::vector<Vec4> &dirs, int num_quad_div) {
	float theta_delta = 180.0 / (2 * (num_quad_div + 1));
	float eps = theta_delta * M_PI / 180.0 / 4.0;

	// Get directions to search
	dirs.clear();
	for(float t1 = 0.0; t1 < 360.0; t1 += theta_delta) {
		for(float t2 = 0.0; t2 < 360.0; t2 += theta_delta) {
			for(float t3 = 0.0; t3 < 360.0; t3 += theta_delta) {
				float rt1 = t1 * M_PI / 180.0;
				float rt2 = t2 * M_PI / 180.0;
				float rt3 = t3 * M_PI / 180.0;

				float x = cos(rt1) * cos(rt2) * cos(rt3);
				float y = sin(rt1) * cos(rt2) * cos(rt3);
				float z = sin(rt2) * cos(rt3);
				float w = sin(rt3);

				Vec4 this_dir(x,y,z,w);

				bool found = false;
				for(unsigned int i = 0 ; i < dirs.size(); ++i) {
					if(dirs[i].angle(this_dir) <= eps) {
						found = true;
						break;
					}
				}

				if(!found) {
					dirs.push_back(this_dir);
				}
			}
		}
	}

	dirs.push_back(Vec4(0,0,0,0));
}

void AutoAligner::simple_search(int &ct1, int &ct2, int &co1, int &co2, int max_iters) {
	int num_quad_div = 0;
	std::vector<Vec4> dirs;
	generateDirections(dirs, num_quad_div);

	int search_radius = 1;
	unsigned int num_settings_to_be_flat = 4;
	float this_pwr, other_pwr;
	
	int num_iter = 0;
	bool peak_found = false;
	weight_type peak_weight;

	while(max_iters < 0 || num_iter < max_iters) {
		num_iter++;
		if(args->verbose) {
			std::cout << "Starting Iteration " << num_iter << std::endl;
		}

		// Perform local search
		pwr_map local_map;
		for(unsigned int i = 0; i < dirs.size(); ++i) {
			int dt1 = dirs[i].xv() * search_radius;
			int dt2 = dirs[i].yv() * search_radius;
			int do1 = dirs[i].zv() * search_radius;
			int do2 = dirs[i].wv() * search_radius;

			// Could check if power already measured at this location and possibly skip it?

			set_gm(co1 + do1, co2 + do2);
			fso->setGM1Val(ct1 + dt1);
			fso->setGM2Val(ct2 + dt2);

			request_pwr();
			this_pwr = fso->getPower();
			get_requested_pwr(other_pwr);

			gm_settings this_setting = makeGMSettings(ct1 + dt1, ct2 + dt2, co1 + do1, co2 + do2);
			local_map[this_setting] = std::make_pair(this_pwr,other_pwr);

			if(args->verbose) {
				std::cout << "Trying direction " << i + 1 << " of " << dirs.size() << ": (" << ct1 + dt1 << ", " << ct2 + dt2 << ", " << co1 + do1 << ", " << co2 + do2 << ") -> (" << this_pwr << ", " << other_pwr << ")" << std::endl;
			}
		}

		// Find best power
		std::vector<gm_settings> max_settings;
		weight_type max_weight, this_weight;
		bool local_first = true;
		for(pwr_map::iterator itr = local_map.begin(); itr != local_map.end(); ++itr) {
			this_weight = calc_weight(itr->second);
			if(local_first) {
				max_settings.push_back(itr->first);
				max_weight = this_weight;
				local_first = false;
			} else {
				if(better_weight(this_weight, max_weight)) {
					max_settings.clear();
					max_weight = this_weight;
				}
				if(same_weight(this_weight, max_weight)) {
					max_settings.push_back(itr->first);
				}
			}
		}

		// Print out max_settings and max_weight
		if(args->verbose) {
			std::cout << "Center: (" << ct1 << ", " << ct2 << ", " << co1 << ", " << co2 << ")" << std::endl;
			std::cout << "Max Weight: (" << max_weight.first << ", " << max_weight.second << ")" << std::endl;
			for(unsigned int i = 0 ; i < max_settings.size(); ++i) {
				std::cout << "Max Setting: (" << max_settings[i].first.first << ", " << max_settings[i].first.second << ", " << max_settings[i].second.first << ", " << max_settings[i].second.second << ")" << std::endl;
			}
		}

		// Decide what to do
		// Flat
		if(max_settings.size() >= num_settings_to_be_flat) {
			// Double search_radius, and keep center same
			if(!peak_found) { // Once a peak is found, search radius should never increase
				search_radius *= 2;

				if(args->verbose) {
					std::cout << "Flat!\tSearch radius is now: " << search_radius << std::endl;
				}
				continue;
			} else {
				// Halve search radius if larger than 1, else end looping
				if(search_radius >= 2) {
					search_radius /= 2;
					if(args->verbose) {
						std::cout << "Flat and Peak!\tSearch radius is now: " << search_radius << std::endl;
					}
					continue;
				} else {
					if(args->verbose) {
						std::cout << "Flat and Peak!\tSearch stopped" << std::endl;
					}
					break;
				}
			}
		}

		// // If a new peak is found then go back to searching as normal
		// if(peak_found && better_weight(max_weight, peak_weight)) {
		// 	peak_found = false;
		// }

		// Peak

		// The initial peak is found when the current center has the highest power
		bool init_peak = (!peak_found && max_settings.size() == 1 && max_settings[0].first.first == ct1 && max_settings[0].first.second == ct2 && max_settings[0].second.first == co1 && max_settings[0].second.second == co2);
		// After the initial peak is found, we relax the constraints, and count the center as a peak as long as it is tied for the highest power
		bool next_peak = false;
		if(peak_found) {
			for(unsigned int i = 0; i < max_settings.size(); ++i) {
				if(max_settings[i].first.first == ct1 && max_settings[i].first.second == ct2 && max_settings[i].second.first == co1 && max_settings[i].second.second == co2) {
					next_peak = true;
					break;
				}
			}
		}

		if(init_peak || next_peak) {
			peak_found = true;
			peak_weight = max_weight;
			if(search_radius >= 2) {
				search_radius /= 2;
				if(args->verbose) {
					std::cout << "Peak!\tSearch radius is now: " << search_radius << std::endl; 
				}
				continue;
			} else {
				if(args->verbose) {
					std::cout << "Max Peak!\tSearch stopped" << std::endl; 
				}
				break;
			}
		}

		// Else move center
		float sum_t1 = 0, sum_t2 = 0, sum_o1 = 0, sum_o2 = 0;
		for(unsigned int i = 0; i < max_settings.size(); ++i) {
			sum_t1 += max_settings[i].first.first;
			sum_t2 += max_settings[i].first.second;
			sum_o1 += max_settings[i].second.first;
			sum_o2 += max_settings[i].second.second;
		}

		int new_ct1 = (int)round(sum_t1 / float(max_settings.size()));
		int new_ct2 = (int)round(sum_t2 / float(max_settings.size()));
		int new_co1 = (int)round(sum_o1 / float(max_settings.size()));
		int new_co2 = (int)round(sum_o2 / float(max_settings.size()));

		if(args->verbose) {
			std::cout << "Center moved to (" << new_ct1 << ", " << new_ct2 << ", " << new_co1 << ", " << new_co2 << ") in dir (" << new_ct1 - ct1 << ", " << new_ct2 - ct2 << ", " << new_co1 - co1 << ", " << new_co2 - co2 << ")" << std::endl;
		}

		ct1 = new_ct1;
		ct2 = new_ct2;
		co1 = new_co1;
		co2 = new_co2;
	}
}

void random_starting_point(gm_settings &init_peak, int &ct1, int &ct2, int &co1, int &co2, float min_search_radius, float max_search_radius) {
	float x1, y1, x2, y2;
	for(int i = 0; i < 100; ++i) {
		x1 = (float(rand()) / float(RAND_MAX) * 2.0) - 1.0;
		y1 = (float(rand()) / float(RAND_MAX) * 2.0) - 1.0;
		x2 = (float(rand()) / float(RAND_MAX) * 2.0) - 1.0;
		y2 = (float(rand()) / float(RAND_MAX) * 2.0) - 1.0;

		if(sqrt(x1 * x1 + y1 * y1 + x2 * x2 + y2 * y2) <= 1.0) {
			break;
		}
	}

	float search_radius = float(rand()) / float(RAND_MAX) * (max_search_radius - min_search_radius) + min_search_radius;

	ct1 = init_peak.first.first + int(x1 * search_radius);
	ct2 = init_peak.first.second + int(y1 * search_radius);
	co1 = init_peak.second.first + int(x2 * search_radius);
	co2 = init_peak.second.second + int(y2 * search_radius);
}

void AutoAligner::controller_run() {
	std::string other_rack_id = "", other_fso_id = "";

	// Set FSOs to the correct link
	get_fso(other_rack_id,other_fso_id);
	fso->setToLink(other_rack_id,other_fso_id);
	fso->startAutoAlign();

	give_fso(fso->getRackID(),fso->getFSOID());

	int ct1, ct2, co1, co2;

	get_gm(co1, co2);
	ct1 = fso->getGM1Val();
	ct2 = fso->getGM2Val();

	std::vector<std::chrono::time_point<std::chrono::system_clock>> ts;
	if(args->auto_algo == "gradient_descent_sphere") {
		simple_search(ct1, ct2, co1, co2, 50);

		if(args->verbose) {
			std::cout << "New center: (" << ct1 << ", " << ct2 << ", " << co1 << ", " << co2 << ")" <<std::endl;
		}
	}
	if(args->auto_algo == "multiple_test") {
		// First Search
		float this_pwr, other_pwr;
		gm_settings init_peak;
		simple_search(ct1, ct2, co1, co2, 50);

		// Get power at this point
		set_gm(co1, co2);
		fso->setGM1Val(ct1);
		fso->setGM2Val(ct2);

		request_pwr();
		this_pwr = fso->getPower();
		get_requested_pwr(other_pwr);
		std::pair<float, float> init_pwr = std::make_pair(this_pwr, other_pwr);

		init_peak = makeGMSettings(ct1, ct2, co1, co2);

		// Start from random starting points and see where we get back to
		std::vector<gm_settings> other_peaks;
		std::vector<std::pair<float, float> > other_powers;
		int num_tests = args->num_peak_tests;
		float min_search_radius = args->random_min_val, max_search_radius = args->random_max_val;
		for(int i = 0; i < num_tests; ++i) {
			random_starting_point(init_peak, ct1, ct2, co1, co2, min_search_radius, max_search_radius);

			simple_search(ct1, ct2, co1, co2, 50);
			other_peaks.push_back(makeGMSettings(ct1, ct2, co1, co2));

			set_gm(co1, co2);
			fso->setGM1Val(ct1);
			fso->setGM2Val(ct2);

			request_pwr();
			this_pwr = fso->getPower();
			get_requested_pwr(other_pwr);

			other_powers.push_back(std::make_pair(this_pwr, other_pwr));
		}

		if(args->peak_file == "") {
			std::cout << "Init Peak: (" << init_peak.first.first << ", " << init_peak.first.second << ", " << init_peak.second.first << ", " << init_peak.second.second << ") -> (" << init_pwr.first << ", " << init_pwr.second << ")" << std::endl;
			for(unsigned int i = 0; i < other_peaks.size(); ++i) {
				std::cout << "Other Peak Dif: (" << other_peaks[i].first.first - init_peak.first.first << ", " << other_peaks[i].first.second - init_peak.first.second << ", " << other_peaks[i].second.first - init_peak.second.first << ", " << other_peaks[i].second.second - init_peak.second.second << ") -> (" << other_powers[i].first << ", " << other_powers[i].second << ")" << std::endl;
			}
		} else {
			std::ofstream ofstr(args->peak_file, std::ofstream::out | std::ofstream::app);
			ofstr << "Init Peak: (" << init_peak.first.first << ", " << init_peak.first.second << ", " << init_peak.second.first << ", " << init_peak.second.second << ") -> (" << init_pwr.first << ", " << init_pwr.second << ")" << std::endl;
			for(unsigned int i = 0; i < other_peaks.size(); ++i) {
				ofstr << "Other Peak Dif: (" << other_peaks[i].first.first - init_peak.first.first << ", " << other_peaks[i].first.second - init_peak.first.second << ", " << other_peaks[i].second.first - init_peak.second.first << ", " << other_peaks[i].second.second - init_peak.second.second << ") -> (" << other_powers[i].first << ", " << other_powers[i].second << ")" << std::endl;
			}
			ofstr << std::endl;
		}
	}
	if(args->auto_algo == "measure_search_space") {
		simple_search(ct1, ct2, co1, co2, 50);

		int num_quad_div = 0;
		std::vector<Vec4> dirs;
		std::vector<std::vector<std::pair<int,std::pair<float, float> > > > power_values;
		generateDirections(dirs, num_quad_div);

		int step_size = 25;
		float this_pwr = -50.0, other_pwr = -50.0;

		for(unsigned int i = 0; i < dirs.size(); ++i) {
			int displacement = 0;
			std::vector<std::pair<int,std::pair<float, float> > > temp;
			
			if(dirs[i].mag() > .0001) {
				while(true) {
					// Set FSO Location
					set_gm(co1 + int(displacement * dirs[i].xv()), co2 + int(displacement * dirs[i].yv()));
					fso->setGM1Val(ct1 + int(displacement * dirs[i].zv()));
					fso->setGM2Val(ct2 + int(displacement * dirs[i].wv()));

					// Measure power
					request_pwr();
					this_pwr = fso->getPower();
					get_requested_pwr(other_pwr);

					// Add data to vector
					temp.push_back(std::make_pair(displacement, std::make_pair(this_pwr, other_pwr)));
					std::cout << "Dir (" << dirs[i].xv() << ", " << dirs[i].yv() << ", " << dirs[i].zv() << ", " << dirs[i].wv() << ") ";
					std::cout << "\tDisp " << displacement << "  \tPower (" << this_pwr << "," << other_pwr << ")" << std::endl;

					// Check if above 40
					if(this_pwr <= -39.9 && other_pwr <= -39.9) {
						break;
					}

					// Update displacement
					displacement += step_size;
				}
			}
			power_values.push_back(temp);
		}

		std::ofstream ofstr(args->peak_file, std::ofstream::out | std::ofstream::app);
		ofstr << "Center (" << ct1 << ", " << ct2 << ", " << co1 << ", " << co2 << ")" << std::endl;
		for(unsigned int i = 0; i < dirs.size(); ++i) {
			ofstr << "Dir;(" << dirs[i].xv() << ", " << dirs[i].yv() << ", " << dirs[i].zv() << ", " << dirs[i].wv() << ")";
			for(unsigned int j = 0; j < power_values[i].size(); ++j) {
				ofstr << ";(" << power_values[i][j].first << ", " << power_values[i][j].second.first << ", " << power_values[i][j].second.second << ")";
			}
			ofstr << std::endl;
		}
	}
	if(args->auto_algo == "test_time") {
		int num_tests = 100;
		std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();
		for(int i = 0; i < num_tests; ++i) {
			fso->getPower();
		}
		std::chrono::time_point<std::chrono::system_clock> end = std::chrono::system_clock::now();

		std::chrono::duration<double> dur = end - start;

		std::cout << "Total Duration " << dur.count() << std::endl << "Avg Duration " << dur.count() / num_tests << std::endl;
	}
	quit();
	fso->endAutoAlign();
}

void AutoAligner::other_run() {
	// respond to messages: request for power, request to try new gm settings, finalize gm settings
	std::string msg;
	std::string token;

	float pwr;
	std::string return_msg, other_rack_id = "", other_fso_id = "";
	fso->startAutoAlign();
	while(true) {
		recv_msg(msg);
		std::stringstream sstr(msg);
		return_msg = "";

		sstr >> token;
		if(token == "end" || token == "quit") {
			break;
		} else if(token == "get_pwr") {
			// Fetch power from DOM and then send it back
			pwr = fso->getPower();
			return_msg = "pwr_is " + std::to_string(pwr);
		} else if(token == "get_gm") {
			// Fetch current gm settings
			return_msg = "gm_is " + std::to_string(fso->getGM1Val()) + " " + std::to_string(fso->getGM2Val());
		} else if(token == "get_fso") {
			// Reply with FSOs rack_id and fso_id
			return_msg = "fso_is " + fso->getRackID() + " " + fso->getFSOID();
		} else if(token == "give_fso") {
			// Gives other FSOs rack_id and fso_id
			sstr >> other_rack_id >> other_fso_id;
			fso->setToLink(other_rack_id,other_fso_id);
			return_msg = "got_fso";
		} else if(token == "set_gm") {
			// Set GM to the two values supplied in the message
			int gm1 = -1,gm2 = -1;
			sstr >> gm1 >> gm2;

			fso->setGM1Val(gm1);
			fso->setGM2Val(gm2);
			return_msg = "gm_set";
		} else if(token == "save_gm") {
			// Save current settings to link settings
			return_msg = "gm_saved_butnotactuallycauseitsnotimplementedyet";
		}
		send_msg(return_msg);
	}
	fso->endAutoAlign();
}

void AutoAligner::send_msg(const std::string &msg) {
	send(sock,msg.c_str(),msg.size(),0);
	// if(args != NULL && args->verbose) {
	// 	std::cout << "Sent: " << msg << std::endl;
	// }
}

void AutoAligner::recv_msg(std::string &msg) {
	msg = "";
	int buf_len = 64;
	char buf[buf_len];

	bool loop = true;
	while(loop) {
		int recv_len = recv(sock,buf,buf_len - 1,0);
		loop = (recv_len == buf_len -1 && buf[recv_len - 1] != '\0');
		buf[recv_len] = '\0';
		msg += buf;
	}
	// if(args != NULL && args->verbose) {
	// 	std::cout << "Recv: " << msg << std::endl;
	// }
}

AutoAligner* listenFor(int listen_port,const std::string &rack_id,const std::string &fso_id) {
	struct addrinfo hints, *serv_info, *p;
	int sock,one = 1,backlog = 10;

	memset(&hints,0,sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;
	hints.ai_flags = AI_PASSIVE;

	std::string listen_port_str = std::to_string(listen_port);
	int rv = getaddrinfo(NULL,listen_port_str.c_str(),&hints,&serv_info);
	if(rv != 0) {
		std::cerr << "Error in getaddrinfo: " << gai_strerror(rv) << std::endl;
		return NULL;
	}

	for(p = serv_info; p != NULL; p = p->ai_next) {
		sock = socket(p->ai_family,p->ai_socktype,p->ai_protocol);
		if(sock == -1) {
			std::cerr << "Error in creating socket" << std::endl;
			continue;
		}

		rv = setsockopt(sock,SOL_SOCKET,SO_REUSEADDR,&one,sizeof(int));
		if(rv == -1) {
			std::cerr << "Error in setting socket options" << std::endl;
			return NULL;
		}

		rv = bind(sock,p->ai_addr,p->ai_addrlen);
		if(rv == -1) {
			std::cerr << "Couldn't bind socket" << std::endl;
			continue;
		}
		break;
	}

	freeaddrinfo(serv_info);
	if(p == NULL) {
		std::cerr << "Coudln't bind any socket" << std::endl;
		return NULL;
	}

	rv = listen(sock,backlog);

	struct sockaddr_storage foreign_addr;
	socklen_t sas_size = sizeof(foreign_addr);
	int connect_sock = accept(sock,(struct sockaddr *)&foreign_addr,&sas_size);
	if(connect_sock == -1) {
		std::cerr << "Error in accepting connection" << std::endl;
		return NULL;
	}

	// Recv
	char buf[100];
	int recv_len = recv(connect_sock,buf,99,0);
	buf[recv_len] = '\0';
	std::string recv_msg = buf;
	std::cout << "Recv: " << recv_msg << std::endl;
	bool fso_match = (recv_msg == rack_id + " " + fso_id);

	// Send
	std::string msg = "deny";
	if(fso_match) {
		msg = "accept";
	}
	send(connect_sock,msg.c_str(),msg.size(),0);
	std::cout << "Sent: " << msg << std::endl;

	close(sock);
	if(fso_match) {
		return new AutoAligner(connect_sock,false);
	} else {
		close(connect_sock);
		return NULL;
	}
}

AutoAligner* connectTo(int send_port,const std::string &host_addr,const std::string &rack_id,const std::string &fso_id) {
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
		return new AutoAligner(sock,true);
	} else {
		close(sock);
		return NULL;
	}
}
