#include "universal-wallpaper/display_manager.h"
#include "universal-wallpaper/x11_backend.h"
#include "universal-wallpaper/wayland_backend.h"
#include "universal-wallpaper/utils.h"

DisplayManager::DisplayManager() = default;

DisplayManager::~DisplayManager() {
    destroy();
}

bool DisplayManager::initialize(bool force_x11, bool force_wayland) {
    if (force_x11 && force_wayland) {
        log_error("Cannot force both X11 and Wayland backends");
        return false;
    }
    
    // Determine which backend to use
    bool use_wayland = false;
    bool use_x11 = false;
    
    if (force_wayland) {
        use_wayland = true;
    } else if (force_x11) {
        use_x11 = true;
    } else {
        // Auto-detect based on environment
        if (is_wayland_session()) {
            use_wayland = true;
            log_info("Detected Wayland session");
        } else if (is_x11_session()) {
            use_x11 = true;
            log_info("Detected X11 session");
        } else {
            log_error("No display server detected");
            return false;
        }
    }
    
    // Create appropriate backend
    if (use_wayland) {
        backend_ = create_wayland_backend();
        if (!backend_) {
            log_warn("Failed to create Wayland backend, falling back to X11");
            backend_ = create_x11_backend();
        }
    } else if (use_x11) {
        backend_ = create_x11_backend();
        if (!backend_) {
            log_warn("Failed to create X11 backend, falling back to Wayland");
            backend_ = create_wayland_backend();
        }
    }
    
    if (!backend_) {
        log_error("Failed to create any display backend");
        return false;
    }
    
    if (!backend_->initialize()) {
        log_error("Failed to initialize display backend");
        backend_.reset();
        return false;
    }
    
    log_info("Display manager initialized with " + backend_->get_backend_name() + " backend");
    return true;
}

void DisplayManager::destroy() {
    if (backend_) {
        backend_->destroy();
        backend_.reset();
    }
}

void DisplayManager::set_renderer(Renderer* renderer) {
    if (backend_) {
        backend_->set_renderer(renderer);
    }
}

std::vector<Monitor> DisplayManager::get_monitors() {
    if (!backend_) return {};
    return backend_->get_monitors();
}

bool DisplayManager::set_wallpaper(const std::string& monitor_name, GLuint texture, int width, int height) {
    if (!backend_) return false;
    return backend_->set_wallpaper(monitor_name, texture, width, height);
}

bool DisplayManager::set_wallpaper_all(GLuint texture, int width, int height) {
    if (!backend_) return false;
    return backend_->set_wallpaper_all(texture, width, height);
}

void* DisplayManager::get_native_display() {
    if (!backend_) return nullptr;
    return backend_->get_native_display();
}

const std::string& DisplayManager::get_backend_name() const {
    static const std::string unknown = "unknown";
    if (!backend_) return unknown;
    return backend_->get_backend_name();
}

void DisplayManager::process_events() {
    if (backend_) {
        backend_->process_events();
    }
}

bool DisplayManager::should_quit() const {
    if (!backend_) return true;
    return backend_->should_quit();
}

std::unique_ptr<DisplayBackend> DisplayManager::create_wayland_backend() {
    try {
        return std::make_unique<WaylandBackend>();
    } catch (const std::exception& e) {
        log_error("Failed to create Wayland backend: " + std::string(e.what()));
        return nullptr;
    }
}

std::unique_ptr<DisplayBackend> DisplayManager::create_x11_backend() {
    try {
        return std::make_unique<X11Backend>();
    } catch (const std::exception& e) {
        log_error("Failed to create X11 backend: " + std::string(e.what()));
        return nullptr;
    }
}
