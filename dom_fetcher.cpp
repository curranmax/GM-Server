
#include "dom_fetcher.h"

#include <iostream>
#include <sstream>
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
#include <map>
#include <libssh/libssh.h>
#include <limits>
#include <unistd.h>

#define COMMAND_STR "\r\nSwitch>"
#define PWR_CMD "show interface transceiver detail\r" // show interfaces [interface-id | vlan vlan-id] transceiver [properties | detail] [module number] [ | {begin | exclude | include} expression]
#define EMPTY_LINE ""
#define RCV_PWR_NAME "Optical Receive Power (dBm)"

#define BUF_SIZE 256
#define MORE_STR " --More-- "

bool endsWith2(const std::string &full_string,const std::string &suffix) {
	return full_string.length() >= suffix.length() && (0 == full_string.compare(full_string.length() - suffix.length(),suffix.length(),suffix));
}

void replaceReturns(std::string &str) {
	std::string new_string = "";
	for(unsigned int i = 0 ; i < str.size(); ++i) {
		if('\r' == str.at(i)) {
		} else if('\n' == str.at(i)) {
			new_string += str.at(i);
		} else {
			new_string += str.at(i);
		}
	}
	str = new_string;
}

// Removes leading spaces and backspaces
void trimLeadingBackSpaces(std::string &str) {
	str = str.substr(str.find_last_of(char(8)) + 1);
}

float DOMFetcher::getPower(const std::string &port) const {
	if(!is_connected) {
		return -40.0;
	}

	std::string command = PWR_CMD;

	std::string response = execCommand(command);
	replaceReturns(response);
	
	std::stringstream sstr(response);
	std::string line;
	bool prefix = true;
	bool column_header = true;
	std::vector<std::string> column_names;
	std::vector<int> column_locs;

	std::map<std::string,std::map<std::string,float> > diagnostic_vals;

	// Parse output from switch
	while(std::getline(sstr,line)) {
		trimLeadingBackSpaces(line);
		if(prefix) {
			prefix = (line != EMPTY_LINE);
		} else {
			if(column_header) {
				if(line == EMPTY_LINE) {
					continue;
				}
				if(line.at(0) == '-') {
					column_header = false;
					continue;
				}
				// Parse Column Headers
				std::string token = "";
				int start_loc = -1;
				bool last_space = false;
				for(unsigned int i = 0; i < line.size(); ++i) {
					if(line.at(i) == ' ' || line.at(i) == '\r') {
						if(token != "") {
							if(!last_space && line.at(i) == ' ') {
								last_space = true;
								continue;
							}
							bool found = false;
							for(unsigned int j = 0; j < column_locs.size(); ++j) {
								if(abs(start_loc - column_locs[j]) <= 2) {
									found = true;
									column_names[j] += " " + token;
									break;
								}
							}
							if(!found) {
								found = false;
								for(unsigned int j = 0; j < column_locs.size(); ++j) {
									if(start_loc < column_locs[j]) {
										found = true;
										column_locs.insert(column_locs.begin() + j,start_loc);
										column_names.insert(column_names.begin() + j,token);
										break;
									}
								}
								if(!found) {
									column_locs.push_back(start_loc);
									column_names.push_back(token);
								}
							}
							token = "";
							start_loc = -1;
							last_space = false;
						}
					} else {
						if(start_loc < 0) {
							start_loc = i;
						}
						if(last_space) {
							token += ' ';
							last_space = false;
						}
						token += line.at(i);
					}
				}
			} else {
				if(line == EMPTY_LINE) {
					column_header = true;
					column_names.clear();
					column_locs.clear();
					continue;
				}

				std::stringstream vals(line);
				std::string p;
				vals >> p;

				float v;
				for(unsigned int i = 1; i < column_names.size(); ++i) {
					vals >> v;
					diagnostic_vals[p][(i == 1 ? column_names[i] : column_names[1] + " " + column_names[i])] = v;
				}
			}
		}
	}

	if(diagnostic_vals.count(port) > 0 && diagnostic_vals[port].count(RCV_PWR_NAME) > 0) {
		float pwr_val = diagnostic_vals[port][RCV_PWR_NAME];
		// Maybe return -INF instead
		return pwr_val;
	} else {
		return -20.0;
	}
}

