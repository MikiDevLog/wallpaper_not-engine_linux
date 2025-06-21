#include "universal-wallpaper/wayland_backend.h"
#include "universal-wallpaper/renderer.h"
#include "universal-wallpaper/utils.h"
#include "universal-wallpaper/wayland_protocols.h"
#include <EGL/egl.h>
#include <algorithm>
#include <cstring>

const std::string WaylandBackend::backend_name_ = "Wayland";

// Protocol listener structures
static const wl_registry_listener registry_listener = {
    WaylandBackend::registry_global,
    WaylandBackend::registry_global_remove
};

static const wl_output_listener output_listener = {
    WaylandBackend::output_geometry,
    WaylandBackend::output_mode,
    WaylandBackend::output_done,
    WaylandBackend::output_scale
};

static const zxdg_output_v1_listener xdg_output_listener = {
    WaylandBackend::xdg_output_logical_position,
    WaylandBackend::xdg_output_logical_size,
    WaylandBackend::xdg_output_done,
    WaylandBackend::xdg_output_name,
    WaylandBackend::xdg_output_description
};

static const zwlr_layer_surface_v1_listener layer_surface_listener = {
    WaylandBackend::layer_surface_configure,
    WaylandBackend::layer_surface_closed
};

static const wl_callback_listener frame_callback_listener = {
    WaylandBackend::frame_callback_done
};

WaylandBackend::WaylandBackend() = default;

WaylandBackend::~WaylandBackend() {
    destroy();
}

bool WaylandBackend::initialize() {
    log_info("Initializing Wayland backend");
    
    display_ = wl_display_connect(nullptr);
    if (!display_) {
        log_error("Failed to connect to Wayland display");
        return false;
    }
    
    registry_ = wl_display_get_registry(display_);
    if (!registry_) {
        log_error("Failed to get Wayland registry");
        return false;
    }
    
    wl_registry_add_listener(registry_, &registry_listener, this);
    
    // Roundtrip to get all globals
    wl_display_roundtrip(display_);
    
    if (!compositor_) {
        log_error("Wayland compositor not available");
        return false;
    }
    
    if (!layer_shell_) {
        log_error("wlr-layer-shell protocol not available");
        return false;
    }
    
    // Another roundtrip to get output information
    wl_display_roundtrip(display_);
    
    // Create XDG outputs if manager is available
    if (xdg_output_manager_) {
        for (const auto& output : outputs_) {
            output->xdg_output = zxdg_output_manager_v1_get_xdg_output(
                xdg_output_manager_, output->output);
            if (output->xdg_output) {
                zxdg_output_v1_add_listener(output->xdg_output, &xdg_output_listener, output.get());
                log_debug("Created XDG output for wl_output");
            }
        }
        
        // Another roundtrip to get XDG output information
        wl_display_roundtrip(display_);
    } else {
        log_warn("XDG output manager not available - output names might not be available");
    }
    
    if (outputs_.empty()) {
        log_error("No Wayland outputs found");
        return false;
    }
    
    log_info("Wayland backend initialized successfully");
    log_info("Found " + std::to_string(outputs_.size()) + " output(s)");
    
    // Log available outputs for debugging
    for (const auto& output : outputs_) {
        std::string name = output->name.empty() ? "Unknown" : output->name;
        log_info("Available output: " + name + " (" + std::to_string(output->width) + "x" + std::to_string(output->height) + ")");
    }
    
    return true;
}

