#pragma once

#include "Timestamp.h"

#include <iostream>
#include <sstream>
#include <string>
#include <cstring>
#include <cstdlib>

namespace reactor
{
    enum LogLevel
    {
        INFO,
        WARN,
        ERROR,
        FATAL,
    };

    inline const char *getBasename(const char *filepath)
    {
        const char *slash = strrchr(filepath, '/');
        return slash ? slash + 1 : filepath;
    }

    class Logger
    {
    public:
        Logger(const char *file, int line, LogLevel level)
            : basename_(getBasename(file)),
              line_(line),
              level_(level)
        {
            Timestamp now = Timestamp::now();
            stream_ << "[" << now.toFormattedString() << "] ";
            stream_ << "[" << levelToString(level_) << "] ";
        }

        ~Logger()
        {
            stream_ << " - " << basename_ << ":" << line_ << "\n";
            std::string log_msg = stream_.str();
            if (level_ >= ERROR)
            {
                std::cerr << log_msg;
                std::cerr.flush();
                if (level_ == FATAL)
                {
                    std::abort();
                }
            }
            else
            {
                std::cout << log_msg;
                std::cout.flush();
            }
        }

        std::ostringstream &stream() { return stream_; }

    private:
        const char *levelToString(LogLevel level)
        {
            switch (level)
            {
            case INFO:
                return "INFO";
            case WARN:
                return "WARN";
            case ERROR:
                return "ERROR";
            case FATAL:
                return "FATAL";
            default:
                return "UNKNOWN";
            }
        }

        std::ostringstream stream_;
        const char *basename_;
        int line_;
        LogLevel level_;
    };

#define LOG_INFO Logger(__FILE__, __LINE__, INFO).stream()
#define LOG_WARN Logger(__FILE__, __LINE__, WARN).stream()
#define LOG_ERROR Logger(__FILE__, __LINE__, ERROR).stream()
#define LOG_FATAL Logger(__FILE__, __LINE__, FATAL).stream()
}