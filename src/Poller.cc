#include "Poller.h"
#include "Channel.h"
#include "EventLoop.h"

#include <poll.h>
#include <assert.h>

namespace reactor
{
    void Poller::assertInLoopThread()
    {
        owner_loop_->assertInLoopThread();
    }

    Timestamp Poller::poll(int timeoutMs, Poller::ChannelList *activeChannels)
    {
        int numevents = ::poll(&*Pollfds_.begin(), Pollfds_.size(), -1);
        LOG_INFO << Pollfds_.size();
        if (numevents > 0)
        {
            fillActiveChannels(numevents, activeChannels);
            LOG_INFO << numevents << " events happened";
        }
        else if (numevents == 0)
        {
            LOG_INFO << "no event happened";
        }
        else
        {
            LOG_FATAL << "Poller::poll";
        }
        Timestamp now(Timestamp::now());
        return now;
    }

    void Poller::fillActiveChannels(int numevents, Poller::ChannelList *activeChannels)
    {
        for (size_t i = 0; i < Pollfds_.size() && numevents > 0; i++)
        {
            if (Pollfds_[i].revents != 0)
            {
                auto it = channels_.find(Pollfds_[i].fd);
                assert(it != channels_.end());
                Channel *channel = it->second;
                assert(channel->fd() == Pollfds_[i].fd);
                channel->set_revent(Pollfds_[i].revents);
                Pollfds_[i].revents = 0;
                activeChannels->push_back(channel);
                --numevents;
            }
        }
    }

    void Poller::upateChannel(Channel *channel)
    {
        assertInLoopThread();
        if (channel->index() == -1)
        {
            assert(channels_.find(channel->fd()) == channels_.end());
            channel->set_index(Pollfds_.size());
            channels_[channel->fd()] = channel;
            struct pollfd pfd;
            pfd.fd = channel->fd();
            pfd.events = channel->events();
            pfd.revents = 0;
            Pollfds_.push_back(pfd);
        }
        else
        {
            auto it = channels_.find(channel->fd());
            assert(it != channels_.end());
            assert(it->second == channel);
            int idx = channel->index();
            assert(0 <= idx && idx < static_cast<int>(Pollfds_.size()));
            struct pollfd &pfd = Pollfds_[idx];
            assert(pfd.fd == channel->fd() || pfd.fd == -1);
            pfd.events = channel->events();
            pfd.revents = 0;
            if (channel->isNoneEvent())
            {
                pfd.fd = -1;
            }
        }
    }
}