#pragma once

#include "base/Timestamp.h"
#include "base/noncopyable.h"

#include <functional>

namespace reactor
{
    namespace net
    {
        class EventLoop;

        class Channel : noncopyable
        {
        public:
            typedef std::function<void()> Callback;
            typedef std::function<void(Timestamp)> ReadCallback;
            
            Channel(EventLoop *loop, int fd);
            ~Channel();

            void handleevent(Timestamp receivetime);
            void update();
            void set_revents(int revents);

            void setReadCallback(ReadCallback cb) { readCallback_ = cb; }
            void setWriteCallback(Callback cb) { writeCallback_ = cb; }
            void setErrorCallback(Callback cb) { errorCallback_ = cb; }
            void setCloseCallback(Callback cb) { closeCallback_ = cb; }
            
            void enableRead() { events_ |= kReadEvent; update(); }
            void enableWrite() { events_ |= kWriteEvent; update(); }
            void disableWriting() {events_ &= ~kWriteEvent; update(); }
            void disableAll() {
                events_ = kNoneEvent; 
                update(); 
            }
            bool isWriting() const { return events_ & kWriteEvent; }
            
            int fd() { return fd_; }
            int index() { return index_; }
            int events() { return events_; }
            EventLoop *ownerLoop() { return ownerLoop_; }
            void set_index(int index) { index_ = index; }
            void set_revent(int revents) { revents_ = revents; }

            bool isNoneEvent() { return events_ == kNoneEvent; }
        private:
            EventLoop *ownerLoop_;
            int fd_;
            int index_; // used by Poller
            static const int kNoneEvent;
            static const int kReadEvent;
            static const int kWriteEvent;
            int events_;
            int revents_;

            bool eventHandling_;

            ReadCallback readCallback_;
            Callback writeCallback_;
            Callback errorCallback_;
            Callback closeCallback_;
        };
    }
}