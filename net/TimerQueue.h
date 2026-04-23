#pragma once

#include "Channel.h"
#include "Timestamp.h"
#include "TimerId.h"
#include "Callbacks.h"

#include <functional>
#include <vector>
#include <set>
#include <utility>

namespace reactor
{
    namespace net
    {
        class Channel;
        class EventLoop;
        class Timer;

        class TimerQueue
        {
        public:
            TimerQueue(EventLoop *loop);
            ~TimerQueue();

            TimerId addTimer(const TimerCallback &callback, Timestamp now, double interval);

        private:
            typedef std::pair<Timestamp, Timer *> Entry;
            typedef std::set<Entry> TimerList;

            void addTimerInLoop(Timer *timer);

            void handleRead(Timestamp receivetime);

            std::vector<Entry> getExpired(Timestamp now);
            void reset(const std::vector<Entry> &expired, Timestamp now);

            bool insert(Timer *timer);

            EventLoop *loop_;
            const int timerfd_;
            Channel timerfdChannel_;

            TimerList timers_;
        };
    }
}