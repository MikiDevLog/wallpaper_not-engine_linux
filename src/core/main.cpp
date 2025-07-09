#include "universal-wallpaper/config.h"
#include "universal-wallpaper/display_manager.h"
#include "universal-wallpaper/renderer.h"
#include "universal-wallpaper/mpv_wrapper.h"
#include "universal-wallpaper/utils.h"
#include "universal-wallpaper/audio_detector.h"
#include <iostream>
#include <csignal>
#include <atomic>
#include <chrono>
#include <thread>

std::atomic<bool> g_running{true};

void signal_handler(int signal) {
    if (signal == SIGINT || signal == SIGTERM) {
        log_info("Received signal " + std::to_string(signal) + ", shutting down...");
        g_running = false;
    }
}

int main(int argc, char* argv[]) {
    // Parse configuration
    Config config;
    try {
        config = Config::parse_args(argc, argv);
    } catch (const std::exception& e) {
        std::cerr << "Error parsing arguments: " << e.what() << std::endl;
        return 1;
    }
    
    // Set log level
    if (config.verbose) {
        set_log_level(LogLevel::LOG_DEBUG);
    } else if (config.log_level == "debug") {
        set_log_level(LogLevel::LOG_DEBUG);
    } else if (config.log_level == "warn") {
        set_log_level(LogLevel::LOG_WARN);
    } else if (config.log_level == "error") {
        set_log_level(LogLevel::LOG_ERROR);
    }
    
    log_info("Wallpaper Not-Engine Linux starting...");
    
    // Check if media file exists
    if (!file_exists(config.media_path)) {
        log_error("Media file not found: " + config.media_path);
        return 1;
    }
    
    // Daemonize if requested
    if (config.daemon) {
        log_info("Running as daemon");
        daemonize();
    }
    
    // Set up signal handling
    signal(SIGINT, signal_handler);
    signal(SIGTERM, signal_handler);
    
    try {
        // Initialize display manager
        DisplayManager display_manager;
        if (!display_manager.initialize(config.force_x11, config.force_wayland)) {
            log_error("Failed to initialize display manager");
            return 1;
        }
        
        log_info("Using " + display_manager.get_backend_name() + " backend");
        
        // Get available monitors
        auto monitors = display_manager.get_monitors();
        if (monitors.empty()) {
            log_error("No monitors found");
            return 1;
        }
        
        log_info("Available monitors:");
        for (const auto& monitor : monitors) {
            log_info("  " + monitor.name + ": " + 
                    std::to_string(monitor.width) + "x" + std::to_string(monitor.height) +
                    " at " + std::to_string(monitor.x) + "," + std::to_string(monitor.y) +
                    (monitor.primary ? " (primary)" : ""));
        }
        
        // Initialize renderer
        Renderer renderer;
        if (!renderer.initialize()) {
            log_error("Failed to initialize renderer");
            return 1;
        }
        
        // Create OpenGL context
        if (!renderer.create_context(display_manager.get_native_display())) {
            log_error("Failed to create OpenGL context");
            return 1;
        }
        
        // Set renderer on display manager for wallpaper rendering
        display_manager.set_renderer(&renderer);
        
        // Initialize MPV
        MPVWrapper mpv;
        
        // Handle audio settings
        bool final_mute_audio = config.mute_audio;
        if (config.silent) {
            final_mute_audio = true;
        }
        
        // Initialize audio detector for auto-mute functionality
        AudioDetector audio_detector;
        if (!config.noautomute && !final_mute_audio) {
            audio_detector.set_enabled(true);
            if (!audio_detector.initialize()) {
                log_warn("Failed to initialize audio detector, auto-mute will be disabled");
            } else {
                log_info("Audio detector initialized - will auto-mute when other apps play audio");
            }
        } else {
            audio_detector.set_enabled(false);
            if (config.noautomute) {
                log_info("Auto-mute disabled by --noautomute flag");
            }
        }
        
        if (!mpv.initialize(config.media_path, config.hardware_decode, config.loop,
                           final_mute_audio, config.volume, config.mpv_options)) {
            log_error("Failed to initialize MPV");
            return 1;
        }
        
        // Create MPV render context
        if (!mpv.create_render_context(Renderer::get_proc_address, &renderer)) {
            log_error("Failed to create MPV render context");
            return 1;
        }
        
        log_info("All components initialized successfully");
        
        // Main loop
        auto last_frame_time = std::chrono::steady_clock::now();
        auto last_event_time = std::chrono::steady_clock::now();
        auto last_audio_check_time = std::chrono::steady_clock::now();
        
        // Use FPS setting from config
        const auto frame_duration = std::chrono::milliseconds(1000 / config.fps);
        // Process events at a lower frequency to reduce CPU usage
        const auto event_duration = std::chrono::milliseconds(16); // ~60 FPS for events
        // Check audio status less frequently
        const auto audio_check_duration = std::chrono::milliseconds(100); // 10 FPS for audio checks
        
        log_info("Running at " + std::to_string(config.fps) + " FPS");
        log_debug("Starting main render loop");
        
        bool was_muted_by_detector = false;
        bool needs_redraw = true; // Force initial render
        
        while (g_running && !display_manager.should_quit()) {
            auto current_time = std::chrono::steady_clock::now();
            auto elapsed = current_time - last_frame_time;
            auto event_elapsed = current_time - last_event_time;
            auto audio_elapsed = current_time - last_audio_check_time;
            
            // Process events less frequently to reduce CPU usage
            if (event_elapsed >= event_duration) {
                display_manager.process_events();
                mpv.process_events();
                last_event_time = current_time;
            }
            
            // Process events less frequently to reduce CPU usage
            if (event_elapsed >= event_duration) {
                display_manager.process_events();
                mpv.process_events();
                last_event_time = current_time;
            }
            
            // Handle auto-mute based on other audio playing (less frequently)
            if (audio_elapsed >= audio_check_duration && audio_detector.is_enabled() && !final_mute_audio) {
                bool other_audio_playing = audio_detector.is_other_audio_playing();
                
                if (other_audio_playing && !was_muted_by_detector) {
                    // Mute wallpaper audio when other apps are playing
                    mpv.set_property("mute", "yes");
                    was_muted_by_detector = true;
                    log_debug("Auto-muted wallpaper audio (other app playing)");
                } else if (!other_audio_playing && was_muted_by_detector) {
                    // Unmute wallpaper audio when no other apps are playing
                    mpv.set_property("mute", "no");
                    was_muted_by_detector = false;
                    log_debug("Auto-unmuted wallpaper audio (no other apps playing)");
                }
                last_audio_check_time = current_time;
            }
            
            // Render frame if enough time has passed and we have new content
            bool should_render = needs_redraw || mpv.has_new_frame();
            
            // Advanced frame throttling to reduce memory bandwidth
            static auto last_render_time = std::chrono::steady_clock::now();
            auto render_elapsed = std::chrono::duration_cast<std::chrono::milliseconds>(current_time - last_render_time);
            
            // Skip render if we just rendered and MPV doesn't have new content
            if (render_elapsed < std::chrono::milliseconds(16) && !mpv.has_new_frame()) {
                should_render = false;
            }
            
            if (elapsed >= frame_duration && should_render) {
                log_debug("Rendering frame (new_content: " + std::string(mpv.has_new_frame() ? "true" : "false") + ")");
                renderer.make_current();
                last_render_time = current_time;
                
                // Render MPV frame to a framebuffer
                // For now, we'll use the screen dimensions of the primary monitor
                const auto& primary_monitor = monitors[0];
                
                // Calculate dimensions based on scaling mode
                int render_width = primary_monitor.width;
                int render_height = primary_monitor.height;
                
                // Apply scaling logic based on config.scaling
                if (config.scaling == "stretch") {
                    // Use full screen dimensions (already set above)
                    log_debug("Using stretch scaling: " + std::to_string(render_width) + "x" + std::to_string(render_height));
                } else if (config.scaling == "fit") {
                    // Maintain aspect ratio, fit within screen
                    // We'll let MPV handle this by using the existing scale_to_fit logic
                    log_debug("Using fit scaling: " + std::to_string(render_width) + "x" + std::to_string(render_height));
                } else if (config.scaling == "fill") {
                    // Fill the screen, may crop
                    log_debug("Using fill scaling: " + std::to_string(render_width) + "x" + std::to_string(render_height));
                } else { // "default"
                    // Use original scaling settings
                    log_debug("Using default scaling: " + std::to_string(render_width) + "x" + std::to_string(render_height));
                }
                
                auto fbo_info = renderer.get_or_create_framebuffer(render_width, render_height);
                
                log_debug("Created framebuffer: " + std::to_string(fbo_info.fbo) + ", texture: " + std::to_string(fbo_info.texture));
                
                if (fbo_info.fbo != 0) {
                    renderer.bind_framebuffer(fbo_info);
                    renderer.clear(0.0f, 0.0f, 0.0f, 1.0f);
                    
                    // Render MPV frame
                    log_debug("Calling MPV render_frame");
                    if (mpv.render_frame(fbo_info.fbo, fbo_info.width, fbo_info.height)) {
                        log_debug("MPV rendered frame successfully");
                        mpv.report_flip();
                        
                        // Only set wallpaper if MPV has video content
                        if (mpv.has_video() && mpv.is_playing()) {
                            // Set wallpaper for specified outputs
                            for (const auto& output_name : config.outputs) {
                                log_debug("Setting wallpaper for output: " + output_name);
                                if (output_name == "ALL") {
                                    display_manager.set_wallpaper_all(fbo_info.texture, 
                                                                     fbo_info.width, fbo_info.height);
                                } else {
                                    display_manager.set_wallpaper(output_name, fbo_info.texture,
                                                                 fbo_info.width, fbo_info.height);
                                }
                            }
                            needs_redraw = false; // Reset redraw flag after successful render
                        } else {
                            static int wait_counter = 0;
                            wait_counter++;
                            if (wait_counter == 1 || wait_counter % 60 == 0) { // Log initially, then every 2 seconds at 30fps
                                log_info("Waiting for MPV to start playing video (has_video: " + 
                                         std::string(mpv.has_video() ? "true" : "false") + 
                                         ", is_playing: " + std::string(mpv.is_playing() ? "true" : "false") + 
                                         ", duration: " + std::to_string(mpv.get_duration()) + "s)");
                            }
                        }
                    } else {
                        log_debug("MPV render_frame returned false");
                    }
                    
                    renderer.bind_default_framebuffer();
                    // Don't destroy the cached framebuffer - it will be reused
                } else {
                    log_debug("Failed to create framebuffer");
                }
                
                last_frame_time = current_time;
            }
            
            // Calculate optimal sleep time based on remaining time until next frame
            auto time_until_next_frame = frame_duration - elapsed;
            auto time_until_next_event = event_duration - event_elapsed;
            auto time_until_next_audio = audio_check_duration - audio_elapsed;
            
            // Sleep until the next required operation
            auto min_sleep = std::min({time_until_next_frame, time_until_next_event, time_until_next_audio});
            if (min_sleep > std::chrono::milliseconds(1)) {
                std::this_thread::sleep_for(min_sleep / 2); // Sleep for half the time to avoid oversleeping
            } else {
                std::this_thread::sleep_for(std::chrono::milliseconds(1));
            }
        }
        
        log_info("Shutting down...");
        
    } catch (const std::exception& e) {
        log_error("Fatal error: " + std::string(e.what()));
        return 1;
    }
    
    log_info("Wallpaper Not-Engine Linux terminated successfully");
    return 0;
}
