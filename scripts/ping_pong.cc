#include <sys/epoll.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/socket.h>
#include <errno.h>
#include <arpa/inet.h>
#include <netinet/tcp.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <memory>
#include <string>
#include <vector>
#include <chrono>
struct Conn
{
    int fd = -1;
    bool connected = false;

    size_t writeIndex_ = 0;
    size_t receivedInBlock = 0;

    bool wantWrite = false;
};

static int setNonblock(int fd)
{
    int flags = ::fcntl(fd, F_GETFL, 0);
    if (flags < 0)
        return -1;
    return ::fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

static int setNoTcpDelay(int fd)
{
    int on = 1;
    return ::setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &on, sizeof(on));
}

static void updateEvent(int epfd, Conn *c, uint32_t events)
{
    struct epoll_event event;
    memset(&event, 0, sizeof(event));
    event.events = EPOLLERR | EPOLLHUP | EPOLLRDHUP;
    event.events |= events;
    event.data.ptr = c;
    if (::epoll_ctl(epfd, EPOLL_CTL_MOD, c->fd, &event) < 0)
    {
        perror("epoll_ctl error");
    }
}

static void close_conn(int epfd, int fd, uint64_t *closedConnection, Conn *c)
{
    struct epoll_event event;
    memset(&event, 0, sizeof(event));
    ::close(fd);
    ::epoll_ctl(epfd, EPOLL_CTL_DEL, fd, &event);
    c->fd = -1;
    (*closedConnection)++;
}

static bool checkConnectionOK(int fd)
{
    int err = 0;
    socklen_t len = sizeof(err);

    if (::getsockopt(fd, SOL_SOCKET, SO_ERROR, &err, &len) < 0)
    {
        return false;
    }

    if (err != 0)
    {
        errno = err;
        printf("Connection failed, fd: %d, error: %d (%s)\n", fd, err, strerror(err));
        return false;
    }

    return true;
}

