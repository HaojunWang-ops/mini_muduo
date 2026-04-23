#include "TcpServer.h"
#include "EventLoop.h"
#include "InetAddress.h"

#include <stdio.h>

class Buffer;

void onConnection(const reactor::net::TcpConnectionPtr& conn)
{
  if (conn->connected())
  {
    printf("onConnection(): new connection [%s] from %s\n",
           conn->name().c_str(),
           conn->peerAddress().toIpPort().c_str());
  }
  else
  {
    printf("onConnection(): connection [%s] is down\n",
           conn->name().c_str());
  }
}


int main()
{
  printf("main(): pid = %d\n", getpid());

  reactor::net::InetAddress listenAddr(9981);
  printf("%s\n", listenAddr.toIpPort().c_str());
  reactor::net::EventLoop loop;

  reactor::net::TcpServer server(&loop, listenAddr);
  server.setConnectionCallback(onConnection);
  server.start();

  loop.loop();
}