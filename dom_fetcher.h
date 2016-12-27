
#ifndef _DOM_FETCHER_H_
#define _DOM_FETCHER_H_

#include "args.h"

#include <string>
#include <libssh/libssh.h>

class DOMFetcher {
public:
	DOMFetcher(Args* args_) : is_connected(false),args(args_) {}
	virtual ~DOMFetcher() {}

	bool isConnected() const { return is_connected; }

	virtual float getPower(const std::string &port) const;
	virtual void connect_to_switch() = 0;
	virtual void disconnect_from_switch() = 0; 
protected:
	virtual std::string execCommand(const std::string &command) const = 0;

	bool is_connected;
	Args *args;

};

class TelnetFetcher : public DOMFetcher {
public:
	TelnetFetcher(const std::string &host_,Args *args_) : DOMFetcher(args_),host(host_),password("trinitron"),sock(-1) {}
	~TelnetFetcher() { if(is_connected) { disconnect_from_switch(); }}
	
	void connect_to_switch();
	void disconnect_from_switch();
	
protected:
	std::string execCommand(const std::string &command) const;

private:

	void send_msg(const std::string msg) const;
	void send_msg(char *msg, int msg_len) const;
	std::string recv_msg() const;

	std::string host,password;
	int sock;
};

class SSHFetcher : public DOMFetcher {
public:
	SSHFetcher(const std::string &host_,const std::string user_name_,const std::string password_,Args* args_) : DOMFetcher(args_),host(host_),user_name(user_name_),password(password_),session(NULL),channel(NULL),shell_label("") {}
	~SSHFetcher() { if(is_connected) { disconnect_from_switch(); }}

	void connect_to_switch();
	void disconnect_from_switch();

protected:
	std::string execCommand(const std::string &command) const;

private:
	void open_shell();
	void close_shell();
	void get_shell_label();
	void trim_response(std::string &resp,const std::string &command) const;

	std::string host,user_name,password;
	ssh_session session;
	ssh_channel channel;
	std::string shell_label;

};

// NULL Subclass
class FakeFetcher : public DOMFetcher {
public:
	FakeFetcher(Args* args_) : DOMFetcher(args_) {}
	~FakeFetcher() { if(is_connected) { disconnect_from_switch(); }}

	float getPower(const std::string &port) const;
	void connect_to_switch();
	void disconnect_from_switch();

protected:
	std::string execCommand(const std::string &command) const;
};

#endif
