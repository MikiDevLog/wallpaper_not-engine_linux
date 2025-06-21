#include "universal-wallpaper/x11_backend.h"
#include "universal-wallpaper/renderer.h"
#include "universal-wallpaper/utils.h"
#include <X11/Xatom.h>
#include <X11/Xutil.h>
#include <GL/gl.h>
#include <cstring>
#include <algorithm>

const std::string X11Backend::backend_name_ = "X11";

X11Backend::X11Backend() = default;

X11Backend::~X11Backend() {
    destroy();
}

bool X11Backend::initialize() {
    log_info("Initializing X11 backend");
    
    display_ = XOpenDisplay(nullptr);
    if (!display_) {
        log_error("Failed to open X11 display");
        return false;
    }
    
    root_window_ = DefaultRootWindow(display_);
    
    if (!detect_monitors()) {
        log_error("Failed to detect monitors");
        return false;
    }
    
    log_info("X11 backend initialized successfully");
    return true;
}

void X11Backend::destroy() {
    if (display_) {
        XCloseDisplay(display_);
        display_ = nullptr;
    }
}

std::vector<Monitor> X11Backend::get_monitors() {
    return monitors_;
}

bool X11Backend::detect_monitors() {
    monitors_.clear();
    
    int screen = DefaultScreen(display_);
    
    // Check if RandR extension is available
    int randr_event_base, randr_error_base;
    if (!XRRQueryExtension(display_, &randr_event_base, &randr_error_base)) {
        log_warn("RandR extension not available, using screen dimensions");
        
        // Fallback to screen dimensions
        Monitor monitor;
        monitor.name = "Screen";
        monitor.x = 0;
        monitor.y = 0;
        monitor.width = DisplayWidth(display_, screen);
        monitor.height = DisplayHeight(display_, screen);
        monitor.refresh_rate = 60; // Default
        monitor.primary = true;
        
        monitors_.push_back(monitor);
        return true;
    }
    
    // Get RandR screen resources
    XRRScreenResources* screen_resources = XRRGetScreenResources(display_, root_window_);
    if (!screen_resources) {
        log_error("Failed to get screen resources");
        return false;
    }
    
    // Iterate through outputs
    for (int i = 0; i < screen_resources->noutput; i++) {
        XRROutputInfo* output_info = XRRGetOutputInfo(display_, screen_resources, screen_resources->outputs[i]);
        if (!output_info) continue;
        
        // Skip disconnected outputs
        if (output_info->connection != RR_Connected || output_info->crtc == None) {
            XRRFreeOutputInfo(output_info);
            continue;
        }
        
        // Get CRTC info
        XRRCrtcInfo* crtc_info = XRRGetCrtcInfo(display_, screen_resources, output_info->crtc);
        if (!crtc_info) {
            XRRFreeOutputInfo(output_info);
            continue;
        }
        
        Monitor monitor;
        monitor.name = output_info->name;
        monitor.x = crtc_info->x;
        monitor.y = crtc_info->y;
        monitor.width = crtc_info->width;
        monitor.height = crtc_info->height;
        monitor.refresh_rate = 60; // TODO: Calculate from mode info
        
        // Check if this is the primary monitor
        RROutput primary = XRRGetOutputPrimary(display_, root_window_);
        monitor.primary = (screen_resources->outputs[i] == primary);
        
        monitors_.push_back(monitor);
        
        XRRFreeCrtcInfo(crtc_info);
        XRRFreeOutputInfo(output_info);
    }
    
    XRRFreeScreenResources(screen_resources);
    
    if (monitors_.empty()) {
        log_warn("No connected monitors found, using screen dimensions");
        
        Monitor monitor;
        monitor.name = "Screen";
        monitor.x = 0;
        monitor.y = 0;
        monitor.width = DisplayWidth(display_, screen);
        monitor.height = DisplayHeight(display_, screen);
        monitor.refresh_rate = 60;
        monitor.primary = true;
        
        monitors_.push_back(monitor);
    }
    
    log_info("Detected " + std::to_string(monitors_.size()) + " monitor(s)");
    for (const auto& monitor : monitors_) {
        log_debug("Monitor: " + monitor.name + " (" + 
                 std::to_string(monitor.width) + "x" + std::to_string(monitor.height) + 
                 " at " + std::to_string(monitor.x) + "," + std::to_string(monitor.y) + ")");
    }
    
    return true;
}

bool X11Backend::set_wallpaper(const std::string& monitor_name, GLuint texture, int width, int height) {
    if (monitor_name == "ALL") {
        return set_wallpaper_all(texture, width, height);
    }
    
    // Find the specific monitor
    auto it = std::find_if(monitors_.begin(), monitors_.end(),
                          [&monitor_name](const Monitor& m) { return m.name == monitor_name; });
    
    if (it == monitors_.end()) {
        log_error("Monitor not found: " + monitor_name);
        return false;
    }
    
    // For X11, we set the wallpaper for the entire root window
    // TODO: Implement per-monitor wallpaper support
    log_warn("Per-monitor wallpaper not yet implemented for X11, setting for all monitors");
    return set_wallpaper_all(texture, width, height);
}

