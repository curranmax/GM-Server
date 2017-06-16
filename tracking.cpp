
#include "tracking.h"

#include "volt_quad.h"

#include <signal.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <chrono>
#include <math.h>
#include <thread>
#include <unistd.h>
#include <stdlib.h>

TrackingSystem::TrackingSystem(int sock_, bool is_controller_)
		: sock(sock_), is_controller(is_controller_),
			fso(nullptr), args(nullptr) {}

TrackingSystem::~TrackingSystem() {
	close(sock);
}

void TrackingSystem::run(FSO* fso_, Args* args_) {
	std::cout << "Starting TrackingSystem::run" << std::endl;
	
	fso = fso_;
	args = args_;

	if (is_controller) {
		if (args->do_map_voltage) {
			std::string option = "";
			std::cout << "MapVoltage (m) or NoiseRun (n)?" << std::endl;
			std::cin >> option;
			if(option == "n") {
				noiseRun();
			}  else {
				mapVoltage();
			}
		} else {
			controllerRun();
		}
	} else {
		listenerRun();
	}

	std::cout << "Ending TrackingSystem::run" << std::endl;
}

float TrackingSystem::computeResponse(float p_volt, float n_volt, float goal_difference) {
	// Treats any voltages that are negative as 0
	if(p_volt <= 0.0) {
		p_volt = 0.0;
	}
	if(n_volt <= 0.0) {
		n_volt = 0.0;
	}

	return args->k_proportional * (n_volt - p_volt - goal_difference);
}

bool controller_loop = false;

void controllerHandler(int s) {
	controller_loop = false;
}

