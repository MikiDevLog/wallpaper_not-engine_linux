#include "universal-wallpaper/audio_detector.h"
#include "universal-wallpaper/utils.h"
#include <pulse/pulseaudio.h>
#include <pulse/introspect.h>
#include <chrono>
#include <cstring>
#include <stdexcept>

AudioDetector::AudioDetector() = default;

AudioDetector::~AudioDetector() {
    cleanup();
}

bool AudioDetector::initialize() {
    if (!enabled_) {
        return true;  // Success, but disabled
    }
    
    log_debug("Initializing audio detector");
    
    try {
        setup_pulseaudio();
        
        // Start monitoring thread
        should_stop_ = false;
        monitor_thread_ = std::make_unique<std::thread>(&AudioDetector::monitor_audio_sources, this);
        
        log_info("Audio detector initialized successfully");
        return true;
    } catch (const std::exception& e) {
        log_error("Failed to initialize audio detector: " + std::string(e.what()));
        cleanup_pulseaudio();
        return false;
    }
}

void AudioDetector::cleanup() {
    should_stop_ = true;
    
    if (monitor_thread_ && monitor_thread_->joinable()) {
        monitor_thread_->join();
    }
    
    cleanup_pulseaudio();
}

bool AudioDetector::is_other_audio_playing() const {
    return enabled_ && other_audio_playing_;
}

void AudioDetector::set_enabled(bool enabled) {
    enabled_ = enabled;
    if (!enabled) {
        other_audio_playing_ = false;
    }
}

bool AudioDetector::is_enabled() const {
    return enabled_;
}

void AudioDetector::setup_pulseaudio() {
    mainloop_ = pa_threaded_mainloop_new();
    if (!mainloop_) {
        throw std::runtime_error("Failed to create PulseAudio mainloop");
    }
    
    pa_mainloop_api* mainloop_api = pa_threaded_mainloop_get_api(mainloop_);
    context_ = pa_context_new(mainloop_api, "wallpaper-ne-linux-audio-detector");
    if (!context_) {
        throw std::runtime_error("Failed to create PulseAudio context");
    }
    
    pa_context_set_state_callback(context_, context_state_callback, this);
    
    if (pa_threaded_mainloop_start(mainloop_) < 0) {
        throw std::runtime_error("Failed to start PulseAudio mainloop");
    }
    
    pa_threaded_mainloop_lock(mainloop_);
    
    if (pa_context_connect(context_, nullptr, PA_CONTEXT_NOFLAGS, nullptr) < 0) {
        pa_threaded_mainloop_unlock(mainloop_);
        throw std::runtime_error("Failed to connect to PulseAudio server");
    }
    
    // Wait for context to be ready
    for (;;) {
        pa_context_state_t state = pa_context_get_state(context_);
        if (state == PA_CONTEXT_READY) {
            break;
        }
        if (!PA_CONTEXT_IS_GOOD(state)) {
            pa_threaded_mainloop_unlock(mainloop_);
            throw std::runtime_error("PulseAudio context failed");
        }
        pa_threaded_mainloop_wait(mainloop_);
    }
    
    pa_threaded_mainloop_unlock(mainloop_);
}

void AudioDetector::cleanup_pulseaudio() {
    if (context_) {
        pa_context_disconnect(context_);
        pa_context_unref(context_);
        context_ = nullptr;
    }
    
    if (mainloop_) {
        pa_threaded_mainloop_stop(mainloop_);
        pa_threaded_mainloop_free(mainloop_);
        mainloop_ = nullptr;
    }
}

void AudioDetector::monitor_audio_sources() {
    log_debug("Starting audio monitoring thread");
    
    while (!should_stop_) {
        if (!enabled_ || !context_) {
            std::this_thread::sleep_for(std::chrono::milliseconds(1000));
            continue;
        }
        
        pa_threaded_mainloop_lock(mainloop_);
        
        // Check sink inputs (applications playing audio)
        pa_operation* op = pa_context_get_sink_input_info_list(context_, sink_input_info_callback, this);
        if (op) {
            while (pa_operation_get_state(op) == PA_OPERATION_RUNNING) {
                pa_threaded_mainloop_wait(mainloop_);
            }
            pa_operation_unref(op);
        }
        
        pa_threaded_mainloop_unlock(mainloop_);
        
        // Check every 1000ms instead of 500ms to reduce CPU usage
        // Audio detection doesn't need to be as frequent
        std::this_thread::sleep_for(std::chrono::milliseconds(1000));
    }
    
    log_debug("Audio monitoring thread stopped");
}

void AudioDetector::context_state_callback(pa_context* context, void* userdata) {
    AudioDetector* detector = static_cast<AudioDetector*>(userdata);
    
    switch (pa_context_get_state(context)) {
        case PA_CONTEXT_READY:
        case PA_CONTEXT_TERMINATED:
        case PA_CONTEXT_FAILED:
            pa_threaded_mainloop_signal(detector->mainloop_, 0);
            break;
        default:
            break;
    }
}

void AudioDetector::sink_input_info_callback(pa_context* context, const pa_sink_input_info* info, int eol, void* userdata) {
    AudioDetector* detector = static_cast<AudioDetector*>(userdata);
    
    if (eol < 0) {
        log_debug("Failed to get sink input info");
        pa_threaded_mainloop_signal(detector->mainloop_, 0);
        return;
    }
    
    if (eol > 0) {
        // End of list
        pa_threaded_mainloop_signal(detector->mainloop_, 0);
        return;
    }
    
    if (info) {
        // Check if this is our own application or another application
        const char* app_name = pa_proplist_gets(info->proplist, PA_PROP_APPLICATION_NAME);
        const char* media_name = pa_proplist_gets(info->proplist, PA_PROP_MEDIA_NAME);
        (void)media_name; // Used for debugging, suppress unused warning
        
        bool is_our_app = false;
        if (app_name) {
            // Check if it's our wallpaper application or MPV
            is_our_app = (strstr(app_name, "wallpaper-ne-linux") != nullptr ||
                         strstr(app_name, "universal-wallpaper") != nullptr ||
                         strstr(app_name, "mpv") != nullptr ||
                         strstr(app_name, "wallpaper") != nullptr);
        }
        
        if (!is_our_app) {
            // Another application is playing audio - check if it's actually playing
            // In PulseAudio, if a sink input exists and is not corked, it's playing
            log_debug("Detected other audio playing: " + std::string(app_name ? app_name : "unknown"));
            detector->other_audio_playing_ = true;
            return;
        }
    }
    
    // If we get here and this is the last callback, no other audio is playing
    if (eol == 0) {
        detector->other_audio_playing_ = false;
    }
}
