
#ifndef _GM_SERVER_H_
#define _GM_SERVER_H_

#include "args.h"
#include "FSO.h"

#include <vector>
#include <string>

class GMServer {
public:
	GMServer(const std::vector<FSO*> fsos_, Args* args_) : args(args_),parent_sock(0),fsos(fsos_),parent_port(0),server_address("") {}
	~GMServer();

	void run();

private:
	void send_msg(int sock,const std::string msg) const;
	std::string recv_msg(int sock) const;

	bool connect_to_gm_network_controller();

	Args* args;

	int parent_sock;
	std::vector<FSO*> fsos;

	int parent_port;
	std::string server_address;
};

#endif
