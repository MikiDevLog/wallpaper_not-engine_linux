#include "universal-wallpaper/mpv_wrapper.h"
#include "universal-wallpaper/utils.h"
#include <iostream>
#include <sstream>

MPVWrapper::MPVWrapper() {
    mpv_ = mpv_create();
    if (!mpv_) {
        throw std::runtime_error("Failed to create MPV instance");
    }
}

MPVWrapper::~MPVWrapper() {
    destroy();
}

bool MPVWrapper::initialize(const std::string& media_path, bool hardware_decode, 
                           bool loop, bool mute_audio, double volume,
                           const std::string& additional_options) {
    if (!mpv_) return false;
    
    // Set basic options
    mpv_set_option_string(mpv_, "terminal", "no");
    mpv_set_option_string(mpv_, "msg-level", "all=no");
    
    if (hardware_decode) {
        mpv_set_option_string(mpv_, "hwdec", "auto-safe");
        mpv_set_option_string(mpv_, "hwdec-codecs", "all");
    } else {
        mpv_set_option_string(mpv_, "hwdec", "no");
    }
    
    if (loop) {
        mpv_set_option_string(mpv_, "loop-file", "inf");
        mpv_set_option_string(mpv_, "loop-playlist", "inf");
    }
    
    if (mute_audio) {
        mpv_set_option_string(mpv_, "audio", "no");
    } else {
        mpv_set_option_string(mpv_, "volume", std::to_string(volume * 100).c_str());
    }
    
    // Performance options
    mpv_set_option_string(mpv_, "vo", "libmpv");
    mpv_set_option_string(mpv_, "gpu-context", "auto");
    mpv_set_option_string(mpv_, "gpu-api", "auto");
    
    // Parse additional options
    if (!additional_options.empty()) {
        std::istringstream iss(additional_options);
        std::string option;
        while (iss >> option) {
            if (option.find("--") == 0) {
                option = option.substr(2);
                size_t eq_pos = option.find('=');
                if (eq_pos != std::string::npos) {
                    std::string key = option.substr(0, eq_pos);
                    std::string value = option.substr(eq_pos + 1);
                    mpv_set_option_string(mpv_, key.c_str(), value.c_str());
                } else {
                    std::string value;
                    if (iss >> value) {
                        mpv_set_option_string(mpv_, option.c_str(), value.c_str());
                    } else {
                        mpv_set_option_string(mpv_, option.c_str(), "yes");
                    }
                }
            }
        }
    }
    
    // Initialize MPV
    if (mpv_initialize(mpv_) < 0) {
        log_error("Failed to initialize MPV");
        return false;
    }
    
    // Load media
    const char* cmd[] = {"loadfile", media_path.c_str(), nullptr};
    if (mpv_command(mpv_, cmd) < 0) {
        log_error("Failed to load media: " + media_path);
        return false;
    }
    
    log_info("MPV initialized successfully with media: " + media_path);
    return true;
}

void MPVWrapper::destroy() {
    if (render_ctx_) {
        mpv_render_context_free(render_ctx_);
        render_ctx_ = nullptr;
    }
    
    if (mpv_) {
        mpv_terminate_destroy(mpv_);
        mpv_ = nullptr;
    }
}

bool MPVWrapper::create_render_context(void* (*get_proc_address)(void* ctx, const char* name), 
                                      void* get_proc_address_ctx) {
    if (!mpv_ || render_ctx_) return false;
    
    mpv_opengl_init_params gl_init_params{};
    gl_init_params.get_proc_address = get_proc_address;
    gl_init_params.get_proc_address_ctx = get_proc_address_ctx;
    
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_API_TYPE, (void*)MPV_RENDER_API_TYPE_OPENGL},
        {MPV_RENDER_PARAM_OPENGL_INIT_PARAMS, &gl_init_params},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };
    
    if (mpv_render_context_create(&render_ctx_, mpv_, params) < 0) {
        log_error("Failed to create MPV render context");
        return false;
    }
    
    // Set up render context update callback to detect when new frames are available
    mpv_render_context_set_update_callback(render_ctx_, 
        [](void* ctx) {
            MPVWrapper* wrapper = static_cast<MPVWrapper*>(ctx);
            wrapper->has_new_frame_ = true;
        }, this);
    
    log_info("MPV render context created successfully");
    return true;
}