void WaylandBackend::destroy() {
    // Clean up surfaces
    for (auto& surface : surfaces_) {
        if (surface->egl_surface != EGL_NO_SURFACE) {
            // EGL surface cleanup would be handled by renderer
        }
        if (surface->egl_window) {
            wl_egl_window_destroy(surface->egl_window);
        }
        if (surface->layer_surface) {
            zwlr_layer_surface_v1_destroy(surface->layer_surface);
        }
        if (surface->surface) {
            wl_surface_destroy(surface->surface);
        }
    }
    surfaces_.clear();
    
    // Clean up outputs
    for (auto& output : outputs_) {
        if (output->xdg_output) {
            zxdg_output_v1_destroy(output->xdg_output);
        }
        if (output->output) {
            wl_output_destroy(output->output);
        }
    }
    outputs_.clear();
    
    if (xdg_output_manager_) {
        zxdg_output_manager_v1_destroy(xdg_output_manager_);
        xdg_output_manager_ = nullptr;
    }
    
    if (layer_shell_) {
        zwlr_layer_shell_v1_destroy(layer_shell_);
        layer_shell_ = nullptr;
    }
    
    if (compositor_) {
        wl_compositor_destroy(compositor_);
        compositor_ = nullptr;
    }
    
    if (registry_) {
        wl_registry_destroy(registry_);
        registry_ = nullptr;
    }
    
    if (display_) {
        wl_display_disconnect(display_);
        display_ = nullptr;
    }
}

std::vector<Monitor> WaylandBackend::get_monitors() {
    std::vector<Monitor> monitors;
    
    for (const auto& output : outputs_) {
        if (!output->done) continue;
        
        Monitor monitor;
        monitor.name = generate_output_name(output.get());
        monitor.x = output->x;
        monitor.y = output->y;
        monitor.width = output->width;
        monitor.height = output->height;
        monitor.refresh_rate = 60; // TODO: Get actual refresh rate
        monitor.primary = monitors.empty(); // First monitor is primary
        
        monitors.push_back(monitor);
    }
    
    return monitors;
}

bool WaylandBackend::set_wallpaper(const std::string& monitor_name, GLuint texture, int width, int height) {
    if (monitor_name == "ALL") {
        return set_wallpaper_all(texture, width, height);
    }
    
    WaylandOutput* output = find_output_by_name(monitor_name);
    if (!output) {
        log_error("Output not found: " + monitor_name);
        log_error("Available outputs:");
        for (const auto& out : outputs_) {
            std::string name = generate_output_name(out.get());
            log_error("  - " + name);
        }
        return false;
    }
    
    return render_to_output(output, texture, width, height);
}

bool WaylandBackend::set_wallpaper_all(GLuint texture, int width, int height) {
    bool success = true;
    
    for (const auto& output : outputs_) {
        if (!output->done) continue;
        
        if (!render_to_output(output.get(), texture, width, height)) {
            success = false;
        }
    }
    
    return success;
}

WaylandSurface* WaylandBackend::create_surface_for_output(WaylandOutput* output) {
    if (!output || !compositor_ || !layer_shell_) return nullptr;
    
    log_debug("Creating surface for output: " + output->name);
    
    auto surface = std::make_unique<WaylandSurface>();
    
    // Create Wayland surface
    surface->surface = wl_compositor_create_surface(compositor_);
    if (!surface->surface) {
        log_error("Failed to create Wayland surface");
        return nullptr;
    }
    log_debug("Created Wayland surface");
    
    // Create layer surface
    surface->layer_surface = zwlr_layer_shell_v1_get_layer_surface(
        layer_shell_, surface->surface, output->output,
        ZWLR_LAYER_SHELL_V1_LAYER_BACKGROUND, "wallpaper");
    
    if (!surface->layer_surface) {
        log_error("Failed to create layer surface");
        wl_surface_destroy(surface->surface);
        return nullptr;
    }
    log_debug("Created layer surface");
    
    zwlr_layer_surface_v1_add_listener(surface->layer_surface, &layer_surface_listener, surface.get());
    
    // Configure the layer surface
    zwlr_layer_surface_v1_set_size(surface->layer_surface, 0, 0); // Use output size
    zwlr_layer_surface_v1_set_anchor(surface->layer_surface,
        ZWLR_LAYER_SURFACE_V1_ANCHOR_TOP |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_RIGHT |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_BOTTOM |
        ZWLR_LAYER_SURFACE_V1_ANCHOR_LEFT);
    zwlr_layer_surface_v1_set_exclusive_zone(surface->layer_surface, -1);
    zwlr_layer_surface_v1_set_keyboard_interactivity(surface->layer_surface, 
        ZWLR_LAYER_SURFACE_V1_KEYBOARD_INTERACTIVITY_NONE);

    // Create input region covering the entire surface (critical for proper behavior)
    struct wl_region* input_region = wl_compositor_create_region(compositor_);
    wl_region_add(input_region, 0, 0, INT32_MAX, INT32_MAX);
    wl_surface_set_input_region(surface->surface, input_region);
    wl_region_destroy(input_region);
    
    log_debug("Configured layer surface, committing");
    
    // Commit the surface to trigger configure
    wl_surface_commit(surface->surface);
    
    // Force a roundtrip to ensure configure events are processed
    wl_display_roundtrip(display_);
    
    WaylandSurface* surface_ptr = surface.get();
    surfaces_.push_back(std::move(surface));
    
    log_debug("Surface added to surfaces list, configured: " + std::to_string(surface_ptr->configured));
    
    return surface_ptr;
}

