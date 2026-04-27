#include "TcpServer.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "Buffer.h"

#include <stdio.h>

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
  printf("main(): pid = %d\n", getpid());

  reactor::net::InetAddress listenAddr(9981);
  printf("%s\n", listenAddr.toIpPort().c_str());
  reactor::net::EventLoop loop;

  reactor::net::TcpServer server(&loop, listenAddr);
  server.setConnectionCallback(onConnection);
  server.setMessageCallback(onMessage);
  server.start();

  loop.loop();
}