
#include <string>
#include <iostream>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <mutex>
#include <algorithm>

#include <libcec/cec.h>
#include <libcec/cecloader.h>


#include <X11/Xlib.h>
#include <X11/extensions/dpms.h>
#include <X11/extensions/XTest.h>

#include <unistd.h>

#include <functional>

using std::function;

using std::atomic_flag;
using std::cerr;
using std::condition_variable;
using std::cout;
using std::endl;
using std::mutex;
using std::string;
using std::unique_lock;

using namespace CEC;
using namespace std::chrono_literals;

const char *DEFAULT_DEVICE_NAME = "Smart Mirror";

struct Defer {
    std::function<void()> _defered;
    Defer(std::function<void()> defered) : _defered(defered) { }
    Defer() : _defered() { }
    Defer & operator=(std::function<void()> defered)
    {
        _defered();
        _defered = defered;
        return *this;
    }
    ~Defer() { _defered(); }
};

class Power
{
private:
    libcec_configuration g_config;
    ICECAdapter *g_parser;
    ICECCallbacks g_callbacks;
    std::string g_port;
    string device_name;
    cec_logical_address addr; // = (cec_logical_address)0;
    bool verbose;
    std::chrono::duration<double> standby;
    std::chrono::duration<double> wakeup_interval;
    std::chrono::duration<double> wakeup_timeout;
    std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<double>> standby_time;
    std::chrono::time_point<std::chrono::system_clock, std::chrono::duration<double>> last_on_time;
    mutex m;
    condition_variable cv;


    // Display* dpy_;

    bool failed;
    std::thread power_off_thread;

private:
    static void handleCecLogMessage(void *cbParam, const cec_log_message *message)
    {
        reinterpret_cast<Power *>(cbParam)->CecLogMessage(cbParam, message);
    }
    void CecLogMessage(void *cbParam, const cec_log_message *message)
    {
        cerr << "CEC:" << message->message << endl;
    }

    static void handleCecAlert(void *cbParam, const libcec_alert type, const libcec_parameter param)
    {
        reinterpret_cast<Power *>(cbParam)->CecAlert(cbParam, type, param);
    }
    void CecAlert(void *cbParam, const libcec_alert type, const libcec_parameter param)
    {
        unique_lock<mutex> lock(m);

        

        switch (type)
        {
        case CEC_ALERT_SERVICE_DEVICE:
            cerr << "ALERT: service device" << endl;
            break;
        case CEC_ALERT_PERMISSION_ERROR:
            cerr << "ALERT: permission error" << endl;
            break;
        case CEC_ALERT_PORT_BUSY:
            cerr << "ALERT: port busy" << endl;
            break;
        case CEC_ALERT_PHYSICAL_ADDRESS_ERROR:
            cerr << "ALERT: physical address error" << endl;
            break;
        case CEC_ALERT_TV_POLL_FAILED:
            cerr << "ALERT: tv poll failed" << endl;
            break;
        case CEC_ALERT_CONNECTION_LOST:
            cerr << "connection lost, trying to reconnect\n"
                 << endl;
            if (g_parser)
            {
                g_parser->Close();
                if (!g_parser->Open(g_port.c_str()))
                {
                    cerr << "failed to reconnect.\n";
                    failed = true;
                    lock.unlock();
                    cv.notify_all();
                }
            }
        }
    }

