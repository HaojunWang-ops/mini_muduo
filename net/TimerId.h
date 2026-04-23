#pragma once

namespace reactor
{
    namespace net
    {
        class Timer;

        class TimerId
        {
        public:
            explicit TimerId(Timer *timer)
                : value_(timer)
            {
            }

        private:
            Timer *value_;
        };
    }
}