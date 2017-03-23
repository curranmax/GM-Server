
#include "tracking.h"

#include <signal.h>
#include <iostream>
#include <fstream>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <thread>
#include <mutex>
// #include <chrono>

TrackingSystem::TrackingSystem(int sock_, bool is_controller_)
		: sock(sock_), is_controller(is_controller_),
			async_listener(false),
			fso(NULL), args(NULL) {}

TrackingSystem::~TrackingSystem() {
	close(sock);
}

void TrackingSystem::run(FSO* fso_, Args* args_) {
	std::cout << "Starting TrackingSystem::run" << std::endl;
	
	fso = fso_;
	args = args_;

	if (is_controller) {
		if (args->do_map_voltage) {
			mapVoltage();	
		} else {
			controllerRun();
		}
	} else {
		if(async_listener) {
			asyncListenerRun();
		} else {
			listenerRun();
		}
	}

	std::cout << "Ending TrackingSystem::run" << std::endl;
}

float TrackingSystem::computeResponse(float p_volt, float n_volt) {
	return args->k_proportional * (n_volt - p_volt);
}

bool controller_loop = false;

void controllerHandler(int s) {
	controller_loop = false;
}

void TrackingSystem::controllerRun() {
	// Assume this side has a gm, and the other side has diodes
	// Set the link of this FSO correctly
	std::string other_rack_id = "", other_fso_id = "";
	get_fso(other_rack_id, other_fso_id);
	fso->setToLink(other_rack_id,other_fso_id);

	std::cout << "TrackingSystem::controllerRun start" << std::endl;
	float ph_volt = 0.0, nh_volt = 0.0, pv_volt = 0.0, nv_volt = 0.0, horizontal_response = 0.0, vertical_response = 0.0;
	int x = 0, every_x_print = 1;

	float init_ph_volt = 0.0, init_nh_volt = 0.0, init_pv_volt = 0.0, init_nv_volt = 0.0;
	get_diode_voltage(init_ph_volt, init_nh_volt, init_pv_volt, init_nv_volt);
	
	struct sigaction sigIntHandler;

	sigIntHandler.sa_handler = controllerHandler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;

	sigaction(SIGINT, &sigIntHandler, NULL);
	controller_loop = true;

	int prev_h_gm = 0, prev_v_gm = 0;
	// std::chrono::time_point<std::chrono::system_clock> start = std::chrono::system_clock::now();
	while(controller_loop) {
		if (x % every_x_print == 0) {
			std::cout << "GM-POS: (" << fso->getHorizontalGMVal() << ", " << fso->getVerticalGMVal() << ")    ";
			std::cout << "RESPONSE: (" << fso->getHorizontalGMVal() - prev_h_gm << ", " << fso->getVerticalGMVal() - prev_v_gm << ")";
			prev_h_gm = fso->getHorizontalGMVal();
			prev_v_gm = fso->getVerticalGMVal();
		}
		// Get diode levels
		get_diode_voltage(ph_volt, nh_volt, pv_volt, nv_volt);

		horizontal_response = computeResponse(ph_volt, nh_volt);
		vertical_response = computeResponse(pv_volt, nv_volt);

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

	// std::chrono::time_point<std::chrono::system_clock> end = std::chrono::system_clock::now();
	// std::chrono::duration<double> dur = end - start;
	float total_seconds = -1.0; // dur.count();
	float seconds_per_test = total_seconds / float(x);

	std::cout << "Avg Iteration time: " << seconds_per_test * 1000 << "millisecond" << std::endl;

	fso->saveCurrentSettings(other_rack_id, other_fso_id);
	fso->save();
	std::cout << "FSO saved!!!" << std::endl;
	
	std::cout << "TrackingSystem::controllerRun end" << std::endl;
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
	std::ofstream ofstr(out_file.c_str(), std::ofstream::out);

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
			std::cout << "HR: " << computeResponse(ph_volt, nh_volt) << "  ";

			std::cout << "VV:(+: " << pv_volt << ", -: " << nv_volt << ")  ";
			std::cout << "VR: " << computeResponse(pv_volt, nv_volt) << std::endl;

			ofstr << horizontal_delta << " " << vertical_delta << " "
					<< ph_volt << " " << nh_volt << " "
					<< pv_volt << " " << nv_volt << " "
					<< computeResponse(ph_volt, nh_volt) << " " << computeResponse(pv_volt, nv_volt) << std::endl;
		}
	}
	ofstr.close();
	end_tracking();

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

std::mutex diode_mtx;
void asyncChildProcess(FSO* fso, float* ph, float* nh, float* pv, float* nv, bool* loop_condition) {
	float tmp_ph = 0.0, tmp_nh = 0.0, tmp_pv = 0.0, tmp_nv = 0.0;
	while(*loop_condition) {
		// Read diode
		tmp_ph = fso->getPositiveHorizontalDiodeVoltage();
		tmp_nh = fso->getNegativeHorizontalDiodeVoltage();
		tmp_pv = fso->getPositiveVerticalDiodeVoltage();
		tmp_nv = fso->getNegativeVerticalDiodeVoltage();

		// Write to shared location
		diode_mtx.lock();
		*ph = tmp_ph;
		*nh = tmp_nh;
		*pv = tmp_pv;
		*nv = tmp_nv;
		diode_mtx.unlock();	
	}
}

void TrackingSystem::asyncListenerRun() {
	std::cout << "TrackingSystem::asyncListenerRun start" << std::endl;

	std::string rack_id = fso->getRackID();
	std::string fso_id = fso->getFSOID();

	// Spawn child thread to constantly read diodes
	bool async_run = true;
	float ph = 0.0, nh = 0.0, pv = 0.0, nv = 0.0;
	float ph_async = 0.0, nh_async = 0.0, pv_async = 0.0, nv_async = 0.0;
	std::thread diode_reader(asyncChildProcess, fso, &ph_async, &nh_async, &pv_async, &nv_async, &async_run);

	// Assume this side has diodes
	std::string msg, token;
	bool listener_loop = true;
	while(listener_loop) {
		recv_msg(msg);
		std::stringstream sstr(msg);

		sstr >> token;
		if(token == "get_fso") {
			std::cout << "B" << std::endl;
			give_fso(rack_id, fso_id);
			std::cout << "C" << std::endl;
		} else if(token == "get_diode_voltage") {
			// Read from the shared memory location, and use those values
			diode_mtx.lock();
			ph = ph_async;
			nh = nh_async;
			pv = pv_async;
			nv = nv_async;
			diode_mtx.unlock();
			
			give_diode_voltage(ph, nh, pv, nv);		
		} else if(token == "end_tracking") {
			listener_loop = false;
			confirm_end_tracking();
		} else {
			std::cerr << "UNEXPECTED MESSAGE: " << msg << std::endl;
		}
	}

	// Kill child thread that is reading the diodes
	async_run = false;
	diode_reader.join();

	std::cout << "TrackingSystem::asyncListenerRun end" << std::endl;
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
	std::stringstream sstr;
	sstr << "give_diode_voltage " << ph_volt << " " << nh_volt << " " << pv_volt << " " << nv_volt;

	std::string message = sstr.str();
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

	std::stringstream sstr;
	sstr << listen_port;
	std::string listen_port_str = sstr.str();
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

	std::stringstream sstr;
	sstr << send_port;
	std::string send_port_str = sstr.str();
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
