#pragma once

#include "Mutex.h"
#include "Thread.h"
#include "Condition.h"

#include "noncopyable.h"

namespace reactor{
    class EventLoop;

    class EventLoopThread : noncopyable
    {
        public:
            EventLoopThread();
            ~EventLoopThread();
            EventLoop* startLoop();

        private:
            void threadFunc();

            EventLoop* loop_;
            bool exiting_;
            Thread thread_;
            MutexLock mutex_;
            Condition cond_;
    };
}