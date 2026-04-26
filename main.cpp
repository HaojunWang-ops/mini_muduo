#include "TcpServer.h"
#include "EventLoop.h"
#include "InetAddress.h"
#include "Buffer.h"

#include <stdio.h>

void onMessage(const reactor::net::TcpConnectionPtr& conn, reactor::net::Buffer* buf, reactor::Timestamp receiveTime)
{
  printf("onMessage(): received %zd bytes from connection [%s] at %s\n",
          buf->readableBytes(),
        conn->name().c_str(),
        receiveTime.toFormattedString().c_str());

  printf("onMessage(): [%s]\n", buf->retrieveAsString().c_str());
}
void onConnection(const reactor::net::TcpConnectionPtr& conn)
{
  if (conn->connected())
  {
    printf("onConnection(): new connection [%s] from %s\n",
           conn->name().c_str(),
           conn->peerAddress().toIpPort().c_str());
    
    int sleepseconds = 100;
    ::sleep(sleepseconds);
    conn->send("fsdsfsaa");
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
  server.setMessageCallback(onMessage);
  server.start();

  loop.loop();
}