// PDI controller
void TrackingSystem::controllerRun() {
	// Assume this side has a gm, and the other side has diodes
	// Set the link of this FSO correctly
	std::string other_rack_id = "", other_fso_id = "";
	get_fso(other_rack_id, other_fso_id);
	fso->setToLink(other_rack_id,other_fso_id);

	int init_h_gm = fso->getHorizontalGMVal();
	int init_v_gm = fso->getVerticalGMVal();

	std::cout << "TrackingSystem::controllerRun start" << std::endl;
	float ph_volt = 0.0, nh_volt = 0.0, pv_volt = 0.0, nv_volt = 0.0, horizontal_response = 0.0, vertical_response = 0.0;
	int x = 0, every_x_print = 1;

	float init_ph_volt = 0.0, init_nh_volt = 0.0, init_pv_volt = 0.0, init_nv_volt = 0.0;
	get_diode_voltage(init_ph_volt, init_nh_volt, init_pv_volt, init_nv_volt);

	std::map<std::pair<int,int>, VoltQuad> data_table;
	if(args->data_drive_tracking_file != "") {
		std::ifstream ifstr(args->data_drive_tracking_file, std::ifstream::in);
		std::string line;
		float hd, vd, ph, nh, pv, nv, hr, vr;
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
			sstr >> hd >> vd >> ph >> nh >> pv >> nv >> hr >> vr;
			if(ph > 0.0 || nh > 0.0 || pv > 0.0 || nv > 0.0) {
				data_table[std::make_pair(hd, vd)] = VoltQuad(ph, nh, pv, nv);
			}
		}
	}

	float k_proportional = args->k_proportional;
	float k_integral = args->k_integral;
	float k_derivative = args->k_derivative;

	float prev_h_err = 0.0;
	float prev_v_err = 0.0;
	float sum_h_err = 0.0;
	float sum_v_err = 0.0;

	bool keep_beam_stationary = args->keep_beam_stationary;
	float goal_horizontal_difference = 0.0, goal_vertical_difference = 0.0;
	if (keep_beam_stationary) {
		goal_horizontal_difference = init_ph_volt - init_nh_volt;
		goal_vertical_difference = init_pv_volt - init_nv_volt;

		std::cout << goal_horizontal_difference << " " << goal_vertical_difference << std::endl;
	}
	
	struct sigaction sigIntHandler;

	sigIntHandler.sa_handler = controllerHandler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;

	sigaction(SIGINT, &sigIntHandler, NULL);
	controller_loop = true;

	int prev_h_gm = 0, prev_v_gm = 0;
	std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();
	while(controller_loop) {
		if (x % every_x_print == 0) {
			std::cout << "GM-POS: (" << fso->getHorizontalGMVal() << ", " << fso->getVerticalGMVal() << ")    ";
			std::cout << "RESPONSE: (" << fso->getHorizontalGMVal() - prev_h_gm << ", " << fso->getVerticalGMVal() - prev_v_gm << ")";
			prev_h_gm = fso->getHorizontalGMVal();
			prev_v_gm = fso->getVerticalGMVal();
		}
		// Get diode levels
		get_diode_voltage(ph_volt, nh_volt, pv_volt, nv_volt);

		float err_h = 0.0;
		float err_v = 0.0;

		if(ph_volt <= 0.0 && nh_volt <= 0.0 && pv_volt <= 0.0 && nv_volt <= 0.0) {
			// If all volts 0 then do nothing
			horizontal_response = 0;
			vertical_response = 0;
		} else if(data_table.size() == 0) {
			horizontal_response = computeResponse(ph_volt, nh_volt, goal_horizontal_difference);
			vertical_response = computeResponse(pv_volt, nv_volt, goal_vertical_difference);

			err_h = nh_volt - ph_volt - goal_horizontal_difference;
			err_v = nv_volt - pv_volt - goal_vertical_difference;
		} else {
			// Set of points with voltages within epsilon
			horizontal_response = 0;
			vertical_response = 0;

			int this_rh = 0, this_rv = 0;
			float this_volt_dist = -1.0, itr_volt_dist = -1.0;
			VoltQuad this_vq(ph_volt, nh_volt, pv_volt, nv_volt);
			for(std::map<std::pair<int, int>, VoltQuad>::const_iterator itr = data_table.begin(); itr != data_table.end(); ++itr) {
				itr_volt_dist = itr->second.dist(this_vq);
				
				// TODO confirm this giant bool expression
				if(this_volt_dist < 0.0 ||
						(itr_volt_dist > args->epsilon && this_volt_dist > args->epsilon &&
								(itr_volt_dist < this_volt_dist ||
									(itr_volt_dist == this_volt_dist &&
										pow(this_rh, 2) + pow(this_rv, 2) > pow(itr->first.first, 2) + pow(itr->first.second, 2)))) ||
						(itr_volt_dist <= args->epsilon && this_volt_dist > args->epsilon) ||
						(itr_volt_dist <= args->epsilon && this_volt_dist <= args->epsilon &&
								pow(this_rh, 2) + pow(this_rv, 2) > pow(itr->first.first, 2) + pow(itr->first.second, 2))) {
				// if((this_volt_dist < 0.0 && itr_volt_dist <= args->epsilon) || 
				// 		(itr_volt_dist <= args->epsilon && this_volt_dist <= args->epsilon &&
				// 				pow(this_rh, 2) + pow(this_rv, 2) > pow(itr->first.first, 2) + pow(itr->first.second, 2))) {
					this_rh = itr->first.first;
					this_rv = itr->first.second;
					this_volt_dist = itr_volt_dist;
				}
			}
			err_h = -this_rh;
			err_v = -this_rv;
		}
		sum_h_err += err_h;
		sum_v_err += err_v;

		horizontal_response = k_proportional * err_h + k_integral * sum_h_err + k_derivative * (err_h - prev_h_err);
		vertical_response =   k_proportional * err_v + k_integral * sum_v_err + k_derivative * (err_v - prev_v_err);

		prev_h_err = err_h;
		prev_v_err = err_v;

		std::cout << "   (" << ph_volt << ", " << nh_volt <<")";
		std::cout << "   (" << pv_volt << ", " << nv_volt <<")" << std::endl;

		// Change GMs accordingly
		fso->setHorizontalGMVal(int(horizontal_response) + fso->getHorizontalGMVal());
		fso->setVerticalGMVal(int(vertical_response) + fso->getVerticalGMVal());

		x += 1;
		// Record Stats???
	}
	signal(SIGINT, SIG_DFL);

	end_tracking();

	std::chrono::time_point<std::chrono::system_clock> end = std::chrono::system_clock::now();
	std::chrono::duration<double> dur = end - start;
	float total_seconds = dur.count();
	float seconds_per_test = total_seconds / float(x);

	std::cout << "Avg Iteration time: " << seconds_per_test * 1000 << "millisecond" << std::endl;

	// Asks user what they want to do with result of tracking
	std::string save_tracking_link = "";
	std::cout << "Do you want to save the current gm settings(y) or revert to initial settings(n)?" << std::endl;
	std::cin >> save_tracking_link;
	
	if(save_tracking_link == "n") {
		fso->setHorizontalGMVal(init_h_gm);
		fso->setVerticalGMVal(init_v_gm);
	}

	fso->saveCurrentSettings(other_rack_id, other_fso_id);
	fso->save();
	std::cout << "FSO saved!!!" << std::endl;
	
	std::cout << "TrackingSystem::controllerRun end" << std::endl;
}