std::string TelnetFetcher::execCommand(const std::string &command) const {
	if(!is_connected) {
		return "";
	}
	send_msg(command);
	// the switch sends the command back one character at a time
	for(unsigned int i = 0; i < command.size(); ++i) {
		recv_msg();
	}

	// The output can be accross different messages, so must combine until it ends with \r\nSwitch> then command
	std::string output = "";
	while(!endsWith2(output,COMMAND_STR)) {
		output += recv_msg();
	}

	// Remove the end part with the switch
	output = output.substr(0,output.length() - strlen(COMMAND_STR));
	
	return output;
}

void TelnetFetcher::connect_to_switch() {
	struct addrinfo hints, *serv_info, *p;

	memset(&hints,0,sizeof(hints));
	hints.ai_family = AF_UNSPEC;
	hints.ai_socktype = SOCK_STREAM;

	std::string send_port_str = "23";
	int rv = getaddrinfo(host.c_str(),send_port_str.c_str(),&hints,&serv_info);
	if(rv != 0) {
		std::cerr << "Error in getaddrinfo: " << gai_strerror(rv) << std::endl;
		return;
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
			std::cout << sock << " " << p->ai_addr << std::endl;
			close(sock);
			std::cerr << "Error when connecting" << std::endl;
			continue;
		}
		break;
	}

	if(p == NULL) {
		std::cerr << "Could not connect to foreign host" << std::endl;
		return;
	}

	freeaddrinfo(serv_info);

	// Messages used to setup telnet session with switch
	// Not sure if this will be true for all switches that we use
	// Check received messages to make sure they match what is expected
	char msg1[] = {(char)0xff,(char)0xfd,(char)0x03,(char)0xff,(char)0xfb,(char)0x18,(char)0xff,(char)0xfb,(char)0x1f,(char)0xff,(char)0xfb,(char)0x20,(char)0xff,(char)0xfb,(char)0x21,(char)0xff,(char)0xfb,(char)0x22,(char)0xff,(char)0xfb,(char)0x27,(char)0xff,(char)0xfd,(char)0x05,(char)0xff,(char)0xfb,(char)0x23,'\0'};
	char msg2[] = {(char)0xff,(char)0xfd,(char)0x01,(char)0xff,(char)0xfa,(char)0x1f,(char)0x00,(char)0x66,(char)0x00,(char)0x39,(char)0xff,(char)0xf0,'\0'};
	char msg3[] = {(char)0xff,(char)0xfa,(char)0x18,(char)0x00,(char)0x78,(char)0x74,(char)0x65,(char)0x72,(char)0x6d,(char)0xff,(char)0xf0,'\0'};
	send_msg(msg1);
	recv_msg();
	send_msg(msg2,12);
	recv_msg();
	recv_msg();
	send_msg(msg3,11);
	recv_msg();
	recv_msg();
	recv_msg();
	recv_msg();
	recv_msg();
	recv_msg();
	send_msg(password + "\r");
	recv_msg();

	is_connected = true;
}

void TelnetFetcher::disconnect_from_switch() {
	send_msg("quit\r");
	is_connected = false;
}

void TelnetFetcher::send_msg(const std::string msg) const {
	if(!is_connected) {
		return;
	}
	send(sock,msg.c_str(),msg.size(),0);
	if(args != NULL && args->verbose) {
		std::cout << "Sent: " << msg << std::endl;
	}
}

void TelnetFetcher::send_msg(char *msg, int msg_len) const {
	if(!is_connected) {
		return;
	}
	send(sock,msg,msg_len,0);
	if(args != NULL && args->verbose) {
		std::cout << "Sent: " << msg << std::endl;
	}
}

std::string TelnetFetcher::recv_msg() const {
	if(!is_connected) {
		return "";
	}
	std::string msg = "";
	int buf_len = 64;
	char buf[buf_len];

	bool loop = true;
	while(loop) {
		int recv_len = recv(sock,buf,buf_len - 1,0);
		loop = (recv_len == buf_len -1 && buf[recv_len - 1] != '\0');
		buf[recv_len] = '\0';
		msg += buf;
	}

	if(args != NULL && args->verbose) {
		std::cout << "Recv: " << msg << std::endl;
	}
	return msg;
}

