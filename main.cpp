#include "EventLoop.h"
#include "TcpServer.h"
#include "InetAddress.h"
#include "Buffer.h"
#include "Logging.h"
#include "AsyncLogging.h"

#include <memory>

using namespace reactor;
using namespace reactor::net;

std::unique_ptr<AsyncLogging> g_asynclogging;

void asyncoutpt(const char* msg, int len)
{
  g_asynclogging->append(msg, len);
}

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
  g_asynclogging.reset(new AsyncLogging("server", 500 * 1000 * 1000));
  g_asynclogging->start();
  Logger::setOutput(asyncoutpt);

  EventLoop loop;
  InetAddress listenAddr(9981);
  TcpServer server(&loop, listenAddr, "EchoServer");
  server.setThreadNum(6);

  server.setConnectionCallback(onConnection);
  server.setMessageCallback(onMessage);

  server.start();  

  loop.loop();

}

