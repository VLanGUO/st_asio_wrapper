
//configuration
#define SERVER_PORT		9527
#define REUSE_CLIENT //use objects pool
#define FORCE_TO_USE_MSG_RECV_BUFFER //force to use the msg recv buffer
#define ENHANCED_STABILITY
//configuration

#include "../include/st_asio_wrapper_server.h"
using namespace st_asio_wrapper;

#define QUIT_COMMAND	"quit"
#define RESTART_COMMAND	"restart"
#define LIST_ALL_CLIENT	"list_all_client"
#define LIST_STATUS		"status"

//demonstrates how to use custom packer
//in the default behavior, each st_tcp_socket has their own packer, and cause memory waste
//at here, we make each echo_socket use the same global packer for memory saving
//notice: do not do this for unpacker, because unpacker has member variables and can't share each other
auto global_packer(boost::make_shared<packer>());

//demonstrates how to control the type of st_server_socket_base::server from template parameters
class i_echo_server : public i_server
{
public:
	virtual void test() = 0;
};

class echo_socket : public st_server_socket_base<i_echo_server>
{
public:
	echo_socket(i_echo_server& server_) : st_server_socket_base(server_) {inner_packer(global_packer);}

public:
	//because we use objects pool(REUSE_CLIENT been defined), so, strictly speaking, this virtual
	//function must be rewrote, but we don't have member variables to initialize but invoke father's
	//reuse() directly, so, it can be omitted, but we keep it for possibly future using
	virtual void reuse() {st_server_socket_base::reuse();}

protected:
	virtual void on_recv_error(const error_code& ec)
	{
		//the type of st_server_socket_base::server now can be controled by derived class(echo_socket),
		//which is actually i_echo_server, so, we can invoke i_echo_server::test virtual function.
		server.test();
		st_server_socket_base::on_recv_error(ec);
	}

	//msg handling: send the original msg back(echo server)
#ifndef FORCE_TO_USE_MSG_RECV_BUFFER
	//this virtual function doesn't exists if FORCE_TO_USE_MSG_RECV_BUFFER been defined
	virtual bool on_msg(msg_type& msg) {send_msg(msg, true); return false;}
#endif
	//we should handle the msg in on_msg_handle for time-consuming task like this:
	virtual void on_msg_handle(msg_type& msg) {send_msg(msg, true);}
	//please remember that we have defined FORCE_TO_USE_MSG_RECV_BUFFER, so, st_tcp_socket will directly
	//use the msg recv buffer, and we need not rewrite on_msg(), which doesn't exists any more
	//msg handling end
};

class echo_server : public st_server_base<echo_socket, i_echo_server>
{
public:
	echo_server(st_service_pump& service_pump_) : st_server_base(service_pump_) {}

	//from i_echo_server, pure virtual function, we must implement it.
	virtual void test() {puts("in echo_server::test()");}
};

int main() {
	puts("type quit to end these two servers.");

	std::string str;
	st_service_pump service_pump;
	st_server server_(service_pump); //only need a simple server? you can directly use st_server
	server_.set_server_addr(SERVER_PORT + 100);
	echo_server echo_server_(service_pump); //echo server

	service_pump.start_service();
	while(service_pump.is_running())
	{
		std::cin >> str;
		if (str == QUIT_COMMAND)
			service_pump.stop_service();
		else if (str == RESTART_COMMAND)
		{
			service_pump.stop_service();
			service_pump.start_service();
		}
		else if (str == LIST_STATUS)
		{
			printf("normal server:\nvalid links: " size_t_format ", closed links: " size_t_format "\n",
				server_.size(), server_.closed_client_size());

			printf("echo server:\nvalid links: " size_t_format ", closed links: " size_t_format "\n",
				echo_server_.size(), echo_server_.closed_client_size());
		}
		else if (str == LIST_ALL_CLIENT)
			server_.list_all_client();
		else
			server_.broadcast_msg(str);
	}

	return 0;
}

//restore configuration
#undef SERVER_PORT
#undef REUSE_CLIENT //use objects pool
#undef FORCE_TO_USE_MSG_RECV_BUFFER //force to use the msg recv buffer
#undef ENHANCED_STABILITY
//restore configuration