void TrackingSystem::timingRun() {
	// Assume this side has a gm, and the other side has diodes
	// Set the link of this FSO correctly
	std::string other_rack_id = "", other_fso_id = "";
	get_fso(other_rack_id, other_fso_id);
	fso->setToLink(other_rack_id,other_fso_id);

	std::cout << "TrackingSystem::timingRun start" << std::endl;
	float ph_volt = 0.0, nh_volt = 0.0, pv_volt = 0.0, nv_volt = 0.0;
	float horizontal_response = 0.0, vertical_response = 0.0;
	int x = 0, every_x_print = 1;

	float init_ph_volt = 0.0, init_nh_volt = 0.0, init_pv_volt = 0.0, init_nv_volt = 0.0;
	get_diode_voltage(init_ph_volt, init_nh_volt, init_pv_volt, init_nv_volt);

	int prev_h_gm = 0, prev_v_gm = 0;
	int num_iters = 1000;
	std::vector<float> volts;
	std::chrono::time_point<std::chrono::steady_clock> start = std::chrono::steady_clock::now();
	for(int i = 0; i < num_iters; ++i) {
		if (x % every_x_print == 0) {
			std::cout << "GM-POS: (" << fso->getHorizontalGMVal() << ", " << fso->getVerticalGMVal() << ")    ";
			std::cout << "RESPONSE: (" << fso->getHorizontalGMVal() - prev_h_gm << ", " << fso->getVerticalGMVal() - prev_v_gm << ")";
			prev_h_gm = fso->getHorizontalGMVal();
			prev_v_gm = fso->getVerticalGMVal();
		}
		// // Get diode levels
		get_diode_voltage(ph_volt, nh_volt, pv_volt, nv_volt);

		horizontal_response = computeResponse(ph_volt, nh_volt, 0.0);
		vertical_response = computeResponse(pv_volt, nv_volt, 0.0);

		std::cout << "   (" << ph_volt << ", " << nh_volt << ")";
		std::cout << "   (" << pv_volt << ", " << nv_volt << ")" << std::endl;

		// // Change GMs accordingly
		fso->setHorizontalGMVal(int(horizontal_response) + fso->getHorizontalGMVal());
		fso->setVerticalGMVal(int(vertical_response) + fso->getVerticalGMVal());

		x += 1;
		// // Record Stats???
		
		// std::this_thread::sleep_for(std::chrono::milliseconds(2));
	}

	end_tracking();

	std::chrono::time_point<std::chrono::steady_clock> end = std::chrono::steady_clock::now();
	std::chrono::duration<double> dur = end - start;
	double total_seconds = dur.count();
	double seconds_per_test = total_seconds / double(x);

	std::cout << "Total Seconds: " << total_seconds << std::endl; 
	std::cout << "Avg Iteration time: " << seconds_per_test * 1000 << " millisecond" << std::endl;

	std::cout << "TrackingSystem::timingRun end" << std::endl;
}

void TrackingSystem::mapVoltage() {
	std::string other_rack_id = "", other_fso_id = "";
	get_fso(other_rack_id, other_fso_id);
	fso->setToLink(other_rack_id, other_fso_id);

	int h_gm_init = fso->getHorizontalGMVal();
	int v_gm_init = fso->getVerticalGMVal();

	std::cout << "TrackingSystem::mapVoltage start"<< std::endl;
	float ph_volt = 0.0, nh_volt = 0.0, pv_volt = 0.0, nv_volt = 0.0;

	int map_range = args->map_range;
	int map_step = args->map_step;

	std::string record_type = args->record_type;

	std::string out_file = args->map_voltage_out_file;
	std::ofstream ofstr(out_file, std::ofstream::out);

	float milliseconds_to_wait = 4.0;

	// Output parameters
	ofstr << std::endl << "PARAMS kp:" << args->k_proportional << " record_type:" << record_type << " map_range:" << map_range << " map_step:" << map_step << std::endl;

	// Col_hdrs
	std::map<std::pair<int, int>, float> ph_volts;
	std::map<std::pair<int, int>, float> nh_volts;
	std::map<std::pair<int, int>, float> pv_volts;
	std::map<std::pair<int, int>, float> nv_volts;
	ofstr << "hd vd ph nh pv nv hr vr" << std::endl;
	for (int horizontal_delta = -map_range; horizontal_delta <= map_range; horizontal_delta += map_step) {
		for (int vertical_delta = -map_range; vertical_delta <= map_range; vertical_delta += map_step) {
			if(record_type == "linear" && horizontal_delta != 0 && vertical_delta != 0) {
				continue;
			}
			std::cout << "GM_DELTA:(" << horizontal_delta << ", " << vertical_delta << ")  ";

			fso->setHorizontalGMVal(horizontal_delta + h_gm_init);
			fso->setVerticalGMVal(vertical_delta + v_gm_init);

			get_diode_voltage(ph_volt, nh_volt, pv_volt, nv_volt);
			std::cout << "HV:(+: " << ph_volt << ", -: " << nh_volt << ")  ";
			std::cout << "HR: " << computeResponse(ph_volt, nh_volt, 0.0) << "  ";

			std::cout << "VV:(+: " << pv_volt << ", -: " << nv_volt << ")  ";
			std::cout << "VR: " << computeResponse(pv_volt, nv_volt, 0.0) << std::endl;

			ofstr << horizontal_delta << " " << vertical_delta << " "
					<< ph_volt << " " << nh_volt << " "
					<< pv_volt << " " << nv_volt << " "
					<< computeResponse(ph_volt, nh_volt, 0.0) << " " << computeResponse(pv_volt, nv_volt, 0.0) << std::endl;

			if (milliseconds_to_wait > 0.0) {
				usleep(int(milliseconds_to_wait * 1000.0));
			}
		}
	}
	ofstr.close();
	end_tracking();

	fso->setHorizontalGMVal(h_gm_init);
	fso->setVerticalGMVal(v_gm_init);

	std::cout << "TrackingSystem::mapVoltage end"<< std::endl;
}

