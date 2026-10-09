#pragma once
#include <string>
enum class LogLevel {
    Debug = 0,
    Info = 1,
    Warning = 2,
    Error = 3
};

class Logger {
public:
    static void setLogLevel(LogLevel level);
    static void setLogFile(const std::string& filename);

    static void debug(const std::string& message);
    static void info(const std::string& message);
    static void warning(const std::string& message);
    static void error(const std::string& message);

    static void clear();

private:
    static void log(LogLevel level, const std::string& message);

    static std::string getTimestamp();

    static LogLevel minLogLevel_;
    static std::ofstream fileStream_;
    static std::string currentFilename_;
};
void log_set_level(LogLevel level);
void log_set_file(const std::string& filename);
void log_clear();

void log_debug(const std::string& message);
void log_info(const std::string& message);
void log_warning(const std::string& message);
void log_error(const std::string& message);