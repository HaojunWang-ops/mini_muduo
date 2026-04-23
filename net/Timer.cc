#include "Timer.h"

namespace reactor
{
    namespace net
    {
        void Timer::restart(Timestamp now)
        {
            if (repeat_)
            {
                expiration_ = addTime(now, interval_);
            }
            else
            {
                expiration_ = Timestamp::invalid();
            }
        }
    }
}