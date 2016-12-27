
#include "tracking.h"

#include <signal.h>
#include <iostream>
#include <sstream>
#include <stdio.h>
#include <string.h>

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
		controllerRun();
	} else {
		listenerRun();
	}

	std::cout << "Ending TrackingSystem::run" << std::endl;
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
	float ph_volt = 0.0, nh_volt = 0.0, pv_volt = 0.0, nv_volt = 0.0;
	int x = 0, every_x_print = 10;

	float init_ph_volt = 0.0, init_nh_volt = 0.0, init_pv_volt = 0.0, init_nv_volt = 0.0;
	get_diode_voltage(init_ph_volt, init_nh_volt, init_pv_volt, init_nv_volt);
	
	struct sigaction sigIntHandler;

	sigIntHandler.sa_handler = controllerHandler;
	sigemptyset(&sigIntHandler.sa_mask);
	sigIntHandler.sa_flags = 0;

	sigaction(SIGINT, &sigIntHandler, NULL);
	controller_loop = true;

	int prev_h_gm = 0, prev_v_gm = 0;
	while(controller_loop) {
		if (x % every_x_print == 0) {
			std::cout << "GM-POS: (" << fso->getHorizontalGMVal() << ", " << fso->getVerticalGMVal() << ")    ";
			std::cout << "RESPONSE: (" << fso->getHorizontalGMVal() - prev_h_gm << ", " << fso->getVerticalGMVal() - prev_v_gm << ")" << std::endl;
			prev_h_gm = fso->getHorizontalGMVal();
			prev_v_gm = fso->getVerticalGMVal();
		}
		// Get diode levels
		get_diode_voltage(ph_volt, nh_volt, pv_volt, nv_volt);

		float horizontal_response = args->k_proportional * ((nh_volt - ph_volt));
		float vertical_response = args->k_proportional * ((nv_volt - pv_volt));

		// Change GMs accordingly
		fso->setHorizontalGMVal(int(horizontal_response) + fso->getHorizontalGMVal());
		fso->setVerticalGMVal(int(vertical_response) + fso->getVerticalGMVal());

		x += 1;
		// Record Stats???
	}
	signal(SIGINT, SIG_DFL);

	end_tracking();
	std::cout << "TrackingSystem::controllerRun end" << std::endl;
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
								fso->getPositiveVerticallDiodeVoltage(),
								fso->getNegativeVerticallDiodeVoltage());			
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
	msg = "";
	int buf_len = 64;
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
