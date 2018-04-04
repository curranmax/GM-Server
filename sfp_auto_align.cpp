
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

float PowerTuple::squaredEuclideanDistance(const PowerTuple& other_tuple) const {
	if(this->size() != other_tuple.size()) {
		return -1.0;
	}
	float sum_dist = 0.0;
	for(int i = 0; i < this->size(); ++i) {
		sum_dist += pow(powers[i] - other_tuple.powers[i], 2.0);
	}
	return sum_dist;
}

SFPAutoAligner::SFPAutoAligner(int sock_, SockType sock_type_) {
	sock = sock_;
	sock_type = sock_type_;

	get_power_mode = GetPowerMode::SLEEP;
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

		if(args->sfp_constant_response >= 1) {
			controllerRunConstantUpdate();
		} else {
			controllerRun();
		}

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
	PowerMap power_map;
	PowerTupleMap power_tuple_map;

	LOG("getting data from: " + in_file);

	{
		// Reads the given file and fills the power_map.
		std::string line;
		int hd, vd;
		float power;
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
			sstr >> hd >> vd >> power;

			// TODO exclude zero values
			power_map[GMVal(hd, vd)] = power;

			{
				std::stringstream sstr;
				sstr << "added entry to power_map: (" << hd << ", " << vd << ") -> " << power;
				VLOG(sstr.str(), 1);
			}
		}

		// Creates the power_tuple_map.
		for(PowerMap::const_iterator table_itr = power_map.cbegin(); table_itr != power_map.cend(); ++table_itr) {
			power_tuple_map[table_itr->first].addValue(table_itr->second, args->sfp_relative_table_option);
			for(std::vector<GMVal>::const_iterator search_itr = search_locs.cbegin(); search_itr != search_locs.cend(); ++search_itr) {
				GMVal lookup_loc = GMVal(table_itr->first.h_gm + search_itr->h_gm, table_itr->first.v_gm + search_itr->v_gm);

				float lookup_power = 0.0;
				if(power_map.count(lookup_loc) > 0) {
					lookup_power = power_map.at(lookup_loc);
				}

				power_tuple_map[table_itr->first].addValue(lookup_power, args->sfp_relative_table_option);
			}
		}

		// Check that all entries in power_tuple_map have a length of (search_locs.size() + 1)
		for(PowerTupleMap::const_iterator table_itr = power_tuple_map.cbegin(); table_itr != power_tuple_map.cend(); ++table_itr) {
			if(table_itr->second.size() != (int(search_locs.size()) + 1)) {
				std::cerr << "Invalid tuple size: " << table_itr->second.size() << std::endl;
				return;
			}
		}
	}

	float tracking_start = args->sfp_tracking_start;
	float tracking_stop = args->sfp_tracking_stop;
	float max_power = 0;

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

	// Tracking Analysis;
	TrackingAnalysis regular_tracking;

	// Timing tests
	std::vector<float> get_power_times;
	std::chrono::time_point<std::chrono::system_clock> start_get_power;
	std::chrono::time_point<std::chrono::system_clock> end_get_power;
	std::chrono::duration<double> dur_get_power;

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
		start_get_power = std::chrono::system_clock::now();
		float this_power = getPower(h_gm, v_gm, false);
		end_get_power = std::chrono::system_clock::now();
		dur_get_power = end_get_power - start_get_power;
		get_power_times.push_back(dur_get_power.count());

		{
			std::stringstream sstr;
			sstr << "GM at (" << h_gm << ", " << v_gm << ") with Power of " << this_power;
			LOG(sstr.str());
		}
		std::cout << "GM at (" << h_gm << ", " << v_gm << ") with Power of " << this_power;

		if(this_power > max_power) {
			max_power = this_power;
		}

		float relative_difference = (max_power - this_power) /  max_power;
		if(!tracking && relative_difference >= tracking_start) {
			LOG("tracking turned on");
			tracking = true;
		}
		if(tracking && relative_difference < tracking_stop) {
			LOG("tracking turned off");
			tracking = false;
		}

		if(tracking) {
			PowerTuple this_tuple;
			this_tuple.addValue(this_power, args->sfp_relative_table_option);
			PowerMap search_powers;
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

				start_get_power = std::chrono::system_clock::now();
				float search_power = getPower(search_locs[i].h_gm + h_gm, search_locs[i].v_gm + v_gm, args->sfp_no_update);
				end_get_power = std::chrono::system_clock::now();
				dur_get_power = end_get_power - start_get_power;
				get_power_times.push_back(dur_get_power.count());

				{
					std::stringstream sstr;
					sstr << "search: GM at (" << search_locs[i].h_gm + h_gm << "[" << search_locs[i].h_gm << "], " << search_locs[i].v_gm + v_gm << "[" << search_locs[i].v_gm << "]) with Power of " << search_power;
					LOG(sstr.str());
				}

				this_tuple.addValue(search_power, args->sfp_relative_table_option);
				search_powers[search_locs[i]] = search_power;
			}

			// Search the table for the correction
			int h_err, v_err;
			float hr, vr;
			if(this_tuple.allZero() || this_power == 1) {
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
				findError(this_tuple, power_tuple_map, h_err, v_err);
	
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

			start_set_gm = std::chrono::system_clock::now();
			fso->setHorizontalGMVal(h_gm + hr);
			fso->setVerticalGMVal(v_gm + vr);
			end_set_gm = std::chrono::system_clock::now();
			dur_set_gm = end_set_gm - start_set_gm;
			set_gm_times.push_back(dur_set_gm.count());

			{
				std::stringstream sstr;
				sstr << "GM set to (" << h_gm + hr << ", " << v_gm + vr << ")";
				LOG(sstr.str());
			}

			std::cout << ", tracking moved (" << hr << ", " << vr << ")" << std::endl;

			regular_tracking.addVal(GMVal(hr, vr));
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

	// Get Power
	float sum_get_power_times = 0.0;
	for(unsigned int i = 0; i < get_power_times.size(); ++i) {
		sum_get_power_times += get_power_times[i];
	}
	std::cout << "Average getPower() time: " << sum_get_power_times / float(get_power_times.size()) * 1000.0 << " milliseconds per function call" << std::endl;
	std::cout << "Avarege getPower() per iter: " << float(get_power_times.size()) / float(num_iters) << " getPower() per iter" << std::endl;

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

	LOG("controllerRun end");
}

