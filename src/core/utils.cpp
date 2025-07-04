#include "universal-wallpaper/utils.h"
#include <iostream>
#include <fstream>
#include <algorithm>
#include <sstream>
#include <unistd.h>
#include <sys/stat.h>
#include <cstdlib>
#include <csignal>

static LogLevel current_log_level = LogLevel::LOG_INFO;

void set_log_level(LogLevel level) {
    current_log_level = level;
}

void log_debug(const std::string& message) {
    if (current_log_level <= LogLevel::LOG_DEBUG) {
        std::cout << "[DEBUG] " << message << std::endl;
    }
}

void log_info(const std::string& message) {
    if (current_log_level <= LogLevel::LOG_INFO) {
        std::cout << "[INFO] " << message << std::endl;
    }
}

void log_warn(const std::string& message) {
    if (current_log_level <= LogLevel::LOG_WARN) {
        std::cout << "[WARN] " << message << std::endl;
    }
}

void log_error(const std::string& message) {
    if (current_log_level <= LogLevel::LOG_ERROR) {
        std::cerr << "[ERROR] " << message << std::endl;
    }
}

bool file_exists(const std::string& path) {
    struct stat buffer;
    return (stat(path.c_str(), &buffer) == 0);
}

bool is_video_file(const std::string& path) {
    std::string ext = path.substr(path.find_last_of('.') + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    return ext == "mp4" || ext == "mkv" || ext == "avi" || ext == "mov" ||
           ext == "webm" || ext == "m4v" || ext == "wmv" || ext == "flv" ||
           ext == "ogv" || ext == "3gp" || ext == "gif";
}

bool is_image_file(const std::string& path) {
    std::string ext = path.substr(path.find_last_of('.') + 1);
    std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
    
    return ext == "jpg" || ext == "jpeg" || ext == "png" || ext == "bmp" ||
           ext == "tiff" || ext == "tif" || ext == "webp" || ext == "svg";
}

std::vector<std::string> split_string(const std::string& str, char delimiter) {
    std::vector<std::string> tokens;
    std::string token;
    std::istringstream tokenStream(str);
    
    while (std::getline(tokenStream, token, delimiter)) {
        tokens.push_back(token);
    }
    
    return tokens;
}

std::string trim_string(const std::string& str) {
    size_t start = str.find_first_not_of(" \t\n\r");
    if (start == std::string::npos) return "";
    
    size_t end = str.find_last_not_of(" \t\n\r");
    return str.substr(start, end - start + 1);
}

void daemonize() {
    pid_t pid = fork();
    
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    
    if (setsid() < 0) {
        exit(EXIT_FAILURE);
    }
    
    signal(SIGCHLD, SIG_IGN);
    signal(SIGHUP, SIG_IGN);
    
    pid = fork();
    
    if (pid < 0) {
        exit(EXIT_FAILURE);
    }
    
    if (pid > 0) {
        exit(EXIT_SUCCESS);
    }
    
    umask(0);
    chdir("/");
    
    for (int x = sysconf(_SC_OPEN_MAX); x >= 0; x--) {
        close(x);
    }
}

bool is_wayland_session() {
    return getenv("WAYLAND_DISPLAY") != nullptr;
}

bool is_x11_session() {
    return getenv("DISPLAY") != nullptr;
}