void printStats(const std::vector<float>& vals) {
	// Min & Max
	float max_val = 0;
	float min_val = 0;

	// Average
	float sum_val = 0.0;

	for(unsigned int i = 0; i < vals.size(); ++i) {
		if (i == 0 || max_val < vals[i]) {
			max_val = vals[i];
		}
		if (i == 0 || min_val > vals[i]) {
			min_val = vals[i];
		}
		sum_val += vals[i];
	}

	float average_value = sum_val / vals.size();

	// Std deviation
	float sum_deviation = 0.0;
	for(unsigned int i = 0; i < vals.size(); ++i) {
		sum_deviation += (average_value - vals[i]) * (average_value - vals[i]);
	}

	float std_deviation = sum_deviation / vals.size();

	std::cout << "max: " << min_val << ", min: " << max_val << ", avg: " << average_value << ", std_dev: " << std_deviation << std::endl;
}

void TrackingSystem::noiseRun() {
	std::string other_rack_id = "", other_fso_id = "";
	get_fso(other_rack_id, other_fso_id);
	fso->setToLink(other_rack_id, other_fso_id);

	int h_gm_init = fso->getHorizontalGMVal();
	int v_gm_init = fso->getVerticalGMVal();

	std::string out_file = args->map_voltage_out_file;
	std::ofstream ofstr(out_file, std::ofstream::out);

	float ph_volt = 0.0, nh_volt = 0.0, pv_volt = 0.0, nv_volt = 0.0;

	std::vector<float> ph_volts, nh_volts, pv_volts, nv_volts;

	unsigned int num_points_to_try = 10;
	int num_samples_to_take = 1000;
	int random_range = 100;

	std::vector<int> const_h_gm_vals = {};
	std::vector<int> const_v_gm_vals = {};

	for(unsigned int i = 1; i <= num_points_to_try; ++i) {
		int h_gm_val = rand() % (2 * random_range + 1) - random_range;
		int v_gm_val = rand() % (2 * random_range + 1) - random_range;

		if(i <= const_h_gm_vals.size() && i <= const_v_gm_vals.size()) {
			h_gm_val = const_h_gm_vals[i - 1];
			v_gm_val = const_v_gm_vals[i - 1];
		}

		std::cout << "Running test " << i  << " at loc (" << h_gm_val << ", " << v_gm_val << ")" << std::endl;
		ofstr << "Test:" << i << " H:" << h_gm_val << " V:" << v_gm_val << std::endl;

		fso->setHorizontalGMVal(h_gm_val + h_gm_init);
		fso->setVerticalGMVal(v_gm_val + v_gm_init);

		ph_volts.clear();
		nh_volts.clear();
		pv_volts.clear();
		nv_volts.clear();
		for(int j = 0; j < num_samples_to_take; ++j) {
			get_diode_voltage(ph_volt, nh_volt, pv_volt, nv_volt);

			ofstr << ph_volt << " " << nh_volt << " " << pv_volt << " " << nv_volt << std::endl;
			ph_volts.push_back(ph_volt);
			nh_volts.push_back(nh_volt);
			pv_volts.push_back(pv_volt);
			nv_volts.push_back(nv_volt);
		}
		std::cout << "PH Diode --> ";
		printStats(ph_volts);
		std::cout << "NH Diode --> ";
		printStats(nh_volts);
		std::cout << "PV Diode --> ";
		printStats(pv_volts);
		std::cout << "NV Diode --> ";
		printStats(nv_volts);

	}
	ofstr.close();
	end_tracking();

	fso->setHorizontalGMVal(h_gm_init);
	fso->setVerticalGMVal(v_gm_init);

	std::cout << "TrackingSystem::mapVoltage end"<< std::endl;
}

