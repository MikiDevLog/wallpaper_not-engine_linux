#pragma once

#include <memory>
#include <functional>
#include <string>
#include <mpv/client.h>
#include <mpv/render_gl.h>

class MPVWrapper {
public:
    MPVWrapper();
    ~MPVWrapper();
    
    bool initialize(const std::string& media_path, bool hardware_decode = true, 
                   bool loop = true, bool mute_audio = true, double volume = 0.0,
                   const std::string& additional_options = std::string());
    
    void destroy();
    
    // Rendering
    bool create_render_context(void* (*get_proc_address)(void* ctx, const char* name), 
                              void* get_proc_address_ctx);
    void set_render_params(int width, int height, int fbo = 0);
    bool render_frame(int fbo, int width, int height);
    void report_flip();
    
    // Control
    void set_property(const std::string& name, const std::string& value);
    void set_property_async(const std::string& name, const std::string& value);
    std::string get_property(const std::string& name) const;
    void command(const std::string& cmd);
    
    // Event handling
    void set_wakeup_callback(std::function<void()> callback);
    void process_events();
    
    // State
    bool is_playing() const;
    bool has_video() const;
    double get_duration() const;
    double get_position() const;
    
    mpv_handle* get_handle() { return mpv_; }
    mpv_render_context* get_render_context() { return render_ctx_; }

private:
    mpv_handle* mpv_ = nullptr;
    mpv_render_context* render_ctx_ = nullptr;
    std::function<void()> wakeup_callback_;
    
    static void on_mpv_events(void* ctx);
};
