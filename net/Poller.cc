#include "Poller.h"
#include "Channel.h"
#include "EventLoop.h"
#include "base/Types.h"

#include <poll.h>
#include <assert.h>
#include <algorithm>

namespace reactor
{
    namespace net
    {
        void Poller::assertInLoopThread()
        {
            owner_loop_->assertInLoopThread();
        }

        Timestamp Poller::poll(int timeoutMs, Poller::ChannelList *activeChannels)
        {
            Timestamp now(Timestamp::now());
            int ret = ::poll(Pollfds_.data(), Pollfds_.size(), timeoutMs);
            if (ret > 0)
            {
                fillActiveChannels(ret, activeChannels);
                LOG_INFO << ret << "fds active";
            }
            else if (ret == 0)
            {
                LOG_INFO << "0 fd active";
            }
            else
            {
                LOG_ERROR << "Poller::poll error";
            }
            return now;
        }

        void Poller::fillActiveChannels(int numevents, Poller::ChannelList *activeChannels) const
        {
            for (auto it = Pollfds_.begin(); it != Pollfds_.end() && numevents > 0; it++)
            {
                if (it->revents)
                {
                    assert(Channels_.find(it->fd) != Channels_.end());
                    Channel *channel = (Channels_.find(it->fd))->second;
                    assert(channel->fd() == it->fd);
                    channel->set_revent(it->revents);
                    activeChannels->push_back(channel);
                    --numevents;
                }
            }
        }

        void Poller::updateChannel(Channel *channel) // 修改pollfds中的事件，不对channel中的事件修改
                                                     // 修改channel中的事件，在enable disable函数 和 fillActivityChannel函数中
        {
            assertInLoopThread();
            LOG_INFO << "Poller::updateChannel: " << "fd = " << channel->fd() << " events = " << channel->events();
            int fd = channel->fd();
            if (Channels_.find(fd) == Channels_.end())
            {
                assert(channel->index() < 0);
                Channels_.insert({fd, channel});
                int idx = static_cast<int>(Pollfds_.size());
                channel->set_index(idx);
                struct pollfd pfd;
                pfd.fd = fd;
                pfd.events = static_cast<short>(channel->events());
                pfd.revents = 0;
                Pollfds_.push_back(pfd);
            }
            else
            {
                assert(Channels_.find(fd) != Channels_.end());
                assert(Channels_[fd] == channel);
                int idx = channel->index();
                assert(0 <= idx && idx < static_cast<int>(Pollfds_.size()));
                struct pollfd &pfd = Pollfds_[idx];
                assert(pfd.fd == fd || pfd.fd == -fd - 1);
                pfd.events = static_cast<short>(channel->events());
                pfd.revents = 0;
                if (channel->isNoneEvent())
                {
                    pfd.fd = -fd - 1; // poll忽略Pollfds_中负数的fd
                }
                else
                {
                    pfd.fd = fd;
                }
            }
        }

        void Poller::removeChannel(Channel *channel)
        {
            assertInLoopThread();
            LOG_INFO << "fd = " << channel->fd();
            assert(Channels_.find(channel->fd()) != Channels_.end());
            assert(Channels_[channel->fd()] == channel);
            assert(channel->isNoneEvent());

            int idx = channel->index();
            assert(0 <= idx && idx < static_cast<int>(Pollfds_.size()));
            const struct pollfd &pfd = Pollfds_[idx];
            assert(pfd.fd == -channel->fd() - 1 && pfd.events == channel->events());
            (void)pfd; // 是否已经disableAll()
            ssize_t n = Channels_.erase(channel->fd());
            assert(n == 1);
            (void)n;
            if (implicit_cast<size_t>(idx) == Pollfds_.size() - 1)
            {
                Pollfds_.pop_back();
            }
            else
            {
                int channelAtEnd = Pollfds_.back().fd;
                if (channelAtEnd < 0) // 判断最后一个fd的正负
                {
                    channelAtEnd = -channelAtEnd - 1;
                }
                std::iter_swap(Pollfds_.begin() + idx, Pollfds_.end() - 1);
                Channels_[channelAtEnd]->set_index(idx);
                Pollfds_.pop_back();
            }
        }
    }
}