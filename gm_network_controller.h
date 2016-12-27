
#ifndef _GM_NETWORK_CONTROLLER_H_
#define _GM_NETWORK_CONTROLLER_H_

#include "args.h"

#include <vector>
#include <map>
#include <string>
#include <time.h>

class GMNetworkController{
public:
	GMNetworkController(Args* args_);
	~GMNetworkController();
	
	void run();

private:
	void send_msg(int sock,const std::string msg) const;
	std::string recv_msg(int sock) const;

	std::string accept_new_connection(int sock) const;
	void get_fso_ids(int sock);

	Args* args;

	int new_con_port;
	int backlog;

	int new_con_sock;
	int controller_sock;
	std::vector<int> child_socks;
	std::map<std::pair<std::string,std::string>,int> child_socks_map;
	std::map<std::pair<std::string,std::string>,time_t> last_power_check;
};

#endif
