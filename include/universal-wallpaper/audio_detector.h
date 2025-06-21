#pragma once

#include <atomic>
#include <memory>
#include <thread>

// Forward declaration for PulseAudio types
struct pa_threaded_mainloop;
struct pa_context;

class AudioDetector {
public:
    AudioDetector();
    ~AudioDetector();
    
    // Initialize audio detection
    bool initialize();
    
    // Cleanup
    void cleanup();
    
    // Check if any other application is currently playing audio
    bool is_other_audio_playing() const;
    
    // Enable/disable auto-mute detection
    void set_enabled(bool enabled);
    bool is_enabled() const;

private:
    std::atomic<bool> enabled_{true};
    std::atomic<bool> other_audio_playing_{false};
    std::atomic<bool> should_stop_{false};
    
    pa_threaded_mainloop* mainloop_ = nullptr;
    pa_context* context_ = nullptr;
    
    std::unique_ptr<std::thread> monitor_thread_;
    
    void monitor_audio_sources();
    void setup_pulseaudio();
    void cleanup_pulseaudio();
    
    // PulseAudio callbacks
    static void context_state_callback(pa_context* context, void* userdata);
    static void sink_input_info_callback(pa_context* context, const struct pa_sink_input_info* info, int eol, void* userdata);
};
