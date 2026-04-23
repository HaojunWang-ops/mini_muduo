#include "TcpConnection.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Socket.h"
#include "logger.h"
#include "SocketsOps.h"

#include <memory>

namespace reactor
{
    namespace net
    {
        TcpConnection::TcpConnection(EventLoop *loop, const std::string &name,
                                     int connfd, const InetAddress &localAddr, const InetAddress &peerAddr)
            : loop_(loop),
              name_(name),
              state_(kConnecting),
              socket_(new Socket(connfd)),
              channel_(new Channel(loop, connfd)),
              localAddr_(localAddr),
              peerAddr_(peerAddr)
        {
            LOG_INFO << "TcpConnection::ctor[" << name_ << "] at " << this
                     << " fd=" << connfd;

            channel_->setReadCallback([this](Timestamp receiveTime)
                                      { this->handleRead(receiveTime); });
            channel_->setWriteCallback([this]()
                                       { this->handlewrite(); });
            channel_->setErrorCallback([this]()
                                       { this->handleError(); });

            channel_->setCloseCallback([this]()
                                       { this->handleClose(); });
        }

        TcpConnection::~TcpConnection()
        {
            LOG_INFO << "TcpConnection::dtor[" << name_ << "] at " << this
                     << " fd=" << channel_->fd();
        }

        const char *TcpConnection::stateToString() const
        {
            switch (state_)
            {
            case kDisconnected:
                return "kDisconnected";
            case kConnecting:
                return "kConnecting";
            case kConnected:
                return "kConnected";
            case kDisconnecting:
                return "kDisconnecting";
            default:
                return "unknown state";
            }
        }

        void TcpConnection::connectEstablished()
        {
            loop_->assertInLoopThread();
            assert(state_ == kConnecting);
            setState(kConnected);
            channel_->enableRead();
            connectionCallback_(shared_from_this());
        }

        void TcpConnection::connectDestroyed()
        {
            loop_->assertInLoopThread();
            if (state_ == kConnected)
            {
                setState(kDisconnected);
                channel_->disableAll();
                connectionCallback_(shared_from_this());
            }
            loop_->removeChannel(channel_.get());
        }

        void TcpConnection::handleRead(Timestamp receivetime)
        {
            loop_->assertInLoopThread();
            int savedError = 0;
            char buf[4096];
            ssize_t n = ::read(channel_->fd(), buf, sizeof(buf));
            if (n > 0)
            {

            }
            else if (n == 0)
            {
                handleClose();
            }
            else
            {                
                errno = savedError;
                LOG_ERROR << "TcpConnection::handleRead()";
                handleError();
            }
        }

        void TcpConnection::handlewrite()
        {

        }

        void TcpConnection::handleError()
        {
            int err = sockets::getSocketError(channel_->fd());
            LOG_ERROR << "TcpConnection::handleError [" << name_
                << "] - SO_ERROR = " << err << " " << strerror(err); 
        }

        void TcpConnection::handleClose()
        {
            loop_->assertInLoopThread();
            LOG_INFO << "fd = " << channel_->fd() << " state = " << stateToString();
            assert(state_ == kConnected || state_ == kConnecting);
            setState(kDisconnected);
            channel_->disableAll();

            TcpConnectionPtr guardThis(shared_from_this());
            connectionCallback_(guardThis);
            closeCallback_(guardThis);
        }
    }
}