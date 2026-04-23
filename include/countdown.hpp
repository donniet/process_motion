#ifndef __COUNTDOWN_HPP__
#define __COUNTDOWN_HPP__

#include <chrono>
#include <thread>
#include <mutex>
#include <condition_variable>

struct Countdown
{
    using duration_type = std::chrono::duration< double >;
    using clock_type = std::chrono::system_clock;
    using time_type = std::chrono::time_point< clock_type, duration_type >;
    using mutex_type = std::mutex;
    using lock_type = std::unique_lock< mutex_type >;

    static constexpr duration_type zero_duration = duration_type{ 0 };

    void stop()
    {
        lock_type lock( _m );
        _stopped = true;
        lock.unlock();
        _cv.notify_all();

        if( _pcounter != nullptr )
            _pcounter->join();

        delete _pcounter;
        _pcounter = nullptr;
    }

    void reset( duration_type until )
    {
        lock_type lock{ _m };
        _duration = until;
        lock.unlock();
        
        reset();
    }

    void reset()
    {
        lock_type lock{ _m };
        reset_locked();
        _cv.notify_all();

        if( _stopped )
        {
            if( _pcounter == nullptr )
                return;
             
            _pcounter->join();
            delete _pcounter;
            _pcounter = nullptr;
            return;
        }
        
        // not stopped
        if( _pcounter != nullptr )
        {
            // if we aren't joinable, then we are fine, it's waiting
            if( not _thread_complete )
                return;

            // otherwise we need to join and destroy the thread to reset it
            _pcounter->join();
            delete _pcounter;
            _pcounter = nullptr;
        }

        _thread_complete = false;
        _pcounter = new std::thread( &Countdown::counter_thread, this );
    }

    duration_type time_remaining()
    {
        lock_type lock{ _m };
        if( is_expired_locked() )
            return duration_type::zero();
        
        return clock_type::now() - _expire_time;
    }

    duration_type duration()
    {
        lock_type lock{ _m };
        return _duration;
    }

    void wait()
    {
        lock_type lock{ _m };
        _cv.wait( lock, [&](){ return is_expired_locked(); } );
    }

    bool is_expired()
    { 
        lock_type lock{ _m };
        return is_expired_locked();
    }

    bool is_stopped()
    {
        lock_type lock{ _m };
        return _stopped;
    }

    Countdown( duration_type until = zero_duration ): 
        _stopped{ true }, _thread_complete{ true }, _pcounter{ nullptr }
    { reset( until ); }

    ~Countdown()
    { stop(); }

protected:
    void reset_locked()
    {
        _expire_time = clock_type::now() + _duration;
        _stopped = ( _duration == zero_duration );
    }

    bool is_expired_locked()
    { return _stopped || clock_type::now() >= _expire_time; }

    void counter_thread()
    {
        time_type my_expire_time;

        for(;;)
        {
            lock_type lock{ _m };
            my_expire_time = _expire_time;

            _cv.wait_until( lock, my_expire_time, [this, my_expire_time]()
                { return _stopped || my_expire_time != _expire_time; });

            // were we stopped?
            if( _stopped )
                break;

            // was the expire time reset?
            if( my_expire_time != _expire_time )
            {
                // we must have been woken up by a _cv.notify_all()
                if( not is_expired_locked() )
                    continue;

                // we don't need to call notify_all since we were notified by it
                break;
            }

            // otherwise the time expired
            _stopped = true;
            _cv.notify_all();
            break;
        }
        _thread_complete = true;
    }

private:
    bool _stopped;
    bool _thread_complete;
    duration_type _duration;
    time_type _expire_time;
    mutex_type _m;
    std::condition_variable _cv;
    std::thread* _pcounter;
};

#endif