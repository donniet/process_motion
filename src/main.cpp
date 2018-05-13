#include "power.h"

#include <boost/program_options.hpp>
#include <boost/asio.hpp>

#include <iostream>
#include <string>
#include <chrono>

#include <math.h>

using std::literals::chrono_literals;

using namespace boost::program_options;
using namespace boost::asio;
using std::string;
using std::cin;
using std::cout;
using std::cerr;
using std::endl;


struct motion_vector {
	signed char x_vector;
	signed char y_vector;
	short sad;
};

int mbx = 120;
int mby = 68;
int magnitude = 60;
int total = 10;
string remote = "127.0.0.1";
int remote_port = 5555;
bool test = false;
ip::udp::endpoint remote_endpoint;

struct motion_detect_packet {
	int magnitude;
};

void send_motion_detect(ip::udp::socket & sock, int magnitude) {
	cout << "sending to " << remote << " on port " << remote_port << endl;

	boost::system::error_code err;
	auto packet = motion_detect_packet{magnitude};

	sock.send_to(buffer((void *)&packet, sizeof(motion_detect_packet)), remote_endpoint, 0, err);

	if (err) {
		cerr << "error: " << err << endl;
	}
}

int main(int ac, char * av[]) {
	options_description desc{"Options"};
	desc.add_options()
		("help,h", "print this message")
		("mbx,x", value<int>(&mbx)->default_value(120), "dimension of x vector")
		("mby,y", value<int>(&mby)->default_value(68), "dimension of y vector")
		("magnitude,m", value<int>(&magnitude)->default_value(60), "magnitude which suggests motion")
		("total,n", value<int>(&total)->default_value(10), "total vectors above magnitude which trigger motion")
		("remote,r", value<string>(&remote)->default_value("127.0.0.1"), "remote udp address for sending motion detection packets")
		("port,p", value<int>(&remote_port)->default_value(5555), "remote udp port")
		("test,t", bool_switch(&test)->default_value(false), "send test udp motion packet")
	;

	variables_map vm;
	store(parse_command_line(ac, av, desc), vm);
	notify(vm);

	if (vm.count("help") > 0) {
		cerr << desc << endl;
		return 1;
	}

	io_service io_service;
	ip::udp::socket socket(io_service);
	socket.open(ip::udp::v4());
	remote_endpoint = ip::udp::endpoint(ip::address::from_string(remote), remote_port);

	Power power;

	if (test) {
		send_motion_detect(socket, 70);
		send_motion_detect(socket, 71);
		send_motion_detect(socket, 72);
		socket.close();
		return 1;
	}

	auto len = (mbx+1)*mby;
	auto bytes = len * sizeof(motion_vector);
	auto imv = new motion_vector[len];

	auto sampling_frequency = 500ms;

	auto start = std::chrono::system_clock::now();

	while(cin.read(reinterpret_cast<char*>(imv), bytes)) {
		auto now = std::chrono::system_clock::now();

		if (now - start < sampling_frequency) {
			continue;
		}

		start = now;

		int c = 0;
		for(int i = 0; c < total && i < len; i++) {
			unsigned char magU=floor(sqrt(imv[i].x_vector*imv[i].x_vector+imv[i].y_vector*imv[i].y_vector));
			if(magU > magnitude) c++;
		}
		//cout << "mag: " << (int)c << endl;
		if(c >= total) {
			send_motion_detect(socket, c);
			power.power_on();

			cout << "detected motion" << endl;
		}
	}

	delete [] imv;

	socket.close();
	return 0;
}
