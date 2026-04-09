#pragma once

#include "Timestamp.h"
#include "Callbacks.h"

#include <functional>

namespace reactor{
    class Timer{
    public:
        Timer(const TimerCallback& callback, Timestamp when, double interval)
            : callback_(callback),
              expiration_(when),
              interval_(interval),
              repeat_(interval > 0.0)
        {}
        
        void run() const{
            callback_();
        }

        Timestamp expiration(){
            return expiration_;
        }
        double interval(){
            return interval_;
        }
        bool repeat(){
            return repeat_;
        }

        void restart(Timestamp now);

    private:
        const TimerCallback callback_;
        Timestamp expiration_;
        const double interval_;
        const bool repeat_;
    };    
}