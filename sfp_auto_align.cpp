
#include "sfp_auto_align.h"

#include "logger.h"
#include "tracking_analysis.h"

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
#include <list>
#include <cmath>

float RSSITuple::squaredEuclideanDistance(const RSSITuple& other_tuple) const {
	if(this->size() != other_tuple.size()) {
		return -1.0;
	}
	float sum_dist = 0.0;
	for(int i = 0; i < this->size(); ++i) {
		sum_dist += pow(rssis[i] - other_tuple.rssis[i], 2.0);
	}
	return sum_dist;
}

SFPAutoAligner::SFPAutoAligner(int sock_, SockType sock_type_) {
	sock = sock_;
	sock_type = sock_type_;

	get_rssi_mode = GetRSSIMode::SLEEP;
	sleep_milliseconds = 12;
	max_num_messages = 1;
	max_num_changes = 1;
	num_message_average = 1;
}

SFPAutoAligner::~SFPAutoAligner() {
	close(sock);
}

void SFPAutoAligner::run(Args* args_, FSO* fso_, const std::string &other_rack_id, const std::string &other_fso_id) {
	setValues(args_, fso_);

	setMultiParam(args->sfp_max_num_messages, args->sfp_max_num_changes, args->sfp_num_message_average);

	fso->setToLink(other_rack_id, other_fso_id);

	bool run_switch = false;

	if(args->sfp_map_power) {
		if(args->sfp_search_delta > 0) {
			mapRunWithSearch();
		} else {
			mapRun();
		}
	} else if(run_switch) {
		switchRun();
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

void SFPAutoAligner::fillSearchLocs(std::vector<GMVal>* search_locs, int num_search_locs, int search_delta) {
	if(search_delta <= 0) {
		std::cerr << "Invalid value for search_delta: " << search_delta << std::endl;
		exit(0);
	}

	if(num_search_locs == 3) {
		search_locs->push_back(GMVal(search_delta, 0));
		search_locs->push_back(GMVal(0, search_delta));
	} else if(num_search_locs == 5) {
		search_locs->push_back(GMVal(search_delta, 0));
		search_locs->push_back(GMVal(0, search_delta));
		search_locs->push_back(GMVal(-search_delta, 0));
		search_locs->push_back(GMVal(0, -search_delta));
	} else {
		std::cerr << "Invalid value for num_search_locs: " << num_search_locs << std::endl;
		exit(0);
	}
}

void SFPAutoAligner::controllerRun() {
	LOG("controllerRun start");

	// Creates the search locations to use.
	std::vector<GMVal> search_locs;
	fillSearchLocs(&search_locs, args->sfp_num_search_locs, args->sfp_search_delta);

	{
		// Logs search locations used.
		std::stringstream sstr;
		for(unsigned int i = 0; i < search_locs.size(); ++i) {
			sstr << "(" << search_locs[i].h_gm << ", " << search_locs[i].v_gm << ")";

			if(i < search_locs.size() - 1) {
				sstr << ", ";
			}
		}

		LOG("search locs are: {" + sstr.str() + "}");
	}

	// Build Table
	std::string in_file = args->sfp_map_in_file;
	std::ifstream ifstr(in_file, std::ifstream::in);
	RSSIMap rssi_map;
	RSSITupleMap rssi_tuple_map;

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
			rssi_map[GMVal(hd, vd)] = rssi;

			{
				std::stringstream sstr;
				sstr << "added entry to rssi_map: (" << hd << ", " << vd << ") -> " << rssi;
				VLOG(sstr.str(), 1);
			}
		}

		// Creates the rssi_tuple_map.
		for(RSSIMap::const_iterator table_itr = rssi_map.cbegin(); table_itr != rssi_map.cend(); ++table_itr) {
			rssi_tuple_map[table_itr->first].addValue(table_itr->second);
			for(std::vector<GMVal>::const_iterator search_itr = search_locs.cbegin(); search_itr != search_locs.cend(); ++search_itr) {
				GMVal lookup_loc = GMVal(table_itr->first.h_gm + search_itr->h_gm, table_itr->first.v_gm + search_itr->v_gm);

				float lookup_rssi = 0.0;
				if(rssi_map.count(lookup_loc) > 0) {
					lookup_rssi = rssi_map.at(lookup_loc);
				}

				rssi_tuple_map[table_itr->first].addValue(lookup_rssi);
			}
		}

		// Check that all entries in rssi_tuple_map have a length of (search_locs.size() + 1)
		for(RSSITupleMap::const_iterator table_itr = rssi_tuple_map.cbegin(); table_itr != rssi_tuple_map.cend(); ++table_itr) {
			if(table_itr->second.size() != (int(search_locs.size()) + 1)) {
				std::cerr << "Invalid tuple size: " << table_itr->second.size() << std::endl;
				return;
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

	float k_proportional = args->k_proportional;
	float k_integral = args->k_integral;
	float k_derivative = args->k_derivative;

	bool tracking = false;
	int h_gm, v_gm;

	int sum_h_err = 0, sum_v_err = 0, prev_h_err = 0, prev_v_err = 0;

	int num_iters = 0;

	// Tracking History
	int history_size = 100;
	std::list<GMVal> tracking_history;

	// Tracking Analysis;
	TrackingAnalysis regular_tracking, history_tracking;

	// Timing tests
	std::vector<float> get_rssi_times;
	std::chrono::time_point<std::chrono::system_clock> start_get_rssi;
	std::chrono::time_point<std::chrono::system_clock> end_get_rssi;
	std::chrono::duration<double> dur_get_rssi;

	std::vector<float> set_gm_times;
	std::chrono::time_point<std::chrono::system_clock> start_set_gm;
	std::chrono::time_point<std::chrono::system_clock> end_set_gm;
	std::chrono::duration<double> dur_set_gm;

	std::vector<float> find_error_times;
	std::chrono::time_point<std::chrono::system_clock> start_find_error;
	std::chrono::time_point<std::chrono::system_clock> end_find_error;
	std::chrono::duration<double> dur_find_error;

	std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();
	while(sfp_controller_loop) {
		LOG("---------start tracking iter---------");

		num_iters += 1;

		h_gm = fso->getHorizontalGMVal();
		v_gm = fso->getVerticalGMVal();

		// Get current power
		start_get_rssi = std::chrono::system_clock::now();
		float this_rssi = getRSSI(h_gm, v_gm);
		end_get_rssi = std::chrono::system_clock::now();
		dur_get_rssi = end_get_rssi - start_get_rssi;
		get_rssi_times.push_back(dur_get_rssi.count());

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
			RSSITuple this_tuple;
			this_tuple.addValue(this_rssi);
			RSSIMap search_rssis;
			// Query the necessary points
			for(unsigned int i = 0; i < search_locs.size(); ++i) {
				start_set_gm = std::chrono::system_clock::now();
				fso->setHorizontalGMVal(search_locs[i].h_gm + h_gm);
				fso->setVerticalGMVal(search_locs[i].v_gm + v_gm);
				end_set_gm = std::chrono::system_clock::now();
				dur_set_gm = end_set_gm - start_set_gm;
				set_gm_times.push_back(dur_set_gm.count());

				{
					std::stringstream sstr;
					sstr << "search: GM set to (" << search_locs[i].h_gm + h_gm << "[" << search_locs[i].h_gm << "], " << search_locs[i].v_gm + v_gm << "[" << search_locs[i].v_gm << "])";
					LOG(sstr.str());
				}

				start_get_rssi = std::chrono::system_clock::now();
				float search_rssi = getRSSI(search_locs[i].h_gm + h_gm, search_locs[i].v_gm + v_gm);
				end_get_rssi = std::chrono::system_clock::now();
				dur_get_rssi = end_get_rssi - start_get_rssi;
				get_rssi_times.push_back(dur_get_rssi.count());

				{
					std::stringstream sstr;
					sstr << "search: GM at (" << search_locs[i].h_gm + h_gm << "[" << search_locs[i].h_gm << "], " << search_locs[i].v_gm + v_gm << "[" << search_locs[i].v_gm << "]) with RSSI of " << search_rssi;
					LOG(sstr.str());
				}

				this_tuple.addValue(search_rssi);
				search_rssis[search_locs[i]] = search_rssi;
			}

			// Search the table for the correction
			int h_err, v_err;
			float hr, vr;
			if(this_tuple.allZero() || this_rssi == 1) {
				h_err = 0;
				v_err = 0;
				hr = 0.0;
				vr = 0.0;

				{
					std::stringstream sstr;
					sstr << "response is (" << hr << ", " << vr << ")";
					LOG(sstr.str());
				}
			} else {
				start_find_error = std::chrono::system_clock::now();
				findError(this_tuple, rssi_tuple_map, h_err, v_err);
	
				end_find_error = std::chrono::system_clock::now();
				dur_find_error = end_find_error - start_find_error;
				find_error_times.push_back(dur_find_error.count());

				{
					std::stringstream sstr;
					sstr << "computed error is (" << h_err << ", " << v_err << ")"; 
					LOG(sstr.str());
				}
	
				sum_h_err += h_err;
				sum_v_err += v_err;
	
				hr = k_proportional * -h_err + k_integral * -sum_h_err + k_derivative * (prev_h_err - h_err);
				vr = k_proportional * -v_err + k_integral * -sum_v_err + k_derivative * (prev_v_err - v_err);

				{
					std::stringstream sstr;
					sstr << "response is (" << hr << ", " << vr << "), detail are [" << k_proportional << " * -(" << h_err <<  ") + " << k_integral << " * -(" << sum_h_err << ") + " << k_derivative << " * (" << prev_h_err << " - " << h_err << "), " << k_proportional << " * -(" << v_err <<  ") + " << k_integral << " * -(" << sum_v_err << ") + " << k_derivative << " * (" << prev_v_err << " - " << v_err << ") ]";
					LOG(sstr.str());
				}
			}

			prev_h_err = h_err;
			prev_v_err = v_err;

			// Add to the history
			tracking_history.push_front(GMVal(hr, vr));
			while(int(tracking_history.size()) > history_size) {
				tracking_history.pop_back();
			}

			// Calculate response including history information
			float history_avg_hr = 0.0;
			float history_avg_vr = 0.0;
			float sum_weight = 0.0;
			float x = 0.0;
			for(std::list<GMVal>::iterator itr = tracking_history.begin(); itr != tracking_history.end(); ++itr) {
				float weight = 1.0 / (x + 1.0);

				history_avg_hr += itr->h_gm * weight;
				history_avg_vr += itr->v_gm * weight;
				sum_weight += weight;
				x += 1.0;
			}
			history_avg_hr /= sum_weight;
			history_avg_vr /= sum_weight;

			start_set_gm = std::chrono::system_clock::now();
			fso->setHorizontalGMVal(h_gm + round(history_avg_hr));
			fso->setVerticalGMVal(v_gm + round(history_avg_vr));
			end_set_gm = std::chrono::system_clock::now();
			dur_set_gm = end_set_gm - start_set_gm;
			set_gm_times.push_back(dur_set_gm.count());

			{
				std::stringstream sstr;
				sstr << "GM set to (" << h_gm + round(history_avg_hr) << ", " << v_gm + round(history_avg_vr) << ")";
				LOG(sstr.str());
			}

			std::cout << ", tracking moved (" << hr << ", " << vr << ") [history response (" << round(history_avg_hr) << ", " << round(history_avg_vr) << ")]" << std::endl;

			regular_tracking.addVal(GMVal(hr, vr));
			history_tracking.addVal(GMVal(round(history_avg_hr), round(history_avg_vr)));
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

	{
		// TODO log the rest of the timinig information.
		std::stringstream sstr;
		sstr << "Avg Iteration time: " << seconds_per_iter * 1000.0 << " milliseconds per iter";
		LOG(sstr.str());
	}

	// Get RSSI
	float sum_get_rssi_times = 0.0;
	for(unsigned int i = 0; i < get_rssi_times.size(); ++i) {
		sum_get_rssi_times += get_rssi_times[i];
	}
	std::cout << "Average getRSSI() time: " << sum_get_rssi_times / float(get_rssi_times.size()) * 1000.0 << " milliseconds per function call" << std::endl;
	std::cout << "Avarege getRSSI() per iter: " << float(get_rssi_times.size()) / float(num_iters) << " getRSSI() per iter" << std::endl;

	// Set GM
	float sum_set_gm_times = 0.0;
	for(unsigned int i = 0; i < set_gm_times.size(); ++i) {
		sum_set_gm_times += set_gm_times[i];
	}
	std::cout << "Average setGM() time: " << sum_set_gm_times / float(set_gm_times.size()) * 1000.0 << " milliseconds per function call" << std::endl;
	std::cout << "Avarege setGM() per iter: " << float(set_gm_times.size()) / float(num_iters) << " setGM() per iter" << std::endl;

	// findError
	float sum_find_error_times = 0.0;
	for(unsigned int i = 0; i < find_error_times.size(); ++i) {
		sum_find_error_times += find_error_times[i];
	}
	std::cout << "Average findError() time: " << sum_find_error_times / float(find_error_times.size()) * 1000.0 << " milliseconds per function call" << std::endl;
	std::cout << "Avarege findError() per iter: " << float(find_error_times.size()) / float(num_iters) << " findError() per iter" << std::endl;

	std::cout << "Average iter change [regular]: " << regular_tracking.avgIterChange() << std::endl;
	std::cout << "Average iter change [history]: " << history_tracking.avgIterChange() << std::endl;

	LOG("controllerRun end");
}

void SFPAutoAligner::findError(const RSSITuple& this_tuple, const RSSITupleMap& rssi_map, int &h_err, int &v_err) {
	bool first = true;
	float best_diff = 0.0;
	float diff_epsilon = args->sfp_table_epsilon;
	float best_epsilon = 0.01;

	{
		std::stringstream sstr;
		sstr << "start findError2: <";
		for(int i = 0; i < this_tuple.size(); ++i) {
			if(i > 0) {
				sstr << ", ";
			}
			sstr << this_tuple.rssis[i];
		}
		sstr << ">";
		LOG(sstr.str());
	}

	for(RSSITupleMap::const_iterator table_itr = rssi_map.cbegin(); table_itr != rssi_map.cend(); ++table_itr) {
		float this_diff = this_tuple.squaredEuclideanDistance(table_itr->second);

		if(this_diff <= diff_epsilon) {
			this_diff = 0.0;
		}

		if(first) {
			first = false;
			h_err = table_itr->first.h_gm;
			v_err = table_itr->first.v_gm;

			best_diff = this_diff;
		} else {
			if(this_diff < best_diff ||
					(fabs(this_diff - best_diff) <= best_epsilon &&
						(pow(table_itr->first.h_gm, 2) + pow(table_itr->first.v_gm, 2) < pow(h_err, 2) + pow(v_err, 2)))) {
				h_err = table_itr->first.h_gm;
				v_err = table_itr->first.v_gm;

				if(this_diff < best_diff) {
					best_diff = this_diff;
				}
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

void SFPAutoAligner::mapRunWithSearch() {
	std::cout << "SFPAutoAligner::mapRunWithSearch start" << std::endl;

	// Gets the search locs to use
	std::vector<GMVal> search_locs;
	fillSearchLocs(&search_locs, args->sfp_num_search_locs, args->sfp_search_delta);

	int h_gm_init = fso->getHorizontalGMVal();
	int v_gm_init = fso->getVerticalGMVal();

	int map_range = args->sfp_map_range;
	int map_step = args->sfp_map_step;
	
	std::string out_file = args->sfp_map_out_file;
	std::ofstream ofstr(out_file, std::ofstream::out);

	ofstr << "PARAMS map_range=" << map_range << " map_step=" << map_step << " with_search=true" << std::endl;  
	ofstr << "H_Delta V_Delta RSSI[0,0]";

	for(unsigned int i = 0; i < search_locs.size(); ++i) {
		ofstr << " RSSI[" << search_locs[i].h_gm << "," << search_locs[i].v_gm << "]";
	}
	ofstr << std::endl;

	for(int h_delta = -map_range; h_delta <= map_range; h_delta += map_step) {
		for(int v_delta = -map_range; v_delta <= map_range; v_delta += map_step) {
			fso->setHorizontalGMVal(h_delta + h_gm_init);
			fso->setVerticalGMVal(v_delta + v_gm_init);

			std::cout << "GM:(" << h_delta << ", " << v_delta << ")";
			
			// Get RSSI
			float rssi = getRSSI(h_delta + h_gm_init, v_delta + v_gm_init);

			std::cout << "\t --> RSSI = " << rssi << std::endl;

			// Save it
			ofstr << h_delta << " " << v_delta << " " << rssi;

			for(unsigned int i = 0; i < search_locs.size(); ++i) {
				fso->setHorizontalGMVal(search_locs[i].h_gm + h_delta + h_gm_init);
				fso->setVerticalGMVal(search_locs[i].v_gm + v_delta + v_gm_init);

				float search_rssi = getRSSI(search_locs[i].h_gm + h_delta + h_gm_init, search_locs[i].v_gm + v_delta + v_gm_init);

				ofstr << " " << search_rssi;
			}

			ofstr << std::endl;
		}
	}
	ofstr.close();
	fso->setHorizontalGMVal(h_gm_init);
	fso->setVerticalGMVal(v_gm_init);

	std::cout << "SFPAutoAligner::mapRunWithSearch start" << std::endl;
}

void SFPAutoAligner::switchRun() {
	setSleepDuration(0);

	int h_gm_init = fso->getHorizontalGMVal();
	int v_gm_init = fso->getVerticalGMVal();

	int pre_switch_h_offset = 50, pre_switch_v_offset = 0;
	int post_switch_h_offset = 0, post_switch_v_offset = 0;

	typedef std::chrono::time_point<std::chrono::system_clock> time;
	typedef std::pair<float,std::pair<time, time> > switchData;

	std::vector<switchData> rssi_times;

	int num_messages = 10000;

	fso->setHorizontalGMVal(h_gm_init + pre_switch_h_offset);
	fso->setVerticalGMVal(v_gm_init + pre_switch_v_offset);

	for(int i = 0; i < num_messages; ++i) {
		time pre_get_rssi = std::chrono::system_clock::now();
		float rssi = getRSSI(h_gm_init + pre_switch_h_offset, v_gm_init + pre_switch_v_offset);
		time post_get_rssi = std::chrono::system_clock::now();
		rssi_times.push_back(std::make_pair(rssi, std::make_pair(pre_get_rssi, post_get_rssi)));
	}

	time pre_switch_time = std::chrono::system_clock::now();
	fso->setHorizontalGMVal(h_gm_init + post_switch_h_offset);
	fso->setVerticalGMVal(v_gm_init + post_switch_v_offset);
	time post_switch_time = std::chrono::system_clock::now();

	for(int i = 0; i < num_messages; ++i) {
		time pre_get_rssi = std::chrono::system_clock::now();
		float rssi = getRSSI(h_gm_init + post_switch_h_offset, v_gm_init + post_switch_v_offset);
		time post_get_rssi = std::chrono::system_clock::now();
		rssi_times.push_back(std::make_pair(rssi, std::make_pair(pre_get_rssi, post_get_rssi)));
	}

	std::string out_file = args->sfp_map_out_file;
	std::ofstream ofstr(out_file, std::ofstream::out);

	time relative_time = pre_switch_time;

	ofstr << "event pre_time(s) post_time(s) val" << std::endl;
	ofstr << "switch " << (pre_switch_time - relative_time).count() << " " << (post_switch_time - relative_time).count() << " " << pre_switch_h_offset << "," << pre_switch_v_offset << "," << post_switch_h_offset << "," << post_switch_v_offset << std::endl;
	for(unsigned int i = 0; i < rssi_times.size(); ++i) {
		ofstr << "rssi " << (rssi_times[i].second.first - relative_time).count() << " " << (rssi_times[i].second.second - relative_time).count() << " " << rssi_times[i].first << std::endl;
	}

	ofstr.close();

	fso->setHorizontalGMVal(h_gm_init);
	fso->setVerticalGMVal(v_gm_init);
}

void SFPAutoAligner::alignRun(Args* args_, FSO* fso_, const std::string &other_rack_id, const std::string &other_fso_id) {
	setValues(args_, fso_);

	setMultiParam(args->sfp_max_num_messages, args->sfp_max_num_changes, args->sfp_num_message_average);

	fso->setToLink(other_rack_id, other_fso_id);

	int h_gm, v_gm;
	int scan_range = 10, scan_step = 1;
	std::string input = "";

	while(true) {
		h_gm = fso->getHorizontalGMVal();
		v_gm = fso->getVerticalGMVal();
		std::cout << "Now at (" << h_gm << ", " << v_gm << ")" << std::endl;

		// Ask user what they want to do: 1) scan a given area, 2) redo the same scan, 3) quit
		input = "";
		while(input != "s" && input != "r" && input != "q") {
			std::cout << "Enter next action: (s)can, (r)edo last scan, (q)uit" << std::endl;
			std::cin >> input;
		}

		if(input == "q") {
			break;
		}

		if(input == "s") {
			std::cout << "Enter the scan range and step" << std::endl;
			std::cin >> scan_range >> scan_step;
		}

		// If requested do the scan and record the RSSIs for all locs
		std::cout << "Scanning with params: " << scan_range << " " << scan_step << std::endl;
		std::vector<GMVal> max_gm_vals;
		float max_rssi;
		bool first = true;
		// Find maximum RSSI
		for(int h_delta = -scan_range; h_delta <= scan_range; h_delta += scan_step) {
			for(int v_delta = -scan_range; v_delta <= scan_range; v_delta += scan_step) {
				fso->setHorizontalGMVal(h_gm + h_delta);
				fso->setVerticalGMVal(v_gm + v_delta);

				float rssi = getRSSI(h_gm + h_delta, v_gm + v_delta);

				if(first) {
					first = false;
					max_rssi = rssi;
					max_gm_vals.push_back(GMVal(h_gm + h_delta, v_gm + v_delta));
				} else {
					if(abs(max_rssi - rssi) <= 0.01) {
						max_gm_vals.push_back(GMVal(h_gm + h_delta, v_gm + v_delta));
					} else if(max_rssi < rssi) {
						max_gm_vals.clear();
						max_gm_vals.push_back(GMVal(h_gm + h_delta, v_gm + v_delta));

						max_rssi = rssi;
					}
				}
			}
		}

		// For now sets the GM to the average of all points, and reports the max_rssi and the number of points with that value
		// TODO compute a bounding polygon, and set the GM to the "middle", and report the area of the hull
		std::cout << "Found " << max_gm_vals.size() << " gm vals with RSSI of " << max_rssi << std::endl;

		int new_h_gm = 0;
		int new_v_gm = 0;
		for(unsigned int i = 0; i < max_gm_vals.size(); ++i) {
			new_h_gm += max_gm_vals[i].h_gm;
			new_v_gm += max_gm_vals[i].v_gm;
		}

		fso->setHorizontalGMVal(new_h_gm / max_gm_vals.size());
		fso->setVerticalGMVal(new_v_gm / max_gm_vals.size());
	}

	fso->saveCurrentSettings(other_rack_id, other_fso_id);
	fso->save();
	std::cout << "FSO saved!!!" << std::endl;
}

float SFPAutoAligner::getRSSI(int h_gm, int v_gm) {
	// TODO confirm "good" RSSI values:
	// 		0) Log values so they can be inspected later.
	// 		1) Sleep for longer.
	// 		2) Read k values and return the first edge value.
	// 		3) Constantly read values in separate thread. Update a shared variable when changes.
	if(get_rssi_mode == GetRSSIMode::SLEEP && sleep_milliseconds > 0) {
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

	float rssi = 0.0;

	if(get_rssi_mode == GetRSSIMode::SLEEP) {
		sendMsg(get_rssi_msg);

		std::string rssi_str;
		recvMsg(rssi_str);

		std::stringstream sstr(rssi_str);
		sstr >> rssi;
	} else if(get_rssi_mode == GetRSSIMode::MULTI) {
		// TODO to count as a change must have been k+ (either 2 or 3) prev_rssis
		bool first = true;
		float prev_rssi = 0.0;
		std::vector<float> unique_rssis;
		int num_changes = 0;


		std::stringstream rcv_rssis;
		rcv_rssis << "received rssis: {";

		for(int i = 0; i < max_num_messages; i++) {
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

			if(first) {
				unique_rssis.push_back(rssi);
			}

			if(!first && prev_rssi != rssi) {
				num_changes++;

				while(unique_rssis.size() >= (unsigned int)(num_message_average)) {
					unique_rssis.erase(unique_rssis.begin());
				}
				unique_rssis.push_back(rssi);

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

		if(unique_rssis.size() != 0) {
			float sum_rssi = 0.0;
			for(unsigned int i = 0; i < unique_rssis.size(); ++i) {
				sum_rssi += unique_rssis[i];
			}
			rssi = sum_rssi / float(unique_rssis.size());
		}
	}

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
