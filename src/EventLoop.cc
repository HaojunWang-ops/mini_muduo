#include "EventLoop.h"
#include "Poller.h"
#include "Channel.h"
#include "currentThread.h"
#include "logger.h"
#include "TimerQueue.h"

#include <assert.h>

namespace reactor
{

    __thread EventLoop *t_loopInThisThread = 0;
    const int kPollTimeMs = 1000;

    EventLoop::EventLoop()
        : looping_(false),
          quit_(false),
          threadId_(CurrentThread::tid()),
          poller_(new Poller(this)),
          timerQueue_(new TimerQueue(this))
    {
        LOG_INFO << "EventLoop created" << this << "in thread" << threadId_;
        if (t_loopInThisThread)
        {
            LOG_FATAL << "Another EventLoop" << t_loopInThisThread << " exits in this thread" << threadId_;
        }
        else
        {
            t_loopInThisThread = this;
        }
    }

    EventLoop::~EventLoop()
    {
        assert(!looping_);
        t_loopInThisThread = NULL;
    }

    void EventLoop::loop()
    {
        assert(!looping_);
        assertInLoopThread();
        looping_ = true;
        quit_ = false;
        while (!quit_)
        {
            activeChannels_.clear();
            poller_->poll(kPollTimeMs, &activeChannels_);
            for (size_t i = 0; i < activeChannels_.size(); i++)
            {
                Channel *channel = activeChannels_[i];
                channel->handleevent();
            }
        }
        LOG_INFO << "EventLoop" << this << " stop looping";
        looping_ = false;
    }

    void EventLoop::quit()
    {
        quit_ = true;
    }

    TimerId EventLoop::runAt(const Timestamp& time, const TimerCallback& cb){
        return timerQueue_->addTimer(cb, time, 0.0);
    }
    TimerId EventLoop::runAfter(double delay, const TimerCallback& cb){
        Timestamp time(addTime(Timestamp::now(), delay));
        return timerQueue_->addTimer(cb, time, 0.0);
    }
    TimerId EventLoop::runEvery(double interval, const TimerCallback& cb){
        Timestamp time(addTime(Timestamp::now(), interval));
        return timerQueue_->addTimer(cb, time, interval);
    }
    void EventLoop::updateChannel(Channel *channel)
    {
        assert(channel->ownerLoop() == this); //证明channel属于当前EventLoop
        assertInLoopThread();                 //证明操作EventLoop的线程是正确的I/O线程
        poller_->upateChannel(channel);
    }

    void EventLoop::abortNotInLoopThread()
    {
        LOG_FATAL << "EventLoop::abortNotInLoopThread - EventLoop " << this
                  << " was created in threadId_ = " << threadId_
                  << ", current thread id = " << CurrentThread::tid();
    }
}