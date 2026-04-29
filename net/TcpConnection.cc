#include "TcpConnection.h"
#include "Channel.h"
#include "EventLoop.h"
#include "Socket.h"
#include "Logging.h"
#include "SocketsOps.h"
#include "base/StringPiece.h"

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
            LOG_TRACE << "TcpConnection::ctor[" << name_ << "] at " << this
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
            LOG_TRACE << "TcpConnection::dtor[" << name_ << "] at " << this
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

        void TcpConnection::send(const void *data, int len)
        {
            send(StringPiece(static_cast<const char *>(data), len));
        }

        void TcpConnection::send(const StringPiece &message)
        {
            if (state_ == kConnected)
            {
                if (loop_->isInLoopThread())
                {
                    sendInLoop(message);
                }
                else
                {
                    loop_->runInLoop([this, message]()
                                     { this->sendInLoop(message.as_string()); });
                }
            }
        }

        void TcpConnection::send(Buffer *buf)
        {
            if (state_ == kConnected)
            {
                if (loop_->isInLoopThread())
                {
                    sendInLoop(buf->peek(), buf->readableBytes());
                    buf->retrieveAll();
                }
                else
                {
                    loop_->queueInLoop([conn = shared_from_this(), str = buf->retrieveAsString()]()
                                       { conn->sendInLoop(str); });
                }
            }
        }
        void TcpConnection::sendInLoop(const StringPiece &message)
        {
            sendInLoop(message.data(), message.size());
        }

        void TcpConnection::sendInLoop(const void *data, size_t len)
        {
            loop_->assertInLoopThread();
            ssize_t nwrote = 0;
            size_t remaining = len;
            bool faultError = false;
            if (state_ == kDisconnected)
            {
                LOG_WARN << "disconnected, give up writing";
                return;
            }

            if (!channel_->isWriting() && outputBuffer_.readableBytes() == 0)
            {
                nwrote = sockets::write(channel_->fd(), data, len);
                if (nwrote >= 0)
                {
                    remaining = len - nwrote;
                    if (remaining == 0 && writeCompleteCallback_)
                    {
                        loop_->queueInLoop([conn = shared_from_this()]
                                           { conn->writeCompleteCallback_(conn); });
                    }
                }
                else
                {
                    nwrote = 0;
                    if (errno != EWOULDBLOCK)
                    {
                        LOG_SYSERR << " sendInLoop() errno";
                        if (errno == EPIPE || errno == ECONNRESET)
                        {
                            faultError = true;
                        }
                    }
                }

                assert(remaining <= len);
                if (!faultError && remaining > 0)
                {
                    size_t oldLen = outputBuffer_.readableBytes();
                    if (oldLen + remaining >= highWaterMark_ && oldLen < highWaterMark_ && highwaterMarkCallback_)
                    {
                        loop_->queueInLoop([conn = shared_from_this(), highWaterSize = oldLen + remaining]()
                                           { conn->highwaterMarkCallback_(conn, highWaterSize); });
                    }
                    outputBuffer_.append((static_cast<const char *>(data)) + nwrote, remaining);
                    if (!channel_->isWriting())
                    {
                        channel_->enableWrite();
                    }
                }
            }
        }

        void TcpConnection::shutdown()
        {
            if (state_ == kConnected)
            {
                setState(kDisconnecting);
                loop_->queueInLoop([this]()
                                   { this->shutdownInLoop(); });
            }
        }

        void TcpConnection::shutdownInLoop()
        {
            if (!channel_->isWriting())
            {
                socket_->shundownWrite();
            }
        }

        void TcpConnection::setTcpNoDelay(bool on)
        {
            socket_->setTcpNoDelay(on);
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
            LOG_TRACE << "Ready to remove channel. FD = " << channel_->fd()
                     << ", state_ = " << state_
                     << ", events_ = " << channel_->events();
            loop_->removeChannel(channel_.get());
        }

        void TcpConnection::handleRead(Timestamp receiveTime)
        {
            loop_->assertInLoopThread();
            int savedErrno = 0;
            ssize_t n = inputBuffer_.readFd(channel_->fd(), &savedErrno);
            if (n > 0)
            {
                messageCallback_(shared_from_this(), &inputBuffer_, receiveTime);
            }
            else if (n == 0)
            {
                handleClose();
            }
            else
            {
                errno = savedErrno;
                LOG_SYSERR << "TcpConnection::handleRead()";
                handleError();
            }
        }

        void TcpConnection::handlewrite()
        {
            if (channel_->isWriting())
            {
                size_t n = sockets::write(channel_->fd(), outputBuffer_.peek(), outputBuffer_.readableBytes());

                if (n > 0)
                {
                    outputBuffer_.retrieve(n);

                    if (outputBuffer_.readableBytes())
                    {
                        channel_->disableWriting();
                        if (writeCompleteCallback_)
                        {
                            loop_->queueInLoop([conn = shared_from_this()](){
                                conn->writeCompleteCallback_(conn);
                            });
                        }

                        if (state_ == kDisconnecting)
                        {
                            shutdownInLoop();
                        }
                    }
                }
                else
                {
                    LOG_SYSERR << "TcpConnection::handleWrite()";
                }
            }
            else
            {
                LOG_SYSERR<< "TcpConnection::handleWrite()";
            }
        }

        void TcpConnection::handleError()
        {
            int err = sockets::getSocketError(channel_->fd());
            LOG_SYSERR<< "TcpConnection::handleError [" << name_
                      << "] - SO_ERROR = " << err << " " << strerror(err);
        }

        void TcpConnection::handleClose()
        {
            loop_->assertInLoopThread();
            assert(state_ == kConnected || state_ == kConnecting);
            setState(kDisconnected);
            channel_->disableAll();
            LOG_INFO << "Ready to remove channel. FD = " << channel_->fd()
                     << ", state_ = " << state_
                     << ", events_ = " << channel_->events();
            TcpConnectionPtr guardThis(shared_from_this());
            connectionCallback_(guardThis);
            closeCallback_(guardThis);
        }
    }
}