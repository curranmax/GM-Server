
#include "gm_server.h"

#include <iostream>
#include <sstream>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <netdb.h>
#include <unistd.h>

GMServer::~GMServer() {
	// Close sockets if open
}

void GMServer::send_msg(int sock,const std::string msg) const {
	send(sock,msg.c_str(),msg.size(),0);
	if(args->verbose) {
		std::cout << "Sent: " << msg << std::endl;
	}
}

std::string GMServer::recv_msg(int sock) const {
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

	if(args->verbose) {
		std::cout << "Recv: " << msg << std::endl;
	}

	return msg;
}

void GMServer::run() {
	// Get these values from args or something
	server_address = "localhost";
	parent_port = 8787;

	bool is_connected = connect_to_gm_network_controller();
	if(!is_connected) {
		std::cerr << "Unable to connect to GM Network Controller" << std::endl;
		return;
	}

	// Listen for updates to active links and requests to check power
	while(true) {
		recv_msg(parent_sock);
	}

}

// Connects to the GM Network controller
bool GMServer::connect_to_gm_network_controller() {
	struct sockaddr_in serv_addr;
	struct hostent *server;

	// Create socket
	parent_sock = socket(AF_INET,SOCK_STREAM,0);
	if(parent_sock < 0) {
		return false;
	}

	server = gethostbyname(server_address.c_str());
	if(server == NULL) {
		std::cerr << "ERROR!!!" << std::endl;
		return false;
	}

	bzero((char*) &serv_addr, sizeof(serv_addr));
	serv_addr.sin_family = AF_INET;
	bcopy((char*) server->h_addr,(char*) &serv_addr.sin_addr.s_addr,server->h_length);
	serv_addr.sin_port = htons(parent_port);
	
	// Connect to server
	int rc = connect(parent_sock,(struct sockaddr*) &serv_addr,sizeof(serv_addr));
	if(rc < 0) {
		std::cerr << "Could not connect to server" << std::endl;
		return false;
	}

	std::string rmsg = recv_msg(parent_sock);
	if(rmsg != "type?") {
		std::cerr << "Invalid server connected to" << std::endl;
		return false;
	}
	send_msg(parent_sock,"gm_server");
	rmsg = recv_msg(parent_sock);
	if(rmsg != "added") {
		std::cerr << "Not added to GM network controller" << std::endl;
		return false;
	}

	// Send FSO information
	rmsg = recv_msg(parent_sock);
	if(rmsg != "fsos?") {
		std::cerr << "Not asked for FSOs" << std::endl;
		return false;
	}
	std::stringstream sstr;
	sstr << fsos.size();
	for(unsigned int i = 0; i < fsos.size(); ++i) {
		sstr << " " << fsos[i]->getRackID() << " " << fsos[i]->getFSOID();
	}
	send_msg(parent_sock,sstr.str());	

	return true;
}
