#include "Channel.h"
#include "EventLoop.h"

#include <poll.h>

namespace reactor
{
    namespace net
    {
        const int Channel::kNoneEvent = 0;
        const int Channel::kReadEvent = POLLIN | POLLPRI;
        const int Channel::kWriteEvent = POLLOUT;

        Channel::Channel(EventLoop *loop, int fd)
            : ownerLoop_(loop),
              fd_(fd),
              index_(-1),
              events_(0),
              revents_(0),
              eventHandling_(false)
        {
        }

        Channel::~Channel()
        {
            assert(!eventHandling_);
        }

        void Channel::update()
        {
            ownerLoop_->updateChannel(this);
        }

        void Channel::handleevent(Timestamp receivetime)
        {
            eventHandling_ = true;

            if (revents_ & POLLHUP && !(revents_ & POLLIN))
            {
                LOG_WARN << "Channel::handle_event() POLLHUP";
                if (closeCallback_) closeCallback_();
            }
            if (revents_ & POLLNVAL)
            {
                LOG_WARN << "Channel::handlevent() POLLNVAL";
            }
            if (revents_ & (POLLERR | POLLNVAL))
            {
                if (errorCallback_)
                    errorCallback_();
            }
            if (revents_ & (POLLIN | POLLPRI | POLLHUP))
            {
                if (readCallback_)
                    readCallback_(receivetime);
            }
            if (revents_ & POLLOUT)
            {
                if (writeCallback_)
                    writeCallback_();
            }
        }
    }
}