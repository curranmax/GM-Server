
#include "half_auto_align.h"

#include <iostream>
#include <sstream>
#include <stdio.h>
#include <string.h>
#include <map>

HalfAutoAlign::HalfAutoAlign(int sock_, bool is_controller_)
		: sock(sock_), is_controller(is_controller_),
			fso(NULL), args(NULL) {}

HalfAutoAlign::~HalfAutoAlign() {
	close(sock);
}

void HalfAutoAlign::run(FSO* fso_, Args* args_) {
	std::cout << "Starting HalfAutoAlign::run" << std::endl;
	fso = fso_;
	args = args_;
	if(is_controller) {
		controllerRun();
	} else {
		listenerRun();
	}
	std::cout << "Ending HalfAutoAlign::run" << std::endl;
}

void HalfAutoAlign::controllerRun() {
	give_fso(fso->getRackID(), fso->getFSOID());

	std::map<std::pair<int, int>, float> power_map;

	// Outer loop controlled by user
	int range = 0, step = 0;
	while(true) {
		std::string token = "";
		std::cout << "Run another iteration? ([y]es/[n]o/[r]edo with same parameters): ";
		std::cin >> token;
		if(token == "y" || token == "r") {
			if (token == "y") {
				std::cout << "Input (range) and (step): ";
				std::cin >> range >> step;
			}
			// Assume that range and step are both >= 1

			// Automatic Inner Loop (TODO add automatic refinement)
			power_map.clear();
			
			set_to_this_link();
			
			int init_hgm_val = 0, init_vgm_val = 0, this_hgm_val = -1, this_vgm_val = -1;
			get_gm_vals(init_hgm_val, init_vgm_val);

			float this_power = -1.0;
			// Try ALL combiniations and record the diode's output
			for(int hgm_delta = -range; hgm_delta <= range; hgm_delta += step) {
				for(int vgm_delta = -range; vgm_delta <= range; vgm_delta += step) {
					this_hgm_val = init_hgm_val + hgm_delta;
					this_vgm_val = init_vgm_val + vgm_delta;
					set_gm_vals(this_hgm_val, this_vgm_val);

					// read power
					this_power = fso->getPowerDiodeVoltage();
					// std::cout << fso->getPositiveHorizontalDiodeVoltage() << " " << fso->getNegativeHorizontalDiodeVoltage() << " " << fso->getPositiveVerticalDiodeVoltage() << " " << fso->getNegativeVerticalDiodeVoltage() << std::endl;
					// std::cout << "(H: " << this_hgm_val << " (" << hgm_delta << "), V: " << this_vgm_val << " (" << vgm_delta << ")) -> " << this_power << " volts" << std::endl; 

					power_map[std::make_pair(this_hgm_val, this_vgm_val)] = this_power;
				}
			}

			// Go through power_map and find the maximum voltage
			float max_power = -1.0;
			int max_hgm_val = -1.0, max_vgm_val = -1.0;
			bool first = true;
			for(std::map<std::pair<int, int>, float>::iterator itr = power_map.begin(); itr != power_map.end(); ++itr) {
				if(first || max_power < itr->second) {
					first = false;
					max_hgm_val = itr->first.first;
					max_vgm_val = itr->first.second;
					max_power = itr->second;
				}
			}

			set_gm_vals(max_hgm_val, max_vgm_val);
			save_link();
			set_to_this_link();

			std::cout << "Max Power achieved with (H: " << max_hgm_val << " (" << max_hgm_val - init_hgm_val << "), V: " << max_vgm_val << " (" << max_vgm_val - init_vgm_val << ")) -> " << max_power << " volts" << std::endl; 
		} else {
			break;
		}
	}

	end_half_auto_align();
}

void HalfAutoAlign::listenerRun() {
	std::string other_rack_id = "", other_fso_id = "";

	std::string msg, token;
	bool listener_loop = true;
	while (listener_loop) {
		// Wait for message from controller, then react and respond
		// TODO better error handling
		recv_msg(msg);
		std::stringstream sstr(msg);
		sstr >> token;
		if(token == "give_fso") {
			sstr >> other_rack_id >> other_fso_id;
			confirm_got_fso();
		} else if(token == "set_to_this_link") {
			if (other_rack_id != "" and other_fso_id != "") {
				fso->setToLink(other_rack_id, other_fso_id);
			}
			confirm_set_to_this_link();
		} else if(token == "get_gm_vals") {
			give_gm_vals(fso->getHorizontalGMVal(), fso->getVerticalGMVal());
		} else if(token == "set_gm_vals") {
			int hgm_val = 0, vgm_val = 0;
			sstr >> hgm_val >> vgm_val;
			fso->setHorizontalGMVal(hgm_val);
			fso->setVerticalGMVal(vgm_val);
			confirm_set_gm_vals();
		} else if(token == "save_link") {
			if (other_rack_id != "" and other_fso_id != "") {
				fso->saveCurrentSettings(other_rack_id, other_fso_id);
			}
			confirm_save_link();
		} else if(token == "end_half_auto_align") {
			listener_loop = false;
			confirm_end_half_auto_align();
		} else {
			std::cerr << "UNEXPECTED MSG: '" << msg << "'" << std::endl;
		}
	}
}

