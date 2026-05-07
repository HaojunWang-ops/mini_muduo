#include "FileUtil.h"
#include "LogFile.h"
#include "Logging.h"

#include <assert.h>
#include <errno.h>
#include <fcntl.h>
#include <stdio.h>
#include <sys/stat.h>
#include <unistd.h>

using namespace reactor;

FileUtil::AppendFile::AppendFile(StringArg filename)
    : fp_(::fopen(filename.c_str(), "ae")),
      writtenBytes_(0)
{
    assert(fp_ != 0);
    ::setbuffer(fp_, buffer_, sizeof buffer_);
}

FileUtil::AppendFile::~AppendFile()
{
    ::fclose(fp_);
}


//最底层，负责系统调用将字符串写到fp_中
void FileUtil::AppendFile::append(const char* logline, size_t len)
{
    size_t written = 0;
    while (written < len)
    {
        size_t remain = len - written;
        size_t n = write(logline, remain);
        if (n < remain)
        {
            int err = ferror(fp_);
            if (err)
            {
                fprintf(stderr, "AppendFile::append() failed %s\n", strerror_tl(err));
                break;
            }
        }
        written += n;
    }
    
    writtenBytes_ += written;
}

void FileUtil::AppendFile::flush()
{
    fflush(fp_);
}

size_t FileUtil::AppendFile::write(const char* logline, size_t len)
{
    size_t n = fwrite_unlocked(logline, 1, len, fp_);
    return n;
}

