#pragma once

#include <string>
#include <vector>
#include <memory>
#include <GL/gl.h>

// Forward declarations
class Renderer;

struct Monitor {
    std::string name;
    int x, y;
    int width, height;
    int refresh_rate;
    bool primary;
};

class DisplayBackend {
public:
    virtual ~DisplayBackend() = default;
    
    virtual bool initialize() = 0;
    virtual void destroy() = 0;
    
    virtual std::vector<Monitor> get_monitors() = 0;
    virtual bool set_wallpaper(const std::string& monitor_name, GLuint texture, int width, int height) = 0;
    virtual bool set_wallpaper_all(GLuint texture, int width, int height) = 0;
    
    virtual void* get_native_display() = 0;
    virtual const std::string& get_backend_name() const = 0;
    
    virtual void process_events() = 0;
    virtual bool should_quit() const = 0;
    
    virtual void set_renderer(Renderer* renderer) = 0;
};

class DisplayManager {
public:
    DisplayManager();
    ~DisplayManager();
    
    bool initialize(bool force_x11 = false, bool force_wayland = false);
    void destroy();
    
    void set_renderer(Renderer* renderer);
    
    std::vector<Monitor> get_monitors();
    bool set_wallpaper(const std::string& monitor_name, GLuint texture, int width, int height);
    bool set_wallpaper_all(GLuint texture, int width, int height);
    
    void* get_native_display();
    const std::string& get_backend_name() const;
    
    void process_events();
    bool should_quit() const;
    
private:
    std::unique_ptr<DisplayBackend> backend_;
    
    std::unique_ptr<DisplayBackend> create_wayland_backend();
    std::unique_ptr<DisplayBackend> create_x11_backend();
};
