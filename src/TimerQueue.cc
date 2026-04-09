#include "TimerQueue.h"
#include "logger.h"
#include "Timer.h"
#include "EventLoop.h"

#include <sys/timerfd.h>
#include <cstdint>
#include <assert.h>

namespace reactor{
    namespace detail{
        int createTimerfd(){
            int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_NONBLOCK | TFD_CLOEXEC);
            if (timerfd < 0){
                LOG_FATAL << "Failed in timerfd_create.";
            }
            return timerfd;
        }

        struct timespec howMuchTimerFromNow(Timestamp when){
            int64_t microseconds = when.microSecondsSinceEpoch() - Timestamp::now().microSecondsSinceEpoch();

            if (microseconds < 100){
                microseconds = 100;
            }

            struct timespec tv;
            
            tv.tv_sec = static_cast<time_t> (microseconds / Timestamp::kMicroSecondsPerSecond);
            tv.tv_nsec = static_cast<long> (microseconds % Timestamp::kMicroSecondsPerSecond * 1000);

            
            return tv;
        }

        void readTimerfd(int timerfd, Timestamp now){

            uint64_t howmany;
            ssize_t n = ::read(timerfd, &howmany, sizeof(howmany));
            LOG_INFO << "TimerQueue::handleRead() " << howmany << " at " << now.toString();
            if (n != sizeof(howmany)){
                LOG_ERROR << "TimerQueue::handleRead() reads " << n << " bytes instead of 8"; 
            }
        }

        void resetTimerfd(int timerfd, Timestamp expiration){
            struct itimerspec newvalue;
            struct itimerspec oldvalue;

            bzero(&newvalue, sizeof(newvalue));
            bzero(&oldvalue, sizeof(oldvalue));

            newvalue.it_value = howMuchTimerFromNow(expiration);

            int ret = ::timerfd_settime(timerfd, 0, &newvalue, &oldvalue);
            if (ret < 0){
                LOG_FATAL << "timerfd_settime error.";
            }
        }
    }
}

using namespace reactor;
using namespace reactor::detail;

TimerQueue::TimerQueue(EventLoop* loop)
    : loop_(loop),
      timerfd_(createTimerfd()),
      timerfdChannel_(loop_, timerfd_),
      timers_()
    {
        timerfdChannel_.setReadCallback(std::bind(&TimerQueue::handleRead, this));
        timerfdChannel_.enableRead();
    }

TimerQueue::~TimerQueue(){
    ::close(timerfd_);

    for (auto it = timers_.begin(); it != timers_.end(); it++){
        delete it->second;
    }        
}

TimerId TimerQueue::addTimer(const TimerCallback& callback, Timestamp when, double interval){
    Timer* timer = new Timer(callback, when, interval);
    loop_->assertInLoopThread();
    bool earliestChanged = insert(timer);
    if (earliestChanged){
        resetTimerfd(timerfd_, timer->expiration());
    }

    return TimerId(timer);
}

void TimerQueue::handleRead(){
    loop_->assertInLoopThread();
    
    Timestamp now(Timestamp::now());
    readTimerfd(timerfd_, now);
    std::vector<Entry> expired = getExpired(now);
    for (auto it = expired.begin(); it != expired.end(); it++){
        it->second->run();
    }

    reset(expired, now);
}

std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now){
    std::vector<Entry> expired;
    Entry sentry = std::make_pair(now, reinterpret_cast<Timer*>(UINTPTR_MAX));
    
    auto it = timers_.lower_bound(sentry);
    assert(it == timers_.end() || now < it->first);

    std::copy(timers_.begin(), it, back_inserter(expired));
    timers_.erase(timers_.begin(), it);
    
    return expired;
}

void TimerQueue::reset(const std::vector<Entry>& expired, Timestamp now){
    Timestamp nextExpire;

    for (auto it = expired.begin(); it != expired.end(); it++){
        if (it->second->repeat()){
            it->second->restart(now);
            insert(it->second);
        }else{
            delete it->second;
        }
    }

    if (!timers_.empty()){
        nextExpire = timers_.begin()->second->expiration();
    }

    if(nextExpire.valid()){
        resetTimerfd(timerfd_, nextExpire);
    }
}
bool TimerQueue::insert(Timer* timer){
    bool earliestChanged = false;
    Timestamp when = timer->expiration();
    auto it = timers_.begin();
    if (it == timers_.end() || when < it->first){
        earliestChanged = true;
    }
    std::pair<TimerList::iterator, bool> result = timers_.insert(std::make_pair(when, timer));
    assert(result.second);
    return earliestChanged;
}