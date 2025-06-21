#pragma once

#include "display_manager.h"
#include <wayland-client.h>
#include <wayland-egl.h>
#include <EGL/egl.h>
#include <memory>

// Forward declarations for protocol interfaces
struct zwlr_layer_shell_v1;
struct zwlr_layer_surface_v1;
struct zxdg_output_manager_v1;
struct zxdg_output_v1;
class Renderer;

struct WaylandOutput {
    wl_output* output = nullptr;
    zxdg_output_v1* xdg_output = nullptr;
    std::string name;
    std::string description;
    int x = 0, y = 0;
    int width = 0, height = 0;
    int scale = 1;
    bool done = false;
};

struct WaylandSurface {
    wl_surface* surface = nullptr;
    zwlr_layer_surface_v1* layer_surface = nullptr;
    wl_egl_window* egl_window = nullptr;
    EGLSurface egl_surface = EGL_NO_SURFACE;
    wl_callback* frame_callback = nullptr;
    int width = 0, height = 0;
    int scale = 1;
    bool configured = false;
    bool rendering = false;
};

class WaylandBackend : public DisplayBackend {
public:
    WaylandBackend();
    ~WaylandBackend() override;
    
    bool initialize() override;
    void destroy() override;
    
    std::vector<Monitor> get_monitors() override;
    bool set_wallpaper(const std::string& monitor_name, GLuint texture, int width, int height) override;
    bool set_wallpaper_all(GLuint texture, int width, int height) override;
    
    void* get_native_display() override;
    const std::string& get_backend_name() const override;
    
    void process_events() override;
    bool should_quit() const override;
    
    // Set the renderer instance for wallpaper rendering
    void set_renderer(Renderer* renderer) { renderer_ = renderer; }

private:
    wl_display* display_ = nullptr;
    wl_registry* registry_ = nullptr;
    wl_compositor* compositor_ = nullptr;
    zwlr_layer_shell_v1* layer_shell_ = nullptr;
    zxdg_output_manager_v1* xdg_output_manager_ = nullptr;
    
    std::vector<std::unique_ptr<WaylandOutput>> outputs_;
    std::vector<std::unique_ptr<WaylandSurface>> surfaces_;
    
    Renderer* renderer_ = nullptr;
    bool should_quit_ = false;
    
    static const std::string backend_name_;
    
    // Wayland callbacks - moved to public for static callback access
public:
    static void registry_global(void* data, wl_registry* registry, uint32_t name,
                               const char* interface, uint32_t version);
    static void registry_global_remove(void* data, wl_registry* registry, uint32_t name);
    
    static void output_geometry(void* data, wl_output* output, int32_t x, int32_t y,
                               int32_t physical_width, int32_t physical_height,
                               int32_t subpixel, const char* make, const char* model,
                               int32_t transform);
    static void output_mode(void* data, wl_output* output, uint32_t flags,
                           int32_t width, int32_t height, int32_t refresh);
    static void output_done(void* data, wl_output* output);
    static void output_scale(void* data, wl_output* output, int32_t factor);
    
    static void xdg_output_logical_position(void* data, zxdg_output_v1* xdg_output,
                                           int32_t x, int32_t y);
    static void xdg_output_logical_size(void* data, zxdg_output_v1* xdg_output,
                                       int32_t width, int32_t height);
    static void xdg_output_done(void* data, zxdg_output_v1* xdg_output);
    static void xdg_output_name(void* data, zxdg_output_v1* xdg_output, const char* name);
    static void xdg_output_description(void* data, zxdg_output_v1* xdg_output, const char* description);
    
    static void layer_surface_configure(void* data, zwlr_layer_surface_v1* layer_surface,
                                       uint32_t serial, uint32_t width, uint32_t height);
    static void layer_surface_closed(void* data, zwlr_layer_surface_v1* layer_surface);
    
    static void frame_callback_done(void* data, wl_callback* callback, uint32_t time);

private:
    
    WaylandSurface* create_surface_for_output(WaylandOutput* output);
    bool render_to_output(WaylandOutput* output, GLuint texture, int tex_width, int tex_height);
    void render_to_surface(WaylandSurface* surface, GLuint texture, int tex_width, int tex_height);
    WaylandSurface* find_surface_for_output(WaylandOutput* output);
    
    WaylandOutput* find_output_by_name(const std::string& name);
};
