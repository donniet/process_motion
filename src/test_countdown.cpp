#include "countdown.hpp"

#include <chrono>
#include <iostream>

void waiting_thread( Countdown& timer )
{
    using std::cout, std::endl;

    cout << "THREAD: I'm waiting!" << endl;
    timer.wait();
    cout << "THREAD: I'm done waiting!" << endl;
}

int main( int ac, char * av[] )
{
    using namespace std::chrono_literals;
    using std::cout, std::endl;

    cout << "starting 2s timer..." << endl;
    auto timer = Countdown{ 2s };
    cout << "waiting for timer to expire..." << endl;
    timer.wait();
    cout << "timer expired." << endl;
    cout << "trying to wait immediately..." << endl;
    timer.wait();
    cout << "done waiting, reseting for 1s..." << endl;
    timer.reset( 1s );
    cout << "waiting for 1s timer..." << endl;
    timer.wait();
    cout << "done waiting." << endl;

    cout << "reseting for 60s" << endl;
    timer.reset( 60s );
    cout << "staring a thread to wait on this timer" << std::endl;
    auto t = std::thread( waiting_thread, std::ref(timer) );
    cout << "sleeping for 5s..." << endl;
    std::this_thread::sleep_for( 5s );
    cout << "reseting the timer for 1s" << endl;
    timer.reset( 1s );
    cout << "waiting..." << endl;
    timer.wait();
    cout << "timer done, joining thread." << endl;
    t.join();
    cout << "joined, done." << endl;
    
    return EXIT_SUCCESS;
}