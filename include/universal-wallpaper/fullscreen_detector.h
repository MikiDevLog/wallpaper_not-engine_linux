#pragma once

#include <atomic>
#include <memory>
#include <thread>
#include <chrono>

class FullscreenDetector {
public:
    FullscreenDetector();
    ~FullscreenDetector();
    
    bool initialize();
    void cleanup();
    
    void set_enabled(bool enabled);
    bool is_enabled() const;
    
    bool is_fullscreen_app_active() const;
    
private:
    void monitor_fullscreen_apps();
    bool check_fullscreen_windows();
    
    std::atomic<bool> enabled_{false};
    std::atomic<bool> fullscreen_app_active_{false};
    std::atomic<bool> should_stop_{false};
    
    std::unique_ptr<std::thread> monitor_thread_;
    
    // Platform-specific detection
    bool check_x11_fullscreen();
    bool check_wayland_fullscreen();
    
    void* display_ = nullptr;  // X11 display or Wayland display
    bool is_wayland_ = false;
};
