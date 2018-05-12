
#include <string>
#include <iostream>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <mutex>

#include <libcec/cec.h>
#include <libcec/cecloader.h>

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::atomic_flag;
using std::mutex;
using std::unique_lock;
using std::condition_variable;

using namespace CEC;

const char * DEFAULT_DEVICE_NAME = "Smart Mirror";

class Power {
private:
  libcec_configuration g_config;
  ICECAdapter *g_parser;
  ICECCallbacks        g_callbacks;
  std::string g_port;
  string device_name;
  cec_logical_address addr; // = (cec_logical_address)0;
  bool verbose;
  bool is_power_on;
  std::chrono::duration<double> standby;
  std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<double> > standby_time;
  mutex m;
  condition_variable cv;

  atomic_flag failed;
  std::thread power_off_thread;


private:
  static void handleCecLogMessage(void * cbParam, const cec_log_message * message) {
    reinterpret_cast<Power*>(cbParam)->CecLogMessage(cbParam, message);
  }
  void CecLogMessage(void *cbParam, const cec_log_message* message) {
  	cerr << message->message << endl;
  }

  static void handleCecAlert(void * cbParam, const libcec_alert type, const libcec_parameter param) {
    reinterpret_cast<Power*>(cbParam)->CecAlert(cbParam, type, param);
  }
  void CecAlert(void *cbParam, const libcec_alert type, const libcec_parameter param) {
  	switch (type) {
  	case CEC_ALERT_CONNECTION_LOST:
  		cerr << "connection lost, trying to reconnect\n" << endl;
  		if(g_parser) {
  			g_parser->Close();
  			if(!g_parser->Open(g_port.c_str())) {
  				cerr << "failed to reconnect.\n";
          			failed.test_and_set();
  			}
  		}
  	}
  }

  void init() {
    g_config.Clear();
    snprintf(g_config.strDeviceName, device_name.length(), device_name.c_str());
    g_config.clientVersion      = LIBCEC_VERSION_CURRENT;
    g_config.bActivateSource    = 1;
    if (verbose) {
      g_callbacks.logMessage      = &Power::handleCecLogMessage;
    }
    g_callbacks.alert           = &Power::handleCecAlert;
    g_config.callbackParam = reinterpret_cast<void*>(this);
    g_config.callbacks = &g_callbacks;
    g_config.deviceTypes.Add(CEC_DEVICE_TYPE_PLAYBACK_DEVICE);

    g_parser = LibCecInitialise(&g_config);

    if (!g_parser) {
      throw string("could not initialize libcec");
    }

    g_parser->InitVideoStandalone();
    cec_adapter_descriptor devices[10];
    uint8_t iDevicesFound = g_parser->DetectAdapters(devices, 10, NULL, true);
    if (iDevicesFound <= 0) {
      throw string("no devices found");
    }
    cout << "devices found:\n";
    for(int i = 0; i < iDevicesFound; i++) {
      cout << devices[i].strComName << endl;
    }
    g_port = devices[0].strComName;
    if (!g_parser->Open(g_port.c_str())) {
      throw string("could not open device");
    }
    addr = (cec_logical_address)0;
  }
  void power_off_func() {
	unique_lock<mutex> lock(m);

	while(true) {
		cv.wait_until(lock, standby_time);

		if (standby_time <= std::chrono::system_clock::now()) {
			g_parser->StandbyDevices(addr);
			is_power_on = false;
		}
	}
  }
public:
  Power()  : 
	  device_name(DEFAULT_DEVICE_NAME), 
	  verbose(false), 
	  failed(ATOMIC_FLAG_INIT), 
	  standby(600.),
	  standby_time(std::chrono::system_clock::now()), 
	  power_off_thread(&Power::power_off_func, this) 
  {
    init();
  }

  void power_on() {
	unique_lock<mutex> lock(m);	
	is_power_on = g_parser->PowerOnDevices(addr);
	standby_time = std::chrono::system_clock::now() + standby;
	lock.unlock();
	cv.notify_one();
  }
  string get_port() const {
    return g_port;
  }
  bool is_fail() {
    if (failed.test_and_set()) {
      return true;
    }
    failed.clear();
    return false;
  }
  ~Power() {
    UnloadLibCec(g_parser);
  }
};
