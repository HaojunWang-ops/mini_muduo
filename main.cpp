#include "EventLoop.h"
#include "TcpServer.h"
#include "InetAddress.h"
#include "Buffer.h"
#include "Logging.h"

using namespace reactor;
using namespace reactor::net;

void onConnection(const TcpConnectionPtr& conn)
{
    LOG_INFO << "connection " << conn->peerAddress().toIpPort()
             << " -> " << conn->localAddress().toIpPort()
             << " is " << (conn->connected() ? "UP" : "DOWN");
}

void onMessage(const TcpConnectionPtr& conn, Buffer* buf, Timestamp)
{
    std::string msg = buf->retrieveAsString();
    conn->send(msg);
}

int main()
{
  EventLoop loop;
  InetAddress listenAddr(9981);
  TcpServer server(&loop, listenAddr, "EchoServer");

  server.setConnectionCallback(onConnection);
  server.setMessageCallback(onMessage);

  server.start();
  loop.loop();
}

