#include "currentThread.h"

#include <stdlib.h>
#include <type_traits>
#include <unistd.h>
#include <sys/syscall.h>
#include <cstdio>
namespace reactor
{
    namespace CurrentThread
    {
        __thread int t_cachedTid = 0;
        __thread char t_tidString[32];
        __thread int t_tidStringLength;
        __thread const char *t_threadName = "unknown";
        static_assert(std::is_same<int, pid_t>::value, "pid should be int");

        void cacheTid()
        {
            int gettid = static_cast<pid_t>(::syscall(SYS_gettid));
            if (t_cachedTid == 0)
            {
                t_cachedTid = gettid;
                t_tidStringLength = snprintf(t_tidString, sizeof t_tidString, "%5d", t_cachedTid);
            }
        }

    }
}