#include "TcpServer.h"
#include "Acceptor.h"
#include "EventLoop.h"
#include "SocketsOps.h"

#include <stdio.h>

namespace reactor
{
    namespace net
    {
        TcpServer::TcpServer(EventLoop* loop, const InetAddress& listenAddr)
            : loop_(loop),
              name_(listenAddr.toIpPort()),
              acceptor_(new Acceptor(loop, listenAddr, true)),
              started_(false),
              nextConnId_(1)
        {
            acceptor_->setNewConnectionCallback([this](int sockfd, const InetAddress& listenAddr){
                this->newConnection(sockfd, listenAddr);
            });
        }

        TcpServer::~TcpServer()
        {

        }

        void TcpServer::start()
        {
            if (!started_)
            {
                started_ = true;
            }

            if (!acceptor_->listening())
            {
                loop_->runInLoop([this](){
                    acceptor_->listen();
                });
            }
        }

        void TcpServer::newConnection(int connfd, const InetAddress& peerAddr)
        {
            loop_->assertInLoopThread();
            char buf[32];
            snprintf(buf, sizeof buf, "#%d", nextConnId_);
            ++nextConnId_;

            std::string connName = name_ + buf;
            LOG_INFO << "TcpServer::newConnection [" << name_
                << "] - new connection [" << connName
                << "] from " << peerAddr.toIpPort();
            
            InetAddress localAddress(sockets::getLocalAddr(connfd));
            InetAddress peerAddrress(sockets::getPeerAddr(connfd));
            TcpConnectionPtr conn (new TcpConnection(loop_, connName, connfd, localAddress, peerAddrress));
            connections_[connName] = conn;
            conn->setConnectionCallback(connectionCallback_);
            conn->setMessageCallback(messageCallback_);
            conn->setCloseCallback([this](const TcpConnectionPtr& tcpConnectionPtr){
                this->removeConnection(tcpConnectionPtr);
            });
            conn->setWriteCompleteCallback(writeCompleteCallback_);
            conn->setHighWaterMarkCallback(highWaterMarkCallback_, highWaterMark_);
            conn->connectEstablished();
        }

        void TcpServer::removeConnection(const TcpConnectionPtr& conn){
           loop_->runInLoop([this, conn](){
                this->removeConnectionInLoop(conn);
           }); 
        }
        void TcpServer::removeConnectionInLoop(const TcpConnectionPtr& conn){
            loop_->assertInLoopThread();
            
            LOG_INFO << "TcpServer::removeConnectionInLoop [" << name_
                << "] - connection " << conn->name();

            size_t n = connections_.erase(conn->name());
            assert(n == 1); (void) n;

            loop_->queueInLoop([this, conn](){
                conn->connectDestroyed();
            });
        }
    }
}