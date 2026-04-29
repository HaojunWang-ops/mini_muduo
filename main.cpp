#include "TcpServer.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "Buffer.h"
#include "AsyncLogging.h"
#include "Logging.h"

#include <stdio.h>
#include <memory>

std::unique_ptr<reactor::AsyncLogging> g_asynclogging;

void asyncoutpt(const char* msg, int len)
{
  g_asynclogging->append(msg, len);
}


void onMessage(const reactor::net::TcpConnectionPtr &conn, reactor::net::Buffer *buf, reactor::Timestamp receiveTime)
{
  conn->send(buf);
}
void onConnection(const reactor::net::TcpConnectionPtr &conn)
{
  conn->setTcpNoDelay(true);
}

int main()
{
  off_t rollSize = 500 * 1000 * 1000;
  
  g_asynclogging.reset(new reactor::AsyncLogging("server", rollSize, 3));

  g_asynclogging->start();
  reactor::Logger::setOutput(asyncoutpt);

  LOG_INFO << "server start";
  LOG_INFO << reactor::CurrentThread::tid();

  reactor::net::InetAddress listenAddr(9981);
  LOG_INFO << listenAddr.toIpPort().c_str();
  reactor::net::EventLoop loop;

  reactor::net::TcpServer server(&loop, listenAddr);
  server.setConnectionCallback(onConnection);
  server.setMessageCallback(onMessage);
  server.start();

  loop.loop();
}