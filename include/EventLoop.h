#pragma once

#include "logger.h"
#include "currentThread.h"
#include "TimerId.h"
#include "Callbacks.h"

#include <vector>
#include <memory>
namespace reactor
{
    class Poller;
    class Channel;
    class TimerQueue;
    
    class EventLoop
    {
    public:
        EventLoop();
        ~EventLoop();

        EventLoop(const EventLoop &) = delete;
        EventLoop &operator=(const EventLoop &) = delete;

        void assertInLoopThread()
        {
            if (!isInLoopThread())
            {
                abortNotInLoopThread();
            }
        }
        bool isInLoopThread() const
        {
            return threadId_ == CurrentThread::tid();
        }

        void loop();
        void updateChannel(Channel *channel);
        void quit();

        TimerId runAt(const Timestamp& time, const TimerCallback& cb);
        TimerId runAfter(double delay, const TimerCallback& cb);
        TimerId runEvery(double interval, const TimerCallback& cb);
    private:
        void abortNotInLoopThread();

        typedef std::vector<Channel *> ChannelList;
        bool looping_;
        bool quit_;
        const pid_t threadId_;
        std::unique_ptr<Poller> poller_;
        std::unique_ptr<TimerQueue> timerQueue_;
        ChannelList activeChannels_;
    };
}