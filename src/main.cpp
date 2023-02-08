#include "power.h"

#include <boost/program_options.hpp>
#include <boost/asio.hpp>

#ifdef USE_OPENCL
#include <boost/compute.hpp>
#endif


#include <iostream>
#include <string>
#include <chrono>
#include <vector>
#include <numeric>
#include <stdint.h>

#include <math.h>

using namespace std::literals::chrono_literals;

using namespace boost::program_options;
using namespace boost::asio;
using namespace boost;
using std::string;
using std::cin;
using std::cout;
using std::cerr;
using std::endl;
using std::vector;


struct motion_vector {
	int8_t x_vector;
	int8_t y_vector;
	int16_t sad;
};

#ifdef USE_OPENCL
BOOST_COMPUTE_ADAPT_STRUCT(motion_vector, motion_vector, (x_vector, y_vector, sad))

BOOST_COMPUTE_FUNCTION(int, vector_length, (motion_vector a), \
	return int(a.x_vector) * int(a.x_vector) + int(a.y_vector) * int(a.y_vector); \
);
#endif

int mbx = 120;
int mby = 68;
int magnitude = 60;
int total = 10;
string remote = "127.0.0.1";
int remote_port = 5555;
bool test = false;
ip::udp::endpoint remote_endpoint;
std::chrono::duration<double> standby_time = 300s;

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

namespace std { namespace chrono {
    std::wistream& operator>>(std::wistream& is, std::chrono::duration<double>& v) {
        int seconds;
        is >> seconds;
        v = std::chrono::seconds(seconds);
        return is;
    }
    std::wostream& operator<<(std::wostream& os, const std::chrono::duration<double>& v) {
        return os << v.count();
    }
    std::istream& operator>>(std::istream& is, std::chrono::duration<double>& v) {
        int seconds;
        is >> seconds;
        v = std::chrono::seconds(seconds);
        return is;
    }
    std::ostream& operator<<(std::ostream& os, const std::chrono::duration<double>& v) {
        return os << v.count();
    }
}}

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
        ("standby,s",  value<std::chrono::duration<double>>(&standby_time)->default_value(300s), "time to wait before putting monitor into standby mode")
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

#ifdef USE_OPENCL
	compute::device device = compute::system::default_device();
	compute::context context(device);
	compute::command_queue queue(context, device);	
#endif

	Power power(standby_time);

	if (test) {
		send_motion_detect(socket, 70);
		send_motion_detect(socket, 71);
		send_motion_detect(socket, 72);
		socket.close();
		return 1;
	}

	auto magnitude2 = magnitude * magnitude;
	auto len = (mbx+1)*mby;
	auto bytes = len * sizeof(motion_vector);
	auto imv = new motion_vector[len];

#ifdef USE_OPENCL
	compute::mapped_view<motion_vector> imv_view(imv, len, context);
	compute::vector<int> counts(len, context);

	using compute::_1;
	using compute::_2;
#else
	auto counts = new int[len];
#endif

	auto sampling_frequency = 100ms;

	auto start = std::chrono::system_clock::now();


	while(cin.read(reinterpret_cast<char*>(imv), bytes)) {
		auto now = std::chrono::system_clock::now();

		if (now - start < sampling_frequency) {
			continue;
		}

		int c = 0;

#ifdef USE_OPENCL
		compute::transform(imv_view.begin(), imv_view.end(), counts.begin(), vector_length);
		compute::transform(counts.begin(), counts.end(), counts.begin(), _1 > magnitude2 ? 1 : 0);
		c = compute::accumulate(counts.begin(), counts.end(), 0, _1 + _2);
#else
		std::transform(imv, imv + len, counts, [](motion_vector const & a) -> int {
			int x = a.x_vector, y = a.y_vector;
			return x * x + y * y;
		});
		std::transform(counts, counts + len, counts, [magnitude2](int a) -> int {
			return a > magnitude2 ? 1 : 0;
		});
        
		//c = std::reduce(counts, counts + len, 0, std::plus<int>());
        // c = std::accumulate(counts, counts + len, 0, std::plus<int>());


#endif
		// for(int i = 0; c < total && i < len; i++) {
		// 	int magU = (int)imv[i].x_vector*(int)imv[i].x_vector+(int)imv[i].y_vector*(int)imv[i].y_vector;

		// 	if(magU > magnitude2) c++;
		// }
		// cout << "mag: " << (int)c << endl;
		if(c >= total) {
			send_motion_detect(socket, c);
			power.power_on();

			// cout << "detected motion" << endl;
		}
	}

	delete [] imv;

#ifndef USE_OPENCL
	delete [] counts;
#endif

	socket.close();
	return 0;
}
