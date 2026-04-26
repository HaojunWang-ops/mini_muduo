#pragma once

#include "Callbacks.h"
#include "TcpConnection.h"
#include "base/noncopyable.h"
#include "InetAddress.h"

#include <map>
#include <memory>

namespace reactor
{
    namespace net
    {
        class Acceptor;
        class EventLoop;

        class TcpServer : noncopyable
        {
        public:
            TcpServer(EventLoop* loop, const InetAddress& listenAddr);
            ~TcpServer();

            void start(); //multiple start and thread safe
            
            void setConnectionCallback(const ConnectionCallback& cb)
            { connectionCallback_ = cb; } // not thread safe

            void setMessageCallback(const MessageCallback& cb)
            { messageCallback_ = cb; }    // not thread safe

            void setWriteCallback(const WriteCompleteCallback& cb)
            { writeCompleteCallback_ = cb; }

            void setHightWaterMarkCallback(const HighWaterMarkCallback& cb, size_t highWaterMark)
            {
                highWaterMarkCallback_ = cb;
                highWaterMark_ = highWaterMark;
            }
        private:
            void newConnection(int connfd, const InetAddress& peeraddr);
            void removeConnection(const TcpConnectionPtr& conn);
            void removeConnectionInLoop(const TcpConnectionPtr& conn);
            
            typedef std::map<std::string, TcpConnectionPtr> ConnectionMap;

            EventLoop* loop_;
            const std::string name_;
            std::unique_ptr<Acceptor> acceptor_;
            
            ConnectionCallback connectionCallback_;
            MessageCallback messageCallback_;
            WriteCompleteCallback writeCompleteCallback_;
            HighWaterMarkCallback highWaterMarkCallback_;

            size_t highWaterMark_;
            bool started_;
            int nextConnId_;
            ConnectionMap connections_;
        };
    }
}