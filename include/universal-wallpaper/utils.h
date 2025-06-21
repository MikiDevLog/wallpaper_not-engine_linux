#pragma once

#include <string>
#include <vector>

enum class LogLevel {
    DEBUG,
    INFO,
    WARN,
    ERROR
};

void set_log_level(LogLevel level);
void log_debug(const std::string& message);
void log_info(const std::string& message);
void log_warn(const std::string& message);
void log_error(const std::string& message);

// File utilities
bool file_exists(const std::string& path);
bool is_video_file(const std::string& path);
bool is_image_file(const std::string& path);

// String utilities
std::vector<std::string> split_string(const std::string& str, char delimiter);
std::string trim_string(const std::string& str);

// Process utilities
void daemonize();
bool is_wayland_session();
bool is_x11_session();
