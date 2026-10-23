#include "logger.h"
#include <iostream>
#include <fstream>
#include <ctime>
#include <mutex>
#include <stdexcept>
#include <utility>

LogLevel Logger::minLogLevel_ = LogLevel::Info;
std::ofstream Logger::fileStream_;
std::string Logger::currentFilename_;

namespace {
std::mutex logger_mutex;
}

void Logger::configure(LogLevel level, const std::string& filename) {
    std::lock_guard lock(logger_mutex);
    std::ofstream configured_file;
    if (!filename.empty()) {
        configured_file.open(filename, std::ios::app);
        if (!configured_file) {
            throw std::runtime_error("Cannot open log file: " + filename);
        }
    }
    if (fileStream_.is_open()) fileStream_.close();
    fileStream_ = std::move(configured_file);
    currentFilename_ = filename;
    minLogLevel_ = level;
}

void Logger::debug(const std::string& message) {
    log(LogLevel::Debug, message);
}

void Logger::info(const std::string& message) {
    log(LogLevel::Info, message);
}

void Logger::warning(const std::string& message) {
    log(LogLevel::Warning, message);
}

void Logger::error(const std::string& message) {
    log(LogLevel::Error, message);
}

void Logger::log(LogLevel level, const std::string& message) {
    std::lock_guard lock(logger_mutex);
    if (level < minLogLevel_) return;

    const char* levelStr = nullptr;
    switch (level) {
    case LogLevel::Debug: levelStr = "DEBUG"; break;
    case LogLevel::Info: levelStr = "INFO"; break;
    case LogLevel::Warning: levelStr = "WARNING"; break;
    case LogLevel::Error: levelStr = "ERROR"; break;
	default: levelStr = "UNKNOWN"; break;
    } 

    std::string logMessage = "[" + getTimestamp() + "] [" + levelStr + "] " + message;

    std::cout << logMessage << std::endl;

    if (fileStream_.is_open()) {
        fileStream_ << logMessage << std::endl;
    }
}

std::string Logger::getTimestamp() {
    auto now = std::time(nullptr);
    std::tm tm_buf = {};
    std::tm* tm = nullptr;

#ifdef _WIN32
    localtime_s(&tm_buf, &now);
    tm = &tm_buf;
#else
    localtime_r(&now, &tm_buf);
    tm = &tm_buf;
#endif

    char buffer[20];
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", tm);
    return std::string(buffer);
}

void Logger::clear() {
    {
        std::lock_guard lock(logger_mutex);
        if (currentFilename_.empty()) return;

        if (fileStream_.is_open()) fileStream_.close();
        fileStream_.open(currentFilename_, std::ios::out | std::ios::trunc);
        if (!fileStream_) return;
        fileStream_.close();
        fileStream_.open(currentFilename_, std::ios::app);
        if (!fileStream_) return;
    }
    log_info("Logger: Log file cleared");
}

void log_debug(const std::string& message) {
    Logger::debug(message);
}

void log_info(const std::string& message) {
    Logger::info(message);
}

void log_clear() {
    Logger::clear();
}

void log_warning(const std::string& message) {
    Logger::warning(message);
}

void log_error(const std::string& message) {
    Logger::error(message);
}