void WaylandBackend::render_to_surface(WaylandSurface* surface, GLuint texture, int tex_width, int tex_height) {
    if (!surface || !surface->configured || !surface->egl_window) {
        log_debug("Surface not ready for rendering - configured: " + std::to_string(surface ? surface->configured : false) + 
                 ", egl_window: " + std::to_string(surface && surface->egl_window != nullptr));
        return;
    }
    
    if (!renderer_) {
        log_error("No renderer available for wallpaper rendering");
        return;
    }
    
    // Don't render if we're already rendering or have a pending frame callback
    if (surface->rendering || surface->frame_callback) {
        log_debug("Skipping render - already rendering or frame callback pending");
        return;
    }
    
    log_debug("Rendering texture " + std::to_string(texture) + " to surface (" + 
              std::to_string(surface->width) + "x" + std::to_string(surface->height) + ")");
    
    // Create EGL surface if it doesn't exist
    if (surface->egl_surface == EGL_NO_SURFACE) {
        log_debug("Creating EGL surface for wallpaper");
        surface->egl_surface = renderer_->create_egl_surface_for_wayland(surface->egl_window);
        if (surface->egl_surface == EGL_NO_SURFACE) {
            log_error("Failed to create EGL surface for wallpaper");
            return;
        }
        log_debug("Created EGL surface for wallpaper rendering");
    }
    
    surface->rendering = true;
    
    // Render the texture to the surface
    log_debug("Calling renderer->render_texture_to_surface");
    bool result = renderer_->render_texture_to_surface(surface->egl_surface, texture, surface->width, surface->height);
    log_debug("Render result: " + std::to_string(result));
    
    if (result) {
        // Set up frame callback for proper rendering synchronization (like linux-wallpaperengine)
        surface->frame_callback = wl_surface_frame(surface->surface);
        wl_callback_add_listener(surface->frame_callback, &frame_callback_listener, surface);
        
        // Set buffer scale if needed
        if (surface->scale > 1) {
            wl_surface_set_buffer_scale(surface->surface, surface->scale);
        }
        
        // Damage the entire surface
        wl_surface_damage(surface->surface, 0, 0, surface->width, surface->height);
        
        // Commit the surface to make changes visible
        log_debug("Committing surface with frame callback");
        wl_surface_commit(surface->surface);
        wl_display_flush(display_);
    }
    
    surface->rendering = false;
}

bool WaylandBackend::render_to_output(WaylandOutput* output, GLuint texture, int tex_width, int tex_height) {
    if (!output || !output->done) {
        log_debug("Output not ready for rendering");
        return false;
    }
    
    log_debug("Rendering to output: " + output->name + " (" + std::to_string(output->width) + "x" + std::to_string(output->height) + ")");
    
    // Find existing surface for this output or create one
    WaylandSurface* surface = find_surface_for_output(output);
    if (!surface) {
        log_debug("Creating new surface for output");
        surface = create_surface_for_output(output);
        if (!surface) {
            log_error("Failed to create surface for output");
            return false;
        }
    } else {
        log_debug("Using existing surface for output");
    }
    
    // Render the texture to the surface
    render_to_surface(surface, texture, tex_width, tex_height);
    return true;
}