void SSHFetcher::connect_to_switch() {
	if(!is_connected) {
		session = ssh_new();

		int verbosity = SSH_LOG_NOLOG;
		int port = 22;

		// Set options
		ssh_options_set(session, SSH_OPTIONS_HOST, host.c_str());
		ssh_options_set(session, SSH_OPTIONS_PORT, &port);
		ssh_options_set(session, SSH_OPTIONS_USER, user_name.c_str());
		ssh_options_set(session, SSH_OPTIONS_LOG_VERBOSITY, &verbosity);

		// Connect
		int rc = ssh_connect(session);
		if(rc != SSH_OK) {
			std::cerr << "Could not connect to SSH server" << std::endl;
			disconnect_from_switch();
			return;
		}
		is_connected = true;

		// Check that server is known
		// TODO handle other cases: see http://api.libssh.org/master/libssh_tutor_guided_tour.html
		int state = ssh_is_server_known(session);
		if(state != SSH_SERVER_KNOWN_OK) {
			std::cerr << "Problem with authenticating server" << std::endl;
			disconnect_from_switch();
			return;
		}

		// Send password for authentication
		rc = ssh_userauth_password(session, NULL, password.c_str());
		if(rc != SSH_AUTH_SUCCESS) {
			std::cerr << "Problem with authenticating with server" << std::endl;
			disconnect_from_switch();
			return;
		}

		open_shell();

	}
	if(args->verbose && is_connected) {
		std::cout << "SSH Connected" << std::endl;
	}
}

void SSHFetcher::disconnect_from_switch() {
	close_shell();
	if(is_connected) {
		ssh_disconnect(session);
		is_connected = false;
	}
	if(session != NULL) {
		ssh_free(session);
		session = NULL;
	}
}

void SSHFetcher::open_shell() {
	if(session == NULL || !is_connected) {
		return;
	}
	channel = ssh_channel_new(session);
	if(channel == NULL) {
		return;
	}

	int rc = ssh_channel_open_session(channel);
	if(rc != SSH_OK) {
		ssh_channel_free(channel);
		channel = NULL;
		return;
	}

	rc = ssh_channel_request_shell(channel);
	if(rc != SSH_OK) {
		ssh_channel_close(channel);
		ssh_channel_free(channel);
		channel = NULL;
		return;
	}

	get_shell_label();
}

void SSHFetcher::close_shell() {
	if(ssh_channel_is_open(channel)) {
		ssh_channel_close(channel);
	}
	if(channel != NULL) {
		ssh_channel_free(channel);
		channel = NULL;
	}
}

std::string SSHFetcher::execCommand(const std::string &command) const {
	if(channel == NULL || !ssh_channel_is_open(channel)) {
		return "";
	}

	int nwritten = ssh_channel_write(channel,command.c_str(),command.size());
	if(nwritten != (int)command.size()) {
		return "";
	}

	std::string result = "";
	char buf[BUF_SIZE];
	int nbytes = ssh_channel_read(channel,buf,BUF_SIZE - 1, 0);
	while(nbytes > 0) {
		buf[nbytes] = '\0';
		result += buf;
		if(endsWith2(result,shell_label)) {
			break;
		}
		// Find a way to display more in the "shell", b/c the back and forth takes longer
		if(endsWith2(result,MORE_STR)) {
			nwritten = ssh_channel_write(channel,"\r",1);
			if(nwritten != 1) {
				return "";
			}
			result = result.substr(0,result.size() - strlen(MORE_STR));
		}
		nbytes = ssh_channel_read(channel,buf,BUF_SIZE - 1, 0);
	}

	if(nbytes < 0) {
		return "";
	}

	trim_response(result,command);

	return result;
}

void SSHFetcher::get_shell_label() {
	shell_label = "";
	char buf[BUF_SIZE];
	int nbytes = ssh_channel_read(channel,buf,BUF_SIZE - 1, 0);
	while(nbytes > 0) {
		buf[nbytes] = '\0';
		shell_label += buf;
		if(endsWith2(shell_label,">")) {
			break;
		}
		nbytes = ssh_channel_read(channel,buf,BUF_SIZE - 1, 0);
	}
}

void SSHFetcher::trim_response(std::string &resp,const std::string &command) const {
	resp = resp.substr(command.size() + 1, resp.size() - shell_label.size() - command.size());
}

float FakeFetcher::getPower(const std::string &port) const {
	usleep(700000); // Wait .7 seconds
	float power_value = float(rand() % 41 - 50);
	if(args->verbose) {
		// std::cout << "getPower(): " << power_value << std::endl;
	}
	return power_value;
}

void FakeFetcher::connect_to_switch() {
	if(args->verbose) {
		std::cout << "connect_to_switch()" << std::endl;
	}
	is_connected = true;
}

void FakeFetcher::disconnect_from_switch() {
	if(args->verbose) {
		std::cout << "disconnect_to_switch()" << std::endl;
	}
	is_connected = false;
}

std::string FakeFetcher::execCommand(const std::string &command) const {
	if(args->verbose) {
		std::cerr << "execCommand() should not be called on FakeFetcher" << std::endl;
	}
	return "";
}
