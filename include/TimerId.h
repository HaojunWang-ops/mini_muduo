#pragma once

namespace reactor{
    class Timer;

    class TimerId{
    public:
        explicit TimerId(Timer* timer)
            :value_(timer)
        {}
        
    private:
        Timer* value_;
    };
}