WaylandSurface* WaylandBackend::find_surface_for_output(WaylandOutput* output) {
    // For now, we'll use a simple approach - one surface per output
    // This could be enhanced to track which surface belongs to which output
    for (auto& surface : surfaces_) {
        if (surface->configured) {
            return surface.get();
        }
    }
    return nullptr;
}

WaylandOutput* WaylandBackend::find_output_by_name(const std::string& name) {
    for (const auto& output : outputs_) {
        // Check both original XDG name and generated name
        if (output->name == name || generate_output_name(output.get()) == name) {
            return output.get();
        }
    }
    return nullptr;
}

void* WaylandBackend::get_native_display() {
    return display_;
}

const std::string& WaylandBackend::get_backend_name() const {
    return backend_name_;
}

void WaylandBackend::process_events() {
    if (!display_) return;
    
    while (wl_display_prepare_read(display_) != 0) {
        wl_display_dispatch_pending(display_);
    }
    
    wl_display_flush(display_);
    wl_display_read_events(display_);
    wl_display_dispatch_pending(display_);
}

bool WaylandBackend::should_quit() const {
    return should_quit_;
}

// Helper function to generate meaningful output names
std::string WaylandBackend::generate_output_name(const WaylandOutput* output) {
    if (!output->name.empty() && output->name != "Unknown") {
        return output->name;
    }
    
    // Generate a name based on resolution and position
    // Use a format similar to what users might expect from X11
    std::string name;
    
    // For single monitor systems, use a simple identifier
    if (outputs_.size() == 1) {
        name = "HDMI-A-1";  // Default to common connector type for single display
    } else {
        // For multi-monitor, use resolution-based naming
        name = "OUTPUT-" + std::to_string(output->width) + "x" + std::to_string(output->height);
        
        if (output->x != 0 || output->y != 0) {
            name += "-" + std::to_string(output->x) + "+" + std::to_string(output->y);
        }
        
        // If there are multiple outputs with same resolution and position, 
        // add an index to differentiate them
        int index = 0;
        for (const auto& other : outputs_) {
            if (other.get() == output) break;
            if (other->width == output->width && other->height == output->height &&
                other->x == output->x && other->y == output->y) {
                index++;
            }
        }
        
        if (index > 0) {
            name += "-" + std::to_string(index);
        }
    }
    
    return name;
}

// Static callback implementations
void WaylandBackend::registry_global(void* data, wl_registry* registry, uint32_t name,
                                     const char* interface, uint32_t version) {
    auto* backend = static_cast<WaylandBackend*>(data);
    
    if (strcmp(interface, wl_compositor_interface.name) == 0) {
        backend->compositor_ = static_cast<wl_compositor*>(
            wl_registry_bind(registry, name, &wl_compositor_interface, 4));
        log_debug("Bound wl_compositor");
    }
    else if (strcmp(interface, zwlr_layer_shell_v1_interface.name) == 0) {
        backend->layer_shell_ = static_cast<zwlr_layer_shell_v1*>(
            wl_registry_bind(registry, name, &zwlr_layer_shell_v1_interface, 1));
        log_debug("Bound zwlr_layer_shell_v1");
    }
    else if (strcmp(interface, wl_output_interface.name) == 0) {
        auto output = std::make_unique<WaylandOutput>();
        output->output = static_cast<wl_output*>(
            wl_registry_bind(registry, name, &wl_output_interface, 2));
        wl_output_add_listener(output->output, &output_listener, output.get());
        backend->outputs_.push_back(std::move(output));
        log_debug("Bound wl_output");
    }
    else if (strcmp(interface, zxdg_output_manager_v1_interface.name) == 0) {
        backend->xdg_output_manager_ = static_cast<zxdg_output_manager_v1*>(
            wl_registry_bind(registry, name, &zxdg_output_manager_v1_interface, 1));
        log_debug("Bound zxdg_output_manager_v1");
    }
}

