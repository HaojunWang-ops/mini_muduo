#pragma once

#include <functional>

namespace reactor
{
    class EventLoop;

    class Channel
    {
    public:
        typedef std::function<void()> callback;
        Channel(EventLoop *loop, int fd);

        Channel(const Channel &) = delete;
        Channel &operator=(const Channel &) = delete;

        void handleevent();
        void update();
        void set_revents(int revents);

        void setReadCallback(callback cb) { ReadCallback_ = cb; }
        void setWriteCallback(callback cb) { WriteCallback_ = cb; }
        void setErrorCallback(callback cb) { ErrorCallback_ = cb; }

        void enableRead()
        {
            events_ |= kReadEvent;
            update();
        }
        void enableWrite()
        {
            events_ |= kWriteEvent;
            update();
        }
        void disenbleall()
        {
            events_ = kNoneEvent;
            update();
        }

        int fd() { return fd_; }
        int index() { return index_; }
        int events() { return events_; }
        EventLoop *ownerLoop() { return ownerLoop_; }
        void set_index(int index) { index_ = index; }
        void set_revent(int revents) { revents_ = revents; }

        bool isNoneEvent() { return revents_ == kNoneEvent; }

    private:
        EventLoop *ownerLoop_;
        int fd_;
        int index_; // used by Poller

        static const int kNoneEvent;
        static const int kReadEvent;
        static const int kWriteEvent;

        int events_;
        int revents_;
        callback ReadCallback_;
        callback WriteCallback_;
        callback ErrorCallback_;
    };
}