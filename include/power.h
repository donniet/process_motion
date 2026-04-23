#ifndef __POWER_H__
#define __POWER_H__

#include "countdown.hpp"

#include <string>
#include <iostream>
#include <atomic>
#include <thread>
#include <condition_variable>
#include <chrono>
#include <mutex>
#include <algorithm>
#include <optional>

#include <libcec/cec.h>
#include <libcec/cecloader.h>

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


class Power {
public:
    Power()
        : device_name(DEFAULT_DEVICE_NAME),
          verbose(true),
          failed(false),
          timer{ Countdown::duration_type::max() },
          power_off_thread(&Power::power_off_func, this)
    {
        // init();
    }

    Power( Countdown::duration_type standby ): Power()
    { timer.reset( standby ); }

    void stop()
    { 
        timer.stop(); 
        cv.notify_all();
        power_off_thread.join();
    }

    void power_on() 
    {
        timer.reset();
        do_power_on();
    }

    bool is_fail()
    {
        unique_lock<mutex> lock(m);
        return failed;
    }
    ~Power()
    { stop(); }

private:
    void power_off_func()
    {
        for(;;) {
            cerr << "waiting for standby time" << endl;
            timer.wait();
            cerr << "standby timeout reached, powering off." << endl;

            unique_lock<mutex> lock(m);
            do_power_off();
        }

        cerr << "exiting power_off loop" << endl;
    }

    static void handleCecLogMessage(void *cbParam, const cec_log_message *message)
    { reinterpret_cast<Power *>(cbParam)->CecLogMessage(cbParam, message); }

    void CecLogMessage(void *cbParam, const cec_log_message *message)
    { cerr << "CEC:" << message->message << endl; }

    static void handleCecAlert(void *cbParam, const libcec_alert type, const libcec_parameter param)
    { reinterpret_cast<Power *>(cbParam)->CecAlert(cbParam, type, param); }

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

    void open()
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
        uint8_t iDevicesFound = g_parser->DetectAdapters(devices, 10, NULL, false);
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
    }

    void close()
    {
        g_parser->Close();
        UnloadLibCec( g_parser );
    }

    void do_power_off() 
    {
        open();
        if( g_parser->StandbyDevices(addr) )
            last_power_status = false;
        close();
    }

    void do_power_on()
    {
        open();
        if( g_parser->PowerOnDevices(addr) )
            last_power_status = true;
        close();
    }

    bool cec_power_status()
    {
        auto power_status = g_parser->GetDevicePowerStatus(addr);
        if( power_status == CEC::CEC_POWER_STATUS_ON || 
            power_status == CEC::CEC_POWER_STATUS_IN_TRANSITION_STANDBY_TO_ON )
        {
            last_power_status = true;
            return true;
        }

        if( power_status == CEC::CEC_POWER_STATUS_STANDBY )
            last_power_status = false;
        else
            last_power_status.reset();

        return false;
    }


private:
    libcec_configuration g_config;
    ICECAdapter *g_parser;
    ICECCallbacks g_callbacks;
    std::string g_port;
    string device_name;
    cec_logical_address addr; // = (cec_logical_address)0;
    bool verbose;

    Countdown timer;

    Countdown::duration_type standby;
    mutex m;
    condition_variable cv;

    std::optional< bool > last_power_status;


    // Display* dpy_;

    bool failed;
    std::thread power_off_thread;
};

#endif