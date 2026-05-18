# Design Notes

## 1. EventLoop 线程归属

每个 EventLoop 只属于一个线程。

Channel、Poller、TcpConnection 状态相关操作都应该在所属 EventLoop 线程中执行。跨线程操作通过 runInLoop() / queueInLoop() 投递到目标 EventLoop 线程。

queueInLoop() 会把任务加入 pendingFunctors_。如果调用者不在 EventLoop 所属线程，就通过 wakeup() 写 wakeupFd_，唤醒正在 poll/epoll_wait 的 IO 线程。

callingPendingFunctors_ 用于处理 doPendingFunctors() 执行过程中又加入新任务的情况。

## 2. TcpConnection 生命周期

TcpConnection 使用 shared_ptr 管理生命周期。

TcpServer 通过 connections_ 保存所有活跃连接。当 read() 返回 0 时，TcpConnection::handleClose() 会关闭 Channel 关注事件，并调用 closeCallback_。

closeCallback_ 最终会进入 TcpServer::removeConnection()，从 connections_ 中移除该连接。

之后 connectDestroyed() 会被投递到连接所属的 EventLoop 线程中执行，最终把 Channel 从 Poller 中移除。

简单理解：

- handleClose()：处理关闭事件
- removeConnection()：解除 TcpServer 对连接的持有
- connectDestroyed()：从 Poller 中移除 Channel

## 3. TimerQueue 取消逻辑

TimerQueue 基于 timerfd 实现定时器。

主要数据结构：

- timers_：按照过期时间排序，用于找到最早到期的 Timer
- activeTimers_：用于支持 cancel 操作
- cancelingTimers_：用于处理回调执行期间 cancel 的情况

当 timerfd 可读时，handleRead() 会调用 getExpired() 取出所有到期 Timer。

getExpired() 会把到期 Timer 从 timers_ 和 activeTimers_ 中移除。之后执行这些 Timer 的回调。

如果一个重复 Timer 在自己的回调中调用 cancel()，此时它已经不在 activeTimers_ 中了，所以无法直接删除。cancelingTimers_ 会记录这个 Timer，防止 reset() 把它重新加入。

## 4. AsyncLogging 双缓冲设计

AsyncLogging 采用前后端分离设计。

前端线程负责生成日志，并写入 currentBuffer_。当 currentBuffer_ 写满后，会把它移动到 buffers_ 中，并用 nextBuffer_ 或新 Buffer 替换 currentBuffer_，然后 notify 后端线程。

后端线程被唤醒或定时超时后，会把 buffers_ 和当前 currentBuffer_ 交换到局部变量 buffersToWrite 中。

这样文件 IO 在锁外完成，减少前端日志线程的阻塞时间。

newBuffer1 和 newBuffer2 是后端线程预分配的两个备用 Buffer，用来补充 currentBuffer_ 和 nextBuffer_，减少锁内内存分配。