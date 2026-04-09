#pragma once

#include "Timestamp.h"
#include "logger.h"

#include <vector>
#include <map>
#include <chrono>
#include <poll.h>

namespace reactor
{
    class EventLoop;
    class Channel;

    class Poller
    {
    public:
        typedef std::vector<Channel *> ChannelList;
        Poller(EventLoop *loop)
            : owner_loop_(loop)
        {
        }

        Poller(const Poller &) = delete;
        Poller &operator=(const Poller &) = delete;

        Timestamp poll(int timeoutMs, ChannelList *activeChannels);
        void upateChannel(Channel *channel);

        void assertInLoopThread();

    private:
        void fillActiveChannels(int numsevents, ChannelList *activeChannels);

        typedef std::vector<struct pollfd> PollFdList;
        typedef std::map<int, Channel *> ChannelMap;

        EventLoop *owner_loop_;
        PollFdList Pollfds_;
        ChannelMap channels_;
    };
}