
#include "gm_network_controller.h"

#include <iostream>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <arpa/inet.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <sys/time.h>
#include <errno.h>
#include <sstream>

GMNetworkController::GMNetworkController(Args *args_) {
	args = args_;

	new_con_port = 8787;
	new_con_sock = 0;
	controller_sock = 0;
	backlog = 10;
}

GMNetworkController::~GMNetworkController() {
	// Close sockets if any still open
}

void GMNetworkController::send_msg(int sock,const std::string msg) const {
	send(sock,msg.c_str(),msg.size(),0);

	if(args->verbose) {
		std::cout << "Sent: " << msg << std::endl;
	}
}

std::string GMNetworkController::recv_msg(int sock) const {
	std::string msg = "";
	int buf_len = 256;
	char buf[buf_len];

	bool loop = true;
	while(loop) {
		int recv_len = recv(sock,buf,buf_len - 1,0);
		loop = (recv_len == buf_len -1 && buf[recv_len - 1] != '\0');
		buf[recv_len] = '\0';
		msg += buf;
	}

	// temporary to work with telnet
	std::string temp = msg;
	msg = "";
	for(unsigned int i = 0; i < temp.size(); ++i) {
		if(temp.at(i) == '\r' || temp.at(i) == '\n') {
			break;
		} else {
			msg += temp.at(i);
		}
	}

	if(args->verbose) {
		std::cout << "Recv: " << msg << std::endl;
	}

	return msg;
}

void GMNetworkController::run() {
	fd_set readfds;
	int max_sd, activity;

	new_con_sock = socket(AF_INET, SOCK_STREAM,0);
	if(new_con_sock == 0) {
		std::cerr << "Could not create socket" << std::endl;
		exit(1);
	}

	int opt = 1;
	int rc = setsockopt(new_con_sock, SOL_SOCKET, SO_REUSEADDR, (char *)&opt, sizeof(opt));
	if(rc < 0) {
		std::cerr << "Unable to set socket options" << std::endl;
		exit(1);
	}

	struct sockaddr_in addr;

	addr.sin_family = AF_INET;
	addr.sin_addr.s_addr = INADDR_ANY;
	addr.sin_port = htons(new_con_port);

	rc = bind(new_con_sock,(struct sockaddr*) &addr, sizeof(addr));
	if(rc < 0) {
		std::cerr << "Couldn't bind socket to port" << std::endl;
		exit(1);
	}

	int addr_len = sizeof(addr);

	rc = listen(new_con_sock,backlog);
	if(rc < 0) {
		std::cerr << "Could not listen on port" << std::endl;
		exit(1);
	}

	// Timoeout settings
	int sec_to = 1;
	int usec_to = 0;
	struct timeval timeout;

	bool loop = true;
	while(loop) {
		// Reinit timeout
		timeout.tv_sec = sec_to;
		timeout.tv_usec = usec_to;

		FD_ZERO(&readfds);

		FD_SET(new_con_sock,&readfds);
		max_sd = new_con_sock;

		if(controller_sock > 0) {
			FD_SET(controller_sock,&readfds);
			if(max_sd < controller_sock) {
				max_sd = controller_sock;
			}
		}

		for(unsigned int i = 0; i < child_socks.size(); ++i) {
			if(child_socks[i] > 0) {
				FD_SET(child_socks[i],&readfds);
				if(child_socks[i] > max_sd) {
					max_sd = child_socks[i];
				}
			}
		}

		activity = select(max_sd + 1, &readfds, NULL, NULL, &timeout);

		if(activity < 0 && errno != EINTR) {
			std::cerr << "Error when selecting sockets" << std::endl;
		}

		// Check for new connections
		if(FD_ISSET(new_con_sock,&readfds)) {
			int new_sock = accept(new_con_sock, (struct sockaddr*) &addr,(socklen_t*) &addr_len);
			if(new_sock < 0) {
				std::cerr << "Coulnd't accept connection" << std::endl;
				exit(1);
			}
			std::string connection_type = accept_new_connection(new_sock);
			if(connection_type == "gm_server") {
				child_socks.push_back(new_sock);
				get_fso_ids(new_sock);
			} else if(connection_type == "network_controller") {
				controller_sock = new_sock;
			} else {
				close(new_sock);
			}
		}

		// Check if new network configuration
		if(controller_sock > 0 && FD_ISSET(controller_sock,&readfds)) {
			// Get new network state, and then send child_sock appropriate commands
		}

		// Check child socks for anything
		for(unsigned int i = 0; i < child_socks.size(); ++i) {
			if(child_socks[i] > 0 && FD_ISSET(child_socks[i],&readfds)) {
				// Message from GM Server
			} 
		}

		// Check when the last time the power was checked, and then if its high enough then send a request to check power and if too low then do auto alignment
		for(std::map<std::pair<std::string,std::string>,time_t>::iterator itr = last_power_check.begin(); itr != last_power_check.end(); ++itr) {
			std::cout << difftime(itr->second,time(NULL)) << std::endl;
		}
	}
}

// After accepting new connection to A
// S: 'type?'
// A: 'gm_server' | 'network_controller'
// if 'gm_server' or ('network_controller' and controller_sock == 0) then S: 'added'
// else S: 'rejected'
std::string GMNetworkController::accept_new_connection(int sock) const {
	// Connection just accepted
	send_msg(sock,"type?");
	std::string type = recv_msg(sock);
	if(type == "gm_server" || (type == "network_controller" && controller_sock == 0)) {
		send_msg(sock,"added");
		return type;
	} else {
		send_msg(sock, "rejected");
		return "connection_rejected";
	}
	return "none";
}

// After adding GMServer A get the ids of its fsos
// S: 'fsos?'
// A: 'X rack_id1 fso_id1 rack_id2 fso_id2 ... rack_idX fso_idX'
void GMNetworkController::get_fso_ids(int sock) {
	// Request FSO IDs
	send_msg(sock,"fsos?");
	std::string fso_msg = recv_msg(sock);

	// Process Response
	std::stringstream sstr(fso_msg);
	std::string rack_id,fso_id;
	int num_fsos = 0;
	sstr >> num_fsos;

	for(int x = 0; x < num_fsos; ++x) {
		rack_id = "";
		fso_id = "";
		sstr >> rack_id >> fso_id;
		child_socks_map[std::make_pair(rack_id,fso_id)] = sock;
		last_power_check[std::make_pair(rack_id,fso_id)] = time(NULL);
	}
}