    void init()
    {
        g_config.Clear();
        size_t max_name_length = sizeof(g_config.strDeviceName) / sizeof(g_config.strDeviceName[0]) - 1;
        strncpy(g_config.strDeviceName, device_name.c_str(), max_name_length);
        g_config.clientVersion = LIBCEC_VERSION_CURRENT;
        g_config.bActivateSource = 1;
        if (verbose)
        {
            g_callbacks.logMessage = &Power::handleCecLogMessage;
        }
        g_callbacks.alert = &Power::handleCecAlert;
        g_config.callbackParam = reinterpret_cast<void *>(this);
        g_config.callbacks = &g_callbacks;
        g_config.deviceTypes.Add(CEC_DEVICE_TYPE_PLAYBACK_DEVICE);

        g_parser = LibCecInitialise(&g_config);

        if (!g_parser)
        {
            throw string("could not initialize libcec");
        }

        g_parser->InitVideoStandalone();
        cec_adapter_descriptor devices[10];
        uint8_t iDevicesFound = g_parser->DetectAdapters(devices, 10, NULL, true);
        if (iDevicesFound <= 0)
        {
            failed = true;
            throw string("no devices found");
        }
        cout << "devices found:\n";
        for (int i = 0; i < iDevicesFound; i++)
        {
            cout << "COM Name: " << devices[i].strComName << endl;
        }
        g_port = devices[0].strComName;
        if (!g_parser->Open(g_port.c_str()))
        {
            failed = true;
            throw string("could not open device");
        }
        
        addr = (cec_logical_address)0;

        // dpy_ = XOpenDisplay(NULL);
        // DPMSDisable(dpy_);
        // cerr << "setting screensaver: " << 1+(int)standby.count() << endl;
        // XSetScreenSaver(dpy_, 1+(int)standby.count(), 1+(int)standby.count(), 1, 1);
    }

    void do_power_off() 
    {
        init();
        g_parser->StandbyDevices(addr);
        UnloadLibCec(g_parser);
    }

    void do_power_on()
    {
        init();
        g_parser->PowerOnDevices(addr);
        UnloadLibCec(g_parser);
    }

    void power_off_func()
    {
        unique_lock<mutex> lock(m);


        for(;;) {
            cv.wait_until(lock, standby_time);

            if(failed) break;

            cerr << "in power_off loop" << endl;
            auto now = std::chrono::system_clock::now();

            if (standby_time <= now)
            {
                cerr << "standby time expired" << endl;
                if (is_power_on())
                {
                    cerr << "powering off" << endl;
                    do_power_off();
                } 
                standby_time = now + standby;
            } 
            else if(!is_power_on() && now - last_on_time > wakeup_interval)
            {
                cerr << "wakeup interval expired, waking up" << endl;
                do_power_on();
                last_on_time = now;
                standby_time = now + 10s;
            }
        }

        cerr << "exiting power_off loop" << endl;
    }

public:
    Power()
        : device_name(DEFAULT_DEVICE_NAME),
          verbose(true),
          failed(false),
          standby(600s),
          wakeup_interval(3600s),
          wakeup_timeout(10s),
          standby_time(std::chrono::system_clock::now()),
          last_on_time(std::chrono::system_clock::now()),
          power_off_thread(&Power::power_off_func, this)
    {
        // init();
    }

    Power(std::chrono::duration<double> standby)
        : device_name(DEFAULT_DEVICE_NAME),
          verbose(true),
          failed(false),
          standby(standby),
          wakeup_interval(3600s),
          wakeup_timeout(10s),
          standby_time(std::chrono::system_clock::now()),
          last_on_time(std::chrono::system_clock::now()),
          power_off_thread(&Power::power_off_func, this)
    {
        // init();
    }

private:
    // must be called with a lock
    bool is_power_on()
    {
        init();
        Defer unload([&](){ 
            UnloadLibCec(g_parser); 
        });

        auto ret = CEC::CEC_POWER_STATUS_ON == g_parser->GetDevicePowerStatus(addr);
        // UnloadLibCec(g_parser);
        return ret;
    }
public:
    void power_on() 
    {
        unique_lock<mutex> lock(m);
        do_power_on();
    }

    bool is_fail()
    {
        unique_lock<mutex> lock(m);
        return failed;
    }
    ~Power()
    {
        unique_lock<mutex> lock(m);
        failed = true;
        cv.notify_all();
        power_off_thread.join();
    }
};