void TrackingSystem::listenerRun() {
	std::cout << "TrackingSystem::listenerRun start" << std::endl;

	// Assume this side has diodes
	std::string msg, token;
	bool listener_loop = true;
	while(listener_loop) {
		recv_msg(msg);
		std::stringstream sstr(msg);

		sstr >> token;
		if(token == "get_fso") {
			give_fso(fso->getRackID(), fso->getFSOID());
		} else if(token == "get_diode_voltage") {
			give_diode_voltage(fso->getPositiveHorizontalDiodeVoltage(),
								fso->getNegativeHorizontalDiodeVoltage(),
								fso->getPositiveVerticalDiodeVoltage(),
								fso->getNegativeVerticalDiodeVoltage());			
		} else if(token == "end_tracking") {
			listener_loop = false;
			confirm_end_tracking();
		} else {
			std::cerr << "UNEXPECTED MESSAGE: " << msg << std::endl;
		}
	}
	std::cout << "TrackingSystem::listenerRun end" << std::endl;
}

void TrackingSystem::send_msg(const std::string &msg) {
	send(sock, msg.c_str(), msg.size(), 0);
}

void TrackingSystem::recv_msg(std::string &msg) {
	// TODO bug when the message received is longer than the buf_len
	msg = "";
	int buf_len = 256;
	char buf[buf_len];

	bool recv_loop = true;
	while(recv_loop) {
		int recv_len = recv(sock, buf, buf_len - 1, 0);
		recv_loop = (recv_len == buf_len - 1 && buf[recv_len - 1] != '\0');
		buf[recv_len] = '\0';
		msg += buf;
	}
}

void TrackingSystem::get_fso(std::string& other_rack_id, std::string& other_fso_id) {
	other_rack_id = "";
	other_fso_id = "";

	send_msg("get_fso");

	std::string rmsg;
	recv_msg(rmsg);

	std::stringstream sstr(rmsg);

	std::string token;
	sstr >> token;

	if (token != "give_fso") {
		return;
	}

	sstr >> other_rack_id >> other_fso_id;
}

void TrackingSystem::get_diode_voltage(float& ph_volt, float& nh_volt, float& pv_volt, float& nv_volt) {
	ph_volt = 0.0;
	nh_volt = 0.0;
	pv_volt = 0.0;
	nv_volt = 0.0;

	send_msg("get_diode_voltage");

	std::string rmsg;
	recv_msg(rmsg);

	std::stringstream sstr(rmsg);

	std::string token;
	sstr >> token;
	if(token != "give_diode_voltage") {
		return;
	}
	sstr >> ph_volt >> nh_volt >> pv_volt >> nv_volt;
}

void TrackingSystem::end_tracking() {
	send_msg("end_tracking");

	std::string rmsg;
	recv_msg(rmsg);

	// Realize this does nothing, but it at least clearly defines expectations
	if(rmsg != "confirm_end_tracking") {
		return;
	}
}

void TrackingSystem::give_fso(const std::string& this_rack, const std::string& this_fso) {
	std::string message = "give_fso " + this_rack + " " + this_fso;
	send_msg(message);
}

void TrackingSystem::give_diode_voltage(float ph_volt, float nh_volt, float pv_volt, float nv_volt) {
	std::string message = "give_diode_voltage " + std::to_string(ph_volt)
											+ " " + std::to_string(nh_volt)
											+ " " + std::to_string(pv_volt)
											+ " " + std::to_string(nv_volt);
	send_msg(message);
}

void TrackingSystem::confirm_end_tracking() {
	send_msg("confirm_end_tracking");
}

TrackingSystem* TrackingSystem::listenFor(int listen_port,const std::string &rack_id,const std::string &fso_id) {
	struct addrinfo hints, *serv_info, *p;
	int sock, one = 1, backlog = 10;

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
		return new TrackingSystem(connect_sock, false);
	} else {
		close(connect_sock);
		return NULL;
	}
}

TrackingSystem* TrackingSystem::connectTo(int send_port,const std::string &host_addr,const std::string &rack_id,const std::string &fso_id) {
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
		return new TrackingSystem(sock,true);
	} else {
		close(sock);
		return NULL;
	}
}