void MPVWrapper::set_render_params(int width, int height, int fbo) {
    // This will be set during render_frame call
}

bool MPVWrapper::render_frame(int fbo, int width, int height) {
    if (!render_ctx_) return false;
    
    // MPV expects viewport coordinates
    int flip_y = 1; // OpenGL framebuffer coordinate system
    
    mpv_opengl_fbo opengl_fbo = {
        .fbo = fbo,
        .w = width,
        .h = height,
        .internal_format = 0, // 0 = auto-detect
    };
    
    mpv_render_param params[] = {
        {MPV_RENDER_PARAM_OPENGL_FBO, &opengl_fbo},
        {MPV_RENDER_PARAM_FLIP_Y, &flip_y},
        {MPV_RENDER_PARAM_INVALID, nullptr}
    };
    
    int result = mpv_render_context_render(render_ctx_, params);
    if (result < 0) {
        log_error("MPV render failed: " + std::string(mpv_error_string(result)));
        return false;
    }
    
    // Mark that we've rendered this frame
    has_new_frame_ = false;
    
    return true;
}

void MPVWrapper::report_flip() {
    if (render_ctx_) {
        mpv_render_context_report_swap(render_ctx_);
    }
}

void MPVWrapper::set_property(const std::string& name, const std::string& value) {
    if (mpv_) {
        mpv_set_property_string(mpv_, name.c_str(), value.c_str());
    }
}

void MPVWrapper::set_property_async(const std::string& name, const std::string& value) {
    if (mpv_) {
        mpv_set_property_string(mpv_, name.c_str(), value.c_str());
    }
}

std::string MPVWrapper::get_property(const std::string& name) const {
    if (!mpv_) return "";
    
    char* result = mpv_get_property_string(mpv_, name.c_str());
    if (result) {
        std::string value = result;
        mpv_free(result);
        return value;
    }
    return "";
}

void MPVWrapper::command(const std::string& cmd) {
    if (mpv_) {
        const char* args[] = {cmd.c_str(), nullptr};
        mpv_command(mpv_, args);
    }
}

void MPVWrapper::set_wakeup_callback(std::function<void()> callback) {
    wakeup_callback_ = callback;
    if (mpv_) {
        mpv_set_wakeup_callback(mpv_, on_mpv_events, this);
    }
}

void MPVWrapper::process_events() {
    if (!mpv_) return;
    
    while (true) {
        mpv_event* event = mpv_wait_event(mpv_, 0);
        if (event->event_id == MPV_EVENT_NONE) break;
        
        switch (event->event_id) {
            case MPV_EVENT_VIDEO_RECONFIG:
                log_debug("Video reconfigured");
                has_new_frame_ = true;
                break;
            case MPV_EVENT_PLAYBACK_RESTART:
                log_debug("Playback restarted");
                has_new_frame_ = true;
                break;
            case MPV_EVENT_END_FILE:
                log_debug("End of file reached");
                break;
            case MPV_EVENT_LOG_MESSAGE: {
                auto* msg = static_cast<mpv_event_log_message*>(event->data);
                log_debug("MPV: " + std::string(msg->text));
                break;
            }
            default:
                break;
        }
    }
}

bool MPVWrapper::is_playing() const {
    return mpv_ && get_property("pause") != "yes";
}

bool MPVWrapper::has_video() const {
    return mpv_ && !get_property("video-codec").empty();
}

double MPVWrapper::get_duration() const {
    if (!mpv_) return 0.0;
    std::string duration_str = get_property("duration");
    return duration_str.empty() ? 0.0 : std::stod(duration_str);
}

double MPVWrapper::get_position() const {
    if (!mpv_) return 0.0;
    std::string position_str = get_property("time-pos");
    return position_str.empty() ? 0.0 : std::stod(position_str);
}

bool MPVWrapper::has_new_frame() const {
    return has_new_frame_;
}

void MPVWrapper::mark_frame_rendered() {
    has_new_frame_ = false;
}

void MPVWrapper::on_mpv_events(void* ctx) {
    auto* wrapper = static_cast<MPVWrapper*>(ctx);
    if (wrapper && wrapper->wakeup_callback_) {
        wrapper->wakeup_callback_();
    }
}
