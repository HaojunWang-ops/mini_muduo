#include "Channel.h"
#include "EventLoop.h"

#include <poll.h>

namespace reactor
{
    const int Channel::kNoneEvent = 0;
    const int Channel::kReadEvent = POLLIN | POLLPRI;
    const int Channel::kWriteEvent = POLLOUT;

    Channel::Channel(EventLoop *loop, int fd)
        : ownerLoop_(loop),
          fd_(fd),
          index_(-1),
          events_(0),
          revents_(0)
    {
    }

    void Channel::update()
    {
        ownerLoop_->updateChannel(this);
    }

    void Channel::handleevent()
    {
        if (revents_ & POLLNVAL)
        {
            LOG_WARN << "Channel::handlevent() POLLNVAL";
        }
        if (revents_ & (POLLERR | POLLNVAL))
        {
            if (ErrorCallback_)
                ErrorCallback_();
        }
        if (revents_ & (POLLIN | POLLPRI | POLLHUP))
        {
            if (ReadCallback_)
                ReadCallback_();
        }
        if (revents_ & POLLOUT)
        {
            if (WriteCallback_)
                WriteCallback_();
        }
    }
}