void WaylandBackend::registry_global_remove(void* data, wl_registry* registry, uint32_t name) {
    // Handle global removal if needed
}

void WaylandBackend::output_geometry(void* data, wl_output* output, int32_t x, int32_t y,
                                    int32_t physical_width, int32_t physical_height,
                                    int32_t subpixel, const char* make, const char* model,
                                    int32_t transform) {
    auto* wayland_output = static_cast<WaylandOutput*>(data);
    wayland_output->x = x;
    wayland_output->y = y;
}

void WaylandBackend::output_mode(void* data, wl_output* output, uint32_t flags,
                                int32_t width, int32_t height, int32_t refresh) {
    auto* wayland_output = static_cast<WaylandOutput*>(data);
    if (flags & WL_OUTPUT_MODE_CURRENT) {
        wayland_output->width = width;
        wayland_output->height = height;
    }
}

void WaylandBackend::output_done(void* data, wl_output* output) {
    auto* wayland_output = static_cast<WaylandOutput*>(data);
    wayland_output->done = true;
}

void WaylandBackend::output_scale(void* data, wl_output* output, int32_t factor) {
    auto* wayland_output = static_cast<WaylandOutput*>(data);
    wayland_output->scale = factor;
}

void WaylandBackend::xdg_output_logical_position(void* data, zxdg_output_v1* xdg_output,
                                                int32_t x, int32_t y) {
    auto* wayland_output = static_cast<WaylandOutput*>(data);
    wayland_output->x = x;
    wayland_output->y = y;
}

void WaylandBackend::xdg_output_logical_size(void* data, zxdg_output_v1* xdg_output,
                                            int32_t width, int32_t height) {
    auto* wayland_output = static_cast<WaylandOutput*>(data);
    wayland_output->width = width;
    wayland_output->height = height;
}

void WaylandBackend::xdg_output_done(void* data, zxdg_output_v1* xdg_output) {
    // XDG output information is complete
}

void WaylandBackend::xdg_output_name(void* data, zxdg_output_v1* xdg_output, const char* name) {
    auto* wayland_output = static_cast<WaylandOutput*>(data);
    wayland_output->name = name;
}

void WaylandBackend::xdg_output_description(void* data, zxdg_output_v1* xdg_output, const char* description) {
    auto* wayland_output = static_cast<WaylandOutput*>(data);
    wayland_output->description = description;
}

void WaylandBackend::layer_surface_configure(void* data, zwlr_layer_surface_v1* layer_surface,
                                            uint32_t serial, uint32_t width, uint32_t height) {
    auto* surface = static_cast<WaylandSurface*>(data);
    
    log_debug("Layer surface configure: " + std::to_string(width) + "x" + std::to_string(height) + 
              ", serial: " + std::to_string(serial));
    
    surface->width = width;
    surface->height = height;
    surface->configured = true;
    
    zwlr_layer_surface_v1_ack_configure(layer_surface, serial);
    
    // Create EGL window if not exists
    if (!surface->egl_window) {
        log_debug("Creating EGL window for layer surface");
        surface->egl_window = wl_egl_window_create(surface->surface, width * surface->scale, height * surface->scale);
        log_debug("EGL window created");
    } else {
        log_debug("Resizing EGL window");
        wl_egl_window_resize(surface->egl_window, width * surface->scale, height * surface->scale, 0, 0);
    }
    
    log_debug("Layer surface configured: " + std::to_string(width) + "x" + std::to_string(height));
}

void WaylandBackend::layer_surface_closed(void* data, zwlr_layer_surface_v1* layer_surface) {
    log_debug("Layer surface closed");
}

void WaylandBackend::frame_callback_done(void* data, wl_callback* callback, uint32_t time) {
    WaylandSurface* surface = static_cast<WaylandSurface*>(data);
    
    // Clean up the callback
    wl_callback_destroy(callback);
    surface->frame_callback = nullptr;
    
    // Surface is now ready for next frame
    log_debug("Frame callback done - surface ready for next frame");
}
