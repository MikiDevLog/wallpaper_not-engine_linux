#pragma once

#include "display_manager.h"
#include <X11/Xlib.h>
#include <X11/extensions/Xrandr.h>
#include <GL/gl.h>

// Forward declaration
class Renderer;

class X11Backend : public DisplayBackend {
public:
    X11Backend();
    ~X11Backend() override;
    
    bool initialize() override;
    void destroy() override;
    
    std::vector<Monitor> get_monitors() override;
    bool set_wallpaper(const std::string& monitor_name, GLuint texture, int width, int height) override;
    bool set_wallpaper_all(GLuint texture, int width, int height) override;
    
    void* get_native_display() override;
    const std::string& get_backend_name() const override;
    
    void process_events() override;
    bool should_quit() const override;
    
    void set_renderer(Renderer* renderer) override;

private:
    Display* display_ = nullptr;
    Window root_window_;
    std::vector<Monitor> monitors_;
    bool should_quit_ = false;
    Renderer* renderer_ = nullptr;
    
    static const std::string backend_name_;
    
    bool detect_monitors();
    bool set_root_pixmap(Pixmap pixmap, int width, int height);
    Pixmap texture_to_pixmap(GLuint texture, int width, int height);
};