void SFPAutoAligner::controllerRunConstantUpdate() {
	LOG("start controllerRunConstantUpdate");

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

	// Thresholds for tracking
	float tracking_start = args->sfp_tracking_start;
	float tracking_stop = args->sfp_tracking_stop;
	float max_power = 0;

	// Basic variables
	bool tracking = false;
	int h_gm, v_gm;
	int num_iters = 0;

	// Allows tracking to be stopped by Ctrl+C
	struct sigaction sigIntHandler;
	sigIntHandler.sa_handler = sfpControllerHandler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;

	sigaction(SIGINT, &sigIntHandler, NULL);
	sfp_controller_loop = true;
	
	// Tracking Analysis;
	TrackingAnalysis constant_tracking;

	// Gradient and Constant response
	float gradient_threshold = args->sfp_gradient_threshold;
	int constant_response = args->sfp_constant_response;
	
	// Timing tests
	std::vector<float> get_power_times;
	std::chrono::time_point<std::chrono::system_clock> start_get_power;
	std::chrono::time_point<std::chrono::system_clock> end_get_power;
	std::chrono::duration<double> dur_get_power;

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
		start_get_power = std::chrono::system_clock::now();
		float this_power = getPower(h_gm, v_gm, false);
		end_get_power = std::chrono::system_clock::now();
		dur_get_power = end_get_power - start_get_power;
		get_power_times.push_back(dur_get_power.count());

		{
			std::stringstream sstr;
			sstr << "GM at (" << h_gm << ", " << v_gm << ") with Power of " << this_power;
			LOG(sstr.str());
		}
		std::cout << "GM at (" << h_gm << ", " << v_gm << ") with Power of " << this_power;

		if(this_power > max_power) {
			max_power = this_power;
		}

		float relative_difference = (max_power - this_power) /  max_power;
		if(!tracking && relative_difference >= tracking_start) {
			LOG("tracking turned on");
			tracking = true;
		}
		if(tracking && relative_difference < tracking_stop) {
			LOG("tracking turned off");
			tracking = false;
		}

		if(tracking) {
			// Query the necessary points
			float h_response = 0.0, v_response = 0.0;
			for(unsigned int i = 0; i < search_locs.size(); ++i) {
				start_set_gm = std::chrono::system_clock::now();
				fso->setHorizontalGMVal(search_locs[i].h_gm + h_gm);
				fso->setVerticalGMVal(search_locs[i].v_gm + v_gm);
				end_set_gm = std::chrono::system_clock::now();
				dur_set_gm = end_set_gm - start_set_gm;
				set_gm_times.push_back(dur_set_gm.count());

				start_get_power = std::chrono::system_clock::now();
				float search_power = getPower(search_locs[i].h_gm + h_gm, search_locs[i].v_gm + v_gm, args->sfp_no_update);
				end_get_power = std::chrono::system_clock::now();
				dur_get_power = end_get_power - start_get_power;
				get_power_times.push_back(dur_get_power.count());

				{
					std::stringstream sstr;
					sstr << "search: GM at (" << search_locs[i].h_gm + h_gm << "[" << search_locs[i].h_gm << "], " << search_locs[i].v_gm + v_gm << "[" << search_locs[i].v_gm << "]) with Power of " << search_power;
					LOG(sstr.str());
				}

				// Calculate gradient
				float gradient = (search_power - this_power) / search_locs[i].mag();
				{
					std::stringstream sstr;
					sstr << "gradient for (" << search_locs[i].h_gm << ", " << search_locs[i].v_gm << ") is " << gradient;
					LOG(sstr.str());
				}

				float h_contribution = 0.0, v_contribution = 0.0;
				if(gradient > gradient_threshold){
					// positive k
					h_contribution = constant_response * search_locs[i].h_gm / search_locs[i].mag();
					v_contribution = constant_response * search_locs[i].v_gm / search_locs[i].mag();
				} else if(gradient < -gradient_threshold) {
					// negative k
					h_contribution = -constant_response * search_locs[i].h_gm / search_locs[i].mag();
					v_contribution = -constant_response * search_locs[i].v_gm / search_locs[i].mag();
				}

				{
					std::stringstream sstr;
					sstr << "contribution for (" << search_locs[i].h_gm << ", " << search_locs[i].v_gm << ") is (" << h_contribution << ", " << v_contribution << ")";
					LOG(sstr.str());
				}

				h_response += h_contribution;
				v_response += v_contribution;
			}

			{
				std::stringstream sstr;
				sstr << "response is (" << h_response << ", " << v_response << ")";
				LOG(sstr.str());
			}

			start_set_gm = std::chrono::system_clock::now();
			fso->setHorizontalGMVal(h_gm + round(h_response));
			fso->setVerticalGMVal(v_gm + round(v_response));
			end_set_gm = std::chrono::system_clock::now();
			dur_set_gm = end_set_gm - start_set_gm;
			set_gm_times.push_back(dur_set_gm.count());

			{
				std::stringstream sstr;
				sstr << "GM set to (" << h_gm + round(h_response) << ", " << v_gm + round(v_response) << ")";
				LOG(sstr.str());
			}

			std::cout << ", tracking moved (" << round(h_response) << ", " << round(v_response) << ")" << std::endl;


			constant_tracking.addVal(GMVal(round(h_response), round(v_response)));
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

	// Get Power
	float sum_get_power_times = 0.0;
	for(unsigned int i = 0; i < get_power_times.size(); ++i) {
		sum_get_power_times += get_power_times[i];
	}
	std::cout << "Average getPower() time: " << sum_get_power_times / float(get_power_times.size()) * 1000.0 << " milliseconds per function call" << std::endl;
	std::cout << "Avarege getPower() per iter: " << float(get_power_times.size()) / float(num_iters) << " getPower() per iter" << std::endl;

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

	std::cout << "Average iter change [const]: " << constant_tracking.avgIterChange() << std::endl;

	LOG("end controllerRunConstantUpdate");
}

void SFPAutoAligner::findError(const PowerTuple& this_tuple, const PowerTupleMap& power_map, int &h_err, int &v_err) {
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
			sstr << this_tuple.powers[i];
		}
		sstr << ">";
		LOG(sstr.str());
	}

	for(PowerTupleMap::const_iterator table_itr = power_map.cbegin(); table_itr != power_map.cend(); ++table_itr) {
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
	ofstr << "H_Delta V_Delta Power" << std::endl;

	for(int h_delta = -map_range; h_delta <= map_range; h_delta += map_step) {
		for(int v_delta = -map_range; v_delta <= map_range; v_delta += map_step) {
			fso->setHorizontalGMVal(h_delta + h_gm_init);
			fso->setVerticalGMVal(v_delta + v_gm_init);

			std::cout << "GM:(" << h_delta << ", " << v_delta << ")";
			
			// Get Power
			float power = getPower(h_delta + h_gm_init, v_delta + v_gm_init, false);

			std::cout << "\t --> Power = " << power << std::endl;

			// Save it
			ofstr << h_delta << " " << v_delta << " " << power << std::endl;
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
	ofstr << "H_Delta V_Delta Power[0,0]";

	for(unsigned int i = 0; i < search_locs.size(); ++i) {
		ofstr << " Power[" << search_locs[i].h_gm << "," << search_locs[i].v_gm << "]";
	}
	ofstr << std::endl;

	for(int h_delta = -map_range; h_delta <= map_range; h_delta += map_step) {
		for(int v_delta = -map_range; v_delta <= map_range; v_delta += map_step) {
			fso->setHorizontalGMVal(h_delta + h_gm_init);
			fso->setVerticalGMVal(v_delta + v_gm_init);

			std::cout << "GM:(" << h_delta << ", " << v_delta << ")";
			
			// Get Power
			float power = getPower(h_delta + h_gm_init, v_delta + v_gm_init, false);

			std::cout << "\t --> Power = " << power << std::endl;

			// Save it
			ofstr << h_delta << " " << v_delta << " " << power;

			for(unsigned int i = 0; i < search_locs.size(); ++i) {
				fso->setHorizontalGMVal(search_locs[i].h_gm + h_delta + h_gm_init);
				fso->setVerticalGMVal(search_locs[i].v_gm + v_delta + v_gm_init);

				float search_power = getPower(search_locs[i].h_gm + h_delta + h_gm_init, search_locs[i].v_gm + v_delta + v_gm_init, false);

				ofstr << " " << search_power;
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

	std::vector<switchData> power_times;

	int num_messages = 10000;

	fso->setHorizontalGMVal(h_gm_init + pre_switch_h_offset);
	fso->setVerticalGMVal(v_gm_init + pre_switch_v_offset);

	for(int i = 0; i < num_messages; ++i) {
		time pre_get_power = std::chrono::system_clock::now();
		float power = getPower(h_gm_init + pre_switch_h_offset, v_gm_init + pre_switch_v_offset, false);
		time post_get_power = std::chrono::system_clock::now();
		power_times.push_back(std::make_pair(power, std::make_pair(pre_get_power, post_get_power)));
	}

	time pre_switch_time = std::chrono::system_clock::now();
	fso->setHorizontalGMVal(h_gm_init + post_switch_h_offset);
	fso->setVerticalGMVal(v_gm_init + post_switch_v_offset);
	time post_switch_time = std::chrono::system_clock::now();

	for(int i = 0; i < num_messages; ++i) {
		time pre_get_power = std::chrono::system_clock::now();
		float power = getPower(h_gm_init + post_switch_h_offset, v_gm_init + post_switch_v_offset, false);
		time post_get_power = std::chrono::system_clock::now();
		power_times.push_back(std::make_pair(power, std::make_pair(pre_get_power, post_get_power)));
	}

	std::string out_file = args->sfp_map_out_file;
	std::ofstream ofstr(out_file, std::ofstream::out);

	time relative_time = pre_switch_time;

	ofstr << "event pre_time(s) post_time(s) val" << std::endl;
	ofstr << "switch " << (pre_switch_time - relative_time).count() << " " << (post_switch_time - relative_time).count() << " " << pre_switch_h_offset << "," << pre_switch_v_offset << "," << post_switch_h_offset << "," << post_switch_v_offset << std::endl;
	for(unsigned int i = 0; i < power_times.size(); ++i) {
		ofstr << "power " << (power_times[i].second.first - relative_time).count() << " " << (power_times[i].second.second - relative_time).count() << " " << power_times[i].first << std::endl;
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

		// If requested do the scan and record the Powers for all locs
		std::cout << "Scanning with params: " << scan_range << " " << scan_step << std::endl;
		std::vector<GMVal> max_gm_vals;
		float max_power;
		bool first = true;
		// Find maximum Power
		for(int h_delta = -scan_range; h_delta <= scan_range; h_delta += scan_step) {
			for(int v_delta = -scan_range; v_delta <= scan_range; v_delta += scan_step) {
				fso->setHorizontalGMVal(h_gm + h_delta);
				fso->setVerticalGMVal(v_gm + v_delta);

				float power = getPower(h_gm + h_delta, v_gm + v_delta, false);

				if(first) {
					first = false;
					max_power = power;
					max_gm_vals.push_back(GMVal(h_gm + h_delta, v_gm + v_delta));
				} else {
					if(abs(max_power - power) <= 0.01) {
						max_gm_vals.push_back(GMVal(h_gm + h_delta, v_gm + v_delta));
					} else if(max_power < power) {
						max_gm_vals.clear();
						max_gm_vals.push_back(GMVal(h_gm + h_delta, v_gm + v_delta));

						max_power = power;
					}
				}
			}
		}

		// For now sets the GM to the average of all points, and reports the max_power and the number of points with that value
		// TODO compute a bounding polygon, and set the GM to the "middle", and report the area of the hull
		std::cout << "Found " << max_gm_vals.size() << " gm vals with Power of " << max_power << std::endl;

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

void SFPAutoAligner::listenerRun() {
	LOG("listenerRun start");

	std::string msg, token;
	bool listener_loop = true;
	while(listener_loop) {
		recvMsg(msg);
		std::stringstream sstr(msg);

		sstr >> token;
		if(token == "X_out") {
			givePowerVoltage(fso->getPowerDiodeVoltage());
		} else if(token == "end_tracking") {
			listener_loop = false;
		} else {
			std::cerr << "UNEXPECTED MESSAGE: " << msg << std::endl;
		}
	}

	LOG("listenerRun end");
}

float SFPAutoAligner::getPower(int h_gm, int v_gm, bool no_update) {
	// TODO confirm "good" Power values:
	// 		0) Log values so they can be inspected later.
	// 		1) Sleep for longer.
	// 		2) Read k values and return the first edge value.
	// 		3) Constantly read values in separate thread. Update a shared variable when changes.
	if(get_power_mode == GetPowerMode::SLEEP && sleep_milliseconds > 0) {
		LOG("getPower: start sleep");
		usleep(1000 * sleep_milliseconds);
		LOG("getPower: end sleep");
	}

	std::string get_power_msg = "X_out";
	if(args->sfp_test_server) {
		std::stringstream sstr;
		sstr << "get_power " << h_gm << " " << v_gm;

		if(no_update) {
			sstr << " no_update";
		}

		get_power_msg = sstr.str();
	}

	float power = 0.0;

	if(get_power_mode == GetPowerMode::SLEEP) {
		sendMsg(get_power_msg);

		std::string power_str;
		recvMsg(power_str);

		std::stringstream sstr(power_str);
		sstr >> power;
	} else if(get_power_mode == GetPowerMode::MULTI) {
		// TODO to count as a change must have been k+ (either 2 or 3) prev_powers
		bool first = true;
		float prev_power = 0.0;
		std::vector<float> unique_powers;
		int num_changes = 0;


		std::stringstream rcv_powers;
		rcv_powers << "received powers: {";

		for(int i = 0; i < max_num_messages; i++) {
			prev_power = power;

			sendMsg(get_power_msg);

			std::string power_str;
			recvMsg(power_str);

			std::stringstream sstr(power_str);
			sstr >> power;

			if(i != 0) {
				rcv_powers << ", ";
			}
			rcv_powers << power;

			if(first) {
				unique_powers.push_back(power);
			}

			if(!first && prev_power != power) {
				num_changes++;

				while(unique_powers.size() >= (unsigned int)(num_message_average)) {
					unique_powers.erase(unique_powers.begin());
				}
				unique_powers.push_back(power);

				if(num_changes >= max_num_changes) {
					break;
				}
			}

			if(first) {
				first = false;
			}
		}

		rcv_powers << "}";
		LOG(rcv_powers.str());

		if(unique_powers.size() != 0) {
			float sum_power = 0.0;
			for(unsigned int i = 0; i < unique_powers.size(); ++i) {
				sum_power += unique_powers[i];
			}
			power = sum_power / float(unique_powers.size());
		}
	}

	{
		std::stringstream sstr;
		sstr << "fetched power is: " << power;
		LOG(sstr.str());
	}
	if(power <= args->sfp_power_zero_value) {
		power = 0.0;
	}

	return power;
}

void SFPAutoAligner::endTracking() {
	LOG("sending end tracking message");
	sendMsg("end_tracking");
}

void SFPAutoAligner::givePowerVoltage(float volt) {
	{
		std::stringstream sstr;
		sstr << "giving voltage of " << volt;
		LOG(sstr.str());
	}

	std::stringstream sstr;
	sstr << volt;
	sendMsg(sstr.str());
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