int get_local_info(int sockfd)
{
    struct sockaddr_in local_addr;
    socklen_t local_len = sizeof(local_addr);
    if (getsockname(sockfd, (struct sockaddr*) &local_addr, &local_len) < 0)
    {
        perror("getsockname error");
        return -1;
    }

    char ip_str[INET_ADDRSTRLEN];

    if (inet_ntop(AF_INET, &local_addr.sin_addr, ip_str, sizeof(ip_str)) == NULL) 
    {
        perror("inet_ntop failed");
        return -1;
    }

    int port = ntohs(local_addr.sin_port);

    /*printf("对端 IP: %s\n", ip_str);
    printf("对端 Port: %d\n", port);*/
    return port;
}
int main(int argc, char *argv[])
{
    if (argc != 6)
    {
        printf("Usage: %s <ip> <port> <block_size> <connections> <seconds>\n", argv[0]);
        printf("Example: %s 127.0.0.1 55555 1024 1000 30\n", argv[0]);
        return 1;
    }

    const char *ip = argv[1];
    int port = atoi(argv[2]);
    size_t block_size = static_cast<size_t>(atoi(argv[3]));
    int connections = atoi(argv[4]);
    int seconds = atoi(argv[5]);

    std::string payload(block_size, 'x');
    std::vector<char> readbuf(1024 * 1024);

    int epfd = ::epoll_create1(EPOLL_CLOEXEC);
    if (epfd < 0)
    {
        perror("epoll_create1 error");
        return 1;
    }

    sockaddr_in serverAddr;
    memset(&serverAddr, 0, sizeof(serverAddr));
    serverAddr.sin_family = AF_INET;
    serverAddr.sin_port = ::htons(static_cast<uint16_t>(port));

    if (::inet_pton(AF_INET, ip, &serverAddr.sin_addr) < 0)
    {
        perror("inet_pton error");
        return 1;
    }

    std::vector<std::unique_ptr<Conn>> Conns;
    Conns.reserve(connections);

    std::vector<int> conn_port;
    conn_port.reserve(connections);

    for (int i = 0; i < connections; i++)
    {
        int fd = ::socket(AF_INET, SOCK_STREAM | SOCK_NONBLOCK | SOCK_CLOEXEC, 0);
        if (fd < 0)
        {
            perror("socket error");
            return -1;
        }

        setNonblock(fd);
        setNoTcpDelay(fd);

        int ret = ::connect(fd, reinterpret_cast<sockaddr *>(&serverAddr), sizeof(serverAddr));
        // 在非阻塞socket的情况下
        // 1.ret == 0 立即连接成功
        // 2.ret == -1 && errno == EINPROGRESS 正在连接
        // 3.ret == -1 && errno != EINPROGRESS 立即失败
        if (ret < 0)
        {
            if (errno == EINPROGRESS)
            {
            }
            else
            {
                perror("connect error");
                close(fd);
                return -1;
            }
        }

        auto c = std::make_unique<Conn>();
        c->fd = fd;
        c->wantWrite = true;
        c->connected = (ret == 0);

        struct epoll_event event;
        memset(&event, 0, sizeof(event));
        event.events = EPOLLIN | EPOLLOUT | EPOLLERR | EPOLLHUP | EPOLLRDHUP;
        event.data.ptr = c.get();

        if (::epoll_ctl(epfd, EPOLL_CTL_ADD, fd, &event) < 0)
        {
            perror("epoll_ctl");
            close(fd);
            return -1;
        }

        Conns.push_back(std::move(c));
    }

    printf("connections created: %zu\n", Conns.size());

    uint64_t totalBytes = 0;
    uint64_t totalMessages = 0;
    uint64_t closedConnections = 0;

    auto start = std::chrono::steady_clock::now();
    auto end = start + std::chrono::seconds(seconds);
    auto lastPrint = start;

    const int MAX_EVENTS = 5100;

    std::vector<epoll_event> events(MAX_EVENTS);

    while (std::chrono::steady_clock::now() < end)
    {
        int n = ::epoll_wait(epfd, events.data(), MAX_EVENTS, 1000);
        if (n < 0)
        {
            if (errno == EINTR)
                continue;
            perror("epoll_wait error");
            break;
        }

        for (int i = 0; i < n; i++)
        {
            Conn *c = static_cast<Conn *>(events[i].data.ptr);
            uint32_t ev = events[i].events;

            if (ev & (EPOLLERR | EPOLLHUP | EPOLLRDHUP))
            {
                if (c->fd >= 0)
                {
                    close_conn(epfd, c->fd, &closedConnections, c);
                }
                continue;
            }

            if (c->fd < 0)
            {
                continue;
            }

            if (!c->connected && (ev & EPOLLOUT))
            {
                if (!checkConnectionOK(c->fd))
                {
                    close_conn(epfd, c->fd, &closedConnections, c);
                    continue;
                }

                c->connected = true;
                c->wantWrite = true;
                c->writeIndex_ = 0;
                
                conn_port.push_back(get_local_info(c->fd));
            }

            if (ev & EPOLLIN)
            {
                while (true)
                {
                    ssize_t nr = ::read(c->fd, readbuf.data(), sizeof(readbuf));
                    if (nr > 0)
                    {
                        totalBytes += static_cast<int64_t>(nr);
                        c->receivedInBlock += static_cast<size_t>(nr);
                        while (c->receivedInBlock >= block_size)
                        {
                            c->receivedInBlock -= block_size;
                            ++totalMessages;
                            c->wantWrite = true;
                        }
                    }
                    else if (nr == 0)
                    {
                        close_conn(epfd, c->fd, &closedConnections, c);
                        break;
                    }
                    else
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            break;
                        }
                        close_conn(epfd, c->fd, &closedConnections, c);
                        break;
                    }
                }
            }

            if (c->fd < 0)
            {
                continue;
            }

            if ((ev & EPOLLOUT) && c->wantWrite)
            {
                while (c->wantWrite && c->fd >= 0)
                {
                    const char *data = payload.data() + c->writeIndex_;
                    size_t left = block_size - c->writeIndex_;
                    ssize_t nw = ::write(c->fd, data, left);
                    if (nw > 0)
                    {
                        c->writeIndex_ += static_cast<size_t>(nw);
                        if (c->writeIndex_ == block_size)
                        {
                            c->writeIndex_ -= block_size;
                            c->wantWrite = false;
                            break;
                        }
                        continue;
                    }
                    else if (nw < 0)
                    {
                        if (errno == EAGAIN || errno == EWOULDBLOCK)
                        {
                            break;
                        }
                        close_conn(epfd, c->fd, &closedConnections, c);
                        break;
                    }
                }
            }

            if (c->fd >= 0)
            {
                uint32_t newevents = EPOLLIN;
                // 还没有建立连接，或者有数据需要写
                if (!c->connected || c->wantWrite)
                {
                    newevents |= EPOLLOUT;
                }

                updateEvent(epfd, c, newevents);
            }
        }
        auto now = std::chrono::steady_clock::now();
        if (now - lastPrint >= std::chrono::seconds(1))
        {
            double elapsed = std::chrono::duration<double>(now - start).count();
            double mib = static_cast<double>(totalBytes) / 1024.0 / 1024.0;

            printf("[%.0fs] %.2f MiB, %.2f MiB/s, messages=%lu, msg/s=%.2f, closed=%lu\n",
                   elapsed,
                   mib,
                   mib / elapsed,
                   totalMessages,
                   totalMessages / elapsed,
                   closedConnections);

            lastPrint = now;
        }
    }
    auto finish = std::chrono::steady_clock::now();
    double elapsed = std::chrono::duration<double>(finish - start).count();
    double mib = static_cast<double>(totalBytes) / 1024.0 / 1024.0;

    printf("\n==== result ====\n");
    printf("elapsed: %.3f s\n", elapsed);
    printf("connections: %zu\n", Conns.size());
    printf("closed: %lu\n", closedConnections);
    printf("bytes: %lu\n", totalBytes);
    printf("messages: %lu\n", totalMessages);
    printf("throughput: %.2f MiB/s\n", mib / elapsed);
    printf("messages/s: %.2f\n", totalMessages / elapsed);

    for (auto &c : Conns)
    {
        if (c->fd >= 0)
        {
            close(c->fd);
        }
    }

    printf("\n");
    close(epfd);
    return 0;
}
