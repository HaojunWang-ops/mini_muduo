#include "EventLoopThread.h"
#include "EventLoop.h"

#include <functional>

namespace reactor
{
    namespace net
    {
        EventLoopThread::EventLoopThread()
            : loop_(NULL),
              exiting_(false),
              thread_(std::bind(&EventLoopThread::threadFunc, this)),
              mutex_(),
              cond_(mutex_)
        {
        }
        EventLoopThread::~EventLoopThread()
        {
            exiting_ = true;
            loop_->quit();
            thread_.join();
        }

        EventLoop *EventLoopThread::startLoop()
        {
            assert(!thread_.started());
            thread_.start();

            {
                MutexLockGuard lock(mutex_);
                while (loop_ == NULL)
                {
                    cond_.wait();
                }
            }

            return loop_;
        }

        void EventLoopThread::threadFunc()
        {
            EventLoop loop;
            {
                MutexLockGuard lock(mutex_);
                loop_ = &loop;
                cond_.notifyAll();
            }
            loop.loop();
        }
    }
}