bool X11Backend::set_wallpaper_all(GLuint texture, int width, int height) {
    if (!display_) return false;
    
    // Convert OpenGL texture to X11 pixmap
    Pixmap pixmap = texture_to_pixmap(texture, width, height);
    if (pixmap == None) {
        log_error("Failed to convert texture to pixmap");
        return false;
    }
    
    bool success = set_root_pixmap(pixmap, width, height);
    
    // Clean up
    XFreePixmap(display_, pixmap);
    
    return success;
}

bool X11Backend::set_root_pixmap(Pixmap pixmap, int width, int height) {
    // Set the root window background
    XSetWindowBackgroundPixmap(display_, root_window_, pixmap);
    XClearWindow(display_, root_window_);
    
    // Set _XROOTPMAP_ID property for compatibility with desktop environments
    Atom xrootpmap_id = XInternAtom(display_, "_XROOTPMAP_ID", False);
    Atom esetroot_pmap_id = XInternAtom(display_, "ESETROOT_PMAP_ID", False);
    
    // Create a copy of the pixmap for the property
    Pixmap property_pixmap = XCreatePixmap(display_, root_window_, width, height, 
                                          DefaultDepth(display_, DefaultScreen(display_)));
    
    GC gc = XCreateGC(display_, property_pixmap, 0, nullptr);
    XCopyArea(display_, pixmap, property_pixmap, gc, 0, 0, width, height, 0, 0);
    XFreeGC(display_, gc);
    
    // Set the properties
    XChangeProperty(display_, root_window_, xrootpmap_id, XA_PIXMAP, 32,
                   PropModeReplace, (unsigned char*)&property_pixmap, 1);
    XChangeProperty(display_, root_window_, esetroot_pmap_id, XA_PIXMAP, 32,
                   PropModeReplace, (unsigned char*)&property_pixmap, 1);
    
    XFlush(display_);
    
    log_debug("Set X11 wallpaper (" + std::to_string(width) + "x" + std::to_string(height) + ")");
    return true;
}

Pixmap X11Backend::texture_to_pixmap(GLuint texture, int width, int height) {
    if (!display_) return None;
    
    log_debug("Converting texture " + std::to_string(texture) + " (" + std::to_string(width) + "x" + std::to_string(height) + ") to pixmap");
    
    // Ensure OpenGL context is current
    if (renderer_ && !renderer_->make_current()) {
        log_error("Failed to make OpenGL context current");
        return None;
    }
    
    // Read texture data from OpenGL
    std::vector<unsigned char> pixels(width * height * 4);
    
    // Bind the texture and read its data
    glBindTexture(GL_TEXTURE_2D, texture);
    glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    
    // Check for OpenGL errors
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        log_error("OpenGL error reading texture: " + std::to_string(error));
        log_warn("Falling back to test pattern");
        // Fall back to test pattern for debugging
        for (int y = 0; y < height; y++) {
            for (int x = 0; x < width; x++) {
                int idx = (y * width + x) * 4;
                pixels[idx] = (x * 255) / width;     // Red
                pixels[idx + 1] = (y * 255) / height; // Green
                pixels[idx + 2] = 128;               // Blue
                pixels[idx + 3] = 255;               // Alpha
            }
        }
    } else {
        log_debug("Successfully read texture data");
        
        // Debug: check if texture has any non-zero data
        bool has_data = false;
        for (int i = 0; i < width * height * 4; i += 4) {
            if (pixels[i] > 0 || pixels[i+1] > 0 || pixels[i+2] > 0) {
                has_data = true;
                break;
            }
        }
        
        if (!has_data) {
            log_debug("Texture appears to be all black - no video content yet");
        } else {
            log_debug("Texture contains video data");
        }
    }
    
    // Convert RGBA to BGRA for X11 (if needed)
    for (int i = 0; i < width * height; i++) {
        int idx = i * 4;
        std::swap(pixels[idx], pixels[idx + 2]); // Swap R and B
    }
    
    int screen = DefaultScreen(display_);
    int depth = DefaultDepth(display_, screen);
    Visual* visual = DefaultVisual(display_, screen);
    
    // Create XImage
    XImage* image = XCreateImage(display_, visual, depth, ZPixmap, 0,
                                (char*)pixels.data(), width, height, 32, 0);
    if (!image) {
        log_error("Failed to create XImage");
        return None;
    }
    
    // Create pixmap
    Pixmap pixmap = XCreatePixmap(display_, root_window_, width, height, depth);
    if (pixmap == None) {
        log_error("Failed to create pixmap");
        XDestroyImage(image);
        return None;
    }
    
    // Copy image to pixmap
    GC gc = XCreateGC(display_, pixmap, 0, nullptr);
    XPutImage(display_, pixmap, gc, image, 0, 0, 0, 0, width, height);
    XFreeGC(display_, gc);
    
    // Clean up (don't destroy image data, it points to our vector)
    image->data = nullptr;
    XDestroyImage(image);
    
    return pixmap;
}

void* X11Backend::get_native_display() {
    return display_;
}

const std::string& X11Backend::get_backend_name() const {
    return backend_name_;
}

void X11Backend::process_events() {
    if (!display_) return;
    
    while (XPending(display_)) {
        XEvent event;
        XNextEvent(display_, &event);
        
        // Handle events if needed
        switch (event.type) {
            case ClientMessage:
                // Handle window manager messages
                break;
            default:
                break;
        }
    }
}

bool X11Backend::should_quit() const {
    return should_quit_;
}

void X11Backend::set_renderer(Renderer* renderer) {
    renderer_ = renderer;
}
