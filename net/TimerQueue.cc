#include "TimerQueue.h"
#include "Logging.h"
#include "Timer.h"
#include "EventLoop.h"

#include <sys/timerfd.h>
#include <cstdint>
#include <assert.h>

namespace reactor
{
    namespace net
    {
        namespace detail
        {
            int createTimerfd()
            {
                int timerfd = ::timerfd_create(CLOCK_MONOTONIC, TFD_CLOEXEC | TFD_NONBLOCK);
                if (timerfd < 0)
                {
                    LOG_SYSERR << "createTimerfd() error, at timerfd_create";
                }
                return timerfd;
            }

            struct timespec howMuchTimeFromNow(Timestamp when)
            {
                struct timespec tv;
                int64_t microseconds = when.microSecondsSinceEpoch() - (Timestamp::now()).microSecondsSinceEpoch();
                if (microseconds < 100)
                {
                    microseconds = 100;
                }

                tv.tv_sec = static_cast<time_t>(microseconds / Timestamp::kMicroSecondsPerSecond);
                tv.tv_nsec = static_cast<long>(microseconds % Timestamp::kMicroSecondsPerSecond * 1000);

                return tv;
            }

            void readTimerfd(int timerfd, Timestamp now)
            {
                uint64_t howmany;
                int n = ::read(timerfd, &howmany, sizeof howmany);
                LOG_INFO << now.toFormattedString() << " " << howmany << " in timerfd";
                if (n != sizeof howmany)
                {
                    LOG_SYSERR << "read " << n << " bytes from timerfd";
                }
            }

            void resetTimerfd(int timerfd, Timestamp expiration)
            {
                struct itimerspec newvalue;
                struct itimerspec oldvalue;

                bzero(&newvalue, sizeof newvalue);
                bzero(&oldvalue, sizeof oldvalue);

                newvalue.it_value = howMuchTimeFromNow(expiration);

                int ret = timerfd_settime(timerfd, 0, &newvalue, &oldvalue);
                if (ret < 0)
                {
                    LOG_SYSERR << "resetTimerfd error, at timerfd_settime()";
                }
            }
        }
    }
}

using namespace reactor;
using namespace reactor::net;
using namespace reactor::net::detail;

TimerQueue::TimerQueue(EventLoop *loop)
    : loop_(loop),
      timerfd_(createTimerfd()),
      timerfdChannel_(loop_, timerfd_)
{
    timerfdChannel_.setReadCallback([this](Timestamp receivetime)
    { 
        handleRead(receivetime); 
    });
    timerfdChannel_.enableRead();
}

TimerQueue::~TimerQueue()
{
    ::close(timerfd_);

    for (auto it = timers_.begin(); it != timers_.end(); it++)
    {
        delete it->second;
    }
}

TimerId TimerQueue::addTimer(const TimerCallback &callback, Timestamp when, double interval)
{
    Timer *timer = new Timer(callback, when, interval);
    loop_->runInLoop([this, timer]()
                     { addTimerInLoop(timer); });
    return TimerId(timer);
}

void TimerQueue::addTimerInLoop(Timer *timer)
{
    loop_->assertInLoopThread();
    bool earliestChange = insert(timer);

    if (earliestChange)
    {
        resetTimerfd(timerfd_, timer->expiration());
    }
}
void TimerQueue::handleRead(Timestamp receivetime)
{
    loop_->assertInLoopThread();
    Timestamp now(Timestamp::now());
    readTimerfd(timerfd_, now);
    std::vector<Entry> Expired = getExpired(now);
    for (auto it = Expired.begin(); it != Expired.end(); it++)
    {
        it->second->run();
    }
    reset(Expired, now);
}

std::vector<TimerQueue::Entry> TimerQueue::getExpired(Timestamp now)
{
    std::vector<Entry> Expired;
    Entry sentry = std::make_pair(now, reinterpret_cast<Timer *>(UINTPTR_MAX));
    auto it = timers_.lower_bound(sentry);
    assert(it == timers_.end() || now < it->first);
    std::copy(timers_.begin(), it, std::back_inserter(Expired));
    timers_.erase(timers_.begin(), it);
    return Expired;
}

void TimerQueue::reset(const std::vector<Entry> &expired, Timestamp now)
{
    Timestamp nextexpire;
    for (auto it = expired.begin(); it != expired.end(); it++)
    {
        if (it->second->repeat())
        {
            it->second->restart(now);
            insert(it->second);
        }
        else
        {
            delete it->second;
        }
    }

    if (!timers_.empty())
    {
        nextexpire = timers_.begin()->first;
    }
    if (nextexpire.valid())
    {
        resetTimerfd(timerfd_, nextexpire);
    }
}

bool TimerQueue::insert(Timer *timer)
{
    bool earliestChange = false;
    if (timers_.empty())
    {
        earliestChange = true;
    }
    auto it = timers_.begin();
    if (it == timers_.end() || timer->expiration() < it->first)
    {
        earliestChange = true;
    }

    std::pair<TimerList::iterator, bool> result = timers_.insert(std::pair<Timestamp, Timer *>(timer->expiration(), timer));
    assert(result.second);
    return earliestChange;
}
