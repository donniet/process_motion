
#include <string>
#include <iostream>
#include <atomic>

#include <libcec/cec.h>
#include <libcec/cecloader.h>

using std::cout;
using std::cerr;
using std::endl;
using std::string;
using std::atomic_flag;

using namespace CEC;

const char * DEFAULT_DEVICE_NAME = "Smart Mirror";

class Power {
private:
  libcec_configuration g_config;
  ICECAdapter *g_parser;
  ICECCallbacks        g_callbacks;
  std::string g_port;
  string device_name;
  bool verbose;
  atomic_flag failed;


private:
  static void handleCecLogMessage(void * cbParam, const cec_log_message * message) {
    reinterpret_cast<Power*>(cbParam)->CecLogMessage(cbParam, message);
  }
  void CecLogMessage(void *cbParam, const cec_log_message* message);

  static void handleCecAlert(void * cbParam, const libcec_alert type, const libcec_parameter param) {
    reinterpret_cast<Power*>(cbParam)->CecAlert(cbParam, type, param);
  }
  void CecAlert(void *cbParam, const libcec_alert type, const libcec_parameter param);

  void init();
public:
  Power();
  string get_port() const;
  bool is_fail();
  ~Power();
};