void HalfAutoAlign::send_msg(const std::string &msg) {
	send(sock, msg.c_str(), msg.size(), 0);
}

void HalfAutoAlign::recv_msg(std::string &msg) {
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

// Controller Helper functions
void HalfAutoAlign::give_fso(const std::string& this_rack_id, const std::string& this_fso_id) {
	std::string message = "give_fso " + this_rack_id + " " + this_fso_id;
	send_msg(message);

	std::string return_message = "";
	recv_msg(return_message);

	if(return_message != "confirm_got_fso") {
		std::cerr << "Error in 'give_fso' response: '" << return_message << "'" << std::endl;
	}
}

void HalfAutoAlign::set_to_this_link() {
	send_msg("set_to_this_link");

	std::string return_message = "";
	recv_msg(return_message);

	if(return_message != "confirm_set_to_this_link") {
		std::cerr << "Error in 'set_to_this_link' response: '" << return_message << "'" << std::endl;
	}
}

void HalfAutoAlign::get_gm_vals(int &horizontal_gm_value, int &vertical_gm_value) {
	horizontal_gm_value = 0;
	vertical_gm_value = 0;
	
	send_msg("get_gm_vals");

	std::string return_message = "";
	recv_msg(return_message);

	std::stringstream sstr(return_message);
	std::string token = "";
	sstr >> token;

	if(token != "give_gm_vals") {
		std::cerr << "Error in 'give_gm_vals' response: '" << return_message << "'" << std::endl;
		return;
	}

	sstr >> horizontal_gm_value >> vertical_gm_value;
}

void HalfAutoAlign::set_gm_vals(int horizontal_gm_value, int vertical_gm_value) {
	std::stringstream sstr;
	sstr << "set_gm_vals " << horizontal_gm_value << " " << vertical_gm_value;
	std::string message = sstr.str();
	send_msg(message);

	std::string return_message = "";
	recv_msg(return_message);

	if(return_message != "confirm_set_gm_vals") {
		std::cerr << "Error in 'set_gm_vals' response: '" << return_message << "'" << std::endl;
	}
}

void HalfAutoAlign::save_link() {
	send_msg("save_link");

	std::string return_message = "";
	recv_msg(return_message);

	if(return_message != "confirm_save_link") {
		std::cerr << "Error in 'save_link' response: '" << return_message << "'" << std::endl;
	}
}

void HalfAutoAlign::end_half_auto_align() {
	send_msg("end_half_auto_align");

	std::string return_message = "";
	recv_msg(return_message);

	if(return_message != "confirm_end_half_auto_align") {
		std::cerr << "Error in 'end_half_auto_align' response: '" << return_message << "'" << std::endl;
	}
}

// Listener Helper functions
void HalfAutoAlign::confirm_got_fso() {
	send_msg("confirm_got_fso");
}

void HalfAutoAlign::confirm_set_to_this_link() {
	send_msg("confirm_set_to_this_link");
}

void HalfAutoAlign::give_gm_vals(int horizontal_gm_value, int vertical_gm_value) {
	std::stringstream sstr;
	sstr << "give_gm_vals " << horizontal_gm_value << " " << vertical_gm_value;
	send_msg(sstr.str());
}

void HalfAutoAlign::confirm_set_gm_vals() {
	send_msg("confirm_set_gm_vals");
}

void HalfAutoAlign::confirm_save_link() {
	send_msg("confirm_save_link");
}

void HalfAutoAlign::confirm_end_half_auto_align() {
	send_msg("confirm_end_half_auto_align");
}

HalfAutoAlign* HalfAutoAlign::listenFor(int listen_port,const std::string &rack_id,const std::string &fso_id) {
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
		return new HalfAutoAlign(connect_sock, false);
	} else {
		close(connect_sock);
		return NULL;
	}
}

HalfAutoAlign* HalfAutoAlign::connectTo(int send_port,const std::string &host_addr,const std::string &rack_id,const std::string &fso_id) {
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
		return new HalfAutoAlign(sock,true);
	} else {
		close(sock);
		return NULL;
	}
}

