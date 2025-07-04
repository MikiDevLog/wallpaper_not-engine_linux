#include "universal-wallpaper/fullscreen_detector.h"
#include "universal-wallpaper/utils.h"
#include <X11/Xlib.h>
#include <X11/Xatom.h>
#include <wayland-client.h>
#include <thread>
#include <chrono>

FullscreenDetector::FullscreenDetector() = default;

FullscreenDetector::~FullscreenDetector() {
    cleanup();
}

bool FullscreenDetector::initialize() {
    if (!enabled_) {
        return true;  // Success, but disabled
    }
    
    log_debug("Initializing fullscreen detector");
    
    // Detect if we're on Wayland or X11
    is_wayland_ = is_wayland_session();
    
    if (is_wayland_) {
        log_debug("Detected Wayland session for fullscreen detection");
        // For Wayland, we'll use a simpler heuristic approach
        // as direct fullscreen detection is limited
    } else {
        log_debug("Detected X11 session for fullscreen detection");
        display_ = XOpenDisplay(nullptr);
        if (!display_) {
            log_error("Failed to open X11 display for fullscreen detection");
            return false;
        }
    }
    
    // Start monitoring thread
    should_stop_ = false;
    monitor_thread_ = std::make_unique<std::thread>(&FullscreenDetector::monitor_fullscreen_apps, this);
    
    log_info("Fullscreen detector initialized successfully");
    return true;
}

void FullscreenDetector::cleanup() {
    should_stop_ = true;
    
    if (monitor_thread_ && monitor_thread_->joinable()) {
        monitor_thread_->join();
    }
    
    if (display_ && !is_wayland_) {
        XCloseDisplay(static_cast<Display*>(display_));
        display_ = nullptr;
    }
}

void FullscreenDetector::set_enabled(bool enabled) {
    enabled_ = enabled;
    if (!enabled) {
        fullscreen_app_active_ = false;
    }
}

bool FullscreenDetector::is_enabled() const {
    return enabled_;
}

bool FullscreenDetector::is_fullscreen_app_active() const {
    return enabled_ && fullscreen_app_active_;
}

void FullscreenDetector::monitor_fullscreen_apps() {
    log_debug("Starting fullscreen monitoring thread");
    
    while (!should_stop_) {
        if (!enabled_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }
        
        bool was_fullscreen = fullscreen_app_active_;
        bool is_fullscreen = check_fullscreen_windows();
        
        if (is_fullscreen != was_fullscreen) {
            fullscreen_app_active_ = is_fullscreen;
            if (is_fullscreen) {
                log_info("Fullscreen application detected - pausing wallpaper");
            } else {
                log_info("Fullscreen application closed - resuming wallpaper");
            }
        }
        
        // Check every 2 seconds - fullscreen detection doesn't need to be as frequent
        std::this_thread::sleep_for(std::chrono::milliseconds(2000));
    }
    
    log_debug("Fullscreen monitoring thread stopped");
}

bool FullscreenDetector::check_fullscreen_windows() {
    if (is_wayland_) {
        return check_wayland_fullscreen();
    } else {
        return check_x11_fullscreen();
    }
}

bool FullscreenDetector::check_x11_fullscreen() {
    if (!display_) return false;
    
    Display* dpy = static_cast<Display*>(display_);
    Window root = DefaultRootWindow(dpy);
    
    // Get the list of all windows
    Atom actual_type;
    int actual_format;
    unsigned long nitems, bytes_after;
    unsigned char* prop = nullptr;
    
    Atom net_client_list = XInternAtom(dpy, "_NET_CLIENT_LIST", False);
    
    if (XGetWindowProperty(dpy, root, net_client_list, 0, 1024, False,
                          XA_WINDOW, &actual_type, &actual_format,
                          &nitems, &bytes_after, &prop) != Success) {
        return false;
    }
    
    if (actual_type != XA_WINDOW) {
        if (prop) XFree(prop);
        return false;
    }
    
    Window* windows = reinterpret_cast<Window*>(prop);
    
    // Check each window for fullscreen state
    Atom net_wm_state = XInternAtom(dpy, "_NET_WM_STATE", False);
    Atom net_wm_state_fullscreen = XInternAtom(dpy, "_NET_WM_STATE_FULLSCREEN", False);
    
    for (unsigned long i = 0; i < nitems; i++) {
        Window window = windows[i];
        
        // Get window attributes to check if it's mapped and visible
        XWindowAttributes attrs;
        if (XGetWindowAttributes(dpy, window, &attrs) != Success) {
            continue;
        }
        
        if (attrs.map_state != IsViewable) {
            continue;
        }
        
        // Check if window is fullscreen
        unsigned char* state_prop = nullptr;
        unsigned long state_nitems;
        
        if (XGetWindowProperty(dpy, window, net_wm_state, 0, 1024, False,
                              XA_ATOM, &actual_type, &actual_format,
                              &state_nitems, &bytes_after, &state_prop) == Success) {
            
            if (actual_type == XA_ATOM) {
                Atom* states = reinterpret_cast<Atom*>(state_prop);
                for (unsigned long j = 0; j < state_nitems; j++) {
                    if (states[j] == net_wm_state_fullscreen) {
                        if (state_prop) XFree(state_prop);
                        if (prop) XFree(prop);
                        return true;
                    }
                }
            }
            
            if (state_prop) XFree(state_prop);
        }
    }
    
    if (prop) XFree(prop);
    return false;
}

bool FullscreenDetector::check_wayland_fullscreen() {
    // For Wayland, we use a heuristic approach since direct window state
    // detection is limited due to security restrictions
    
    // Check if any high-CPU processes are running that might be fullscreen apps
    // This is a simplified heuristic - in a full implementation, we'd use
    // Wayland protocols where available
    
    // For now, we'll be conservative and assume no fullscreen detection
    // on Wayland to avoid false positives
    return false;
}
