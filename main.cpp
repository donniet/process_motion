#include <iostream>
#include <math.h>

#include <boost/program_options.hpp>

using namespace boost::program_options;

typedef struct {
	signed char x_vector;
	signed char y_vector;
	short sad;
} INLINE_MOTION_VECTOR; 

int mbx = 120;
int mby = 68;
int magnitude = 60;
int total = 10;

using std::cin;
using std::cout;
using std::cerr;
using std::endl;

int main(int ac, char * av[]) {
	options_description desc{"Options"};
	desc.add_options()
		("help,h", "print this message")
		("mbx,x", value<int>(&mbx)->default_value(120), "dimension of x vector")
		("mby,y", value<int>(&mby)->default_value(68), "dimension of y vector")
		("magnitude,m", value<int>(&magnitude)->default_value(60), "magnitude which suggests motion")
		("total,n", value<int>(&total)->default_value(10), "total vectors above magnitude which trigger motion")
	;

	variables_map vm;
	store(parse_command_line(ac, av, desc), vm);
	notify(vm);

	if (vm.count("help") > 0) {
		cerr << desc << endl;
		return 1;
	}

	auto len = (mbx+1)*mby;
	auto bytes = len * sizeof(INLINE_MOTION_VECTOR);
	auto imv = new INLINE_MOTION_VECTOR[len];

	while(cin.read(reinterpret_cast<char*>(imv), bytes)) {
		int c = 0;
		for(int i = 0; c < total && i < len; i++) {
			unsigned char magU=floor(sqrt(imv[i].x_vector*imv[i].x_vector+imv[i].y_vector*imv[i].y_vector));
			if(magU > magnitude) c++; 
		}
		cout << "mag: " << (int)c << endl;
		if(c >= total) {
			cout << "detected motion" << endl;
		}
	}

	delete [] imv;

	return 0;
}
