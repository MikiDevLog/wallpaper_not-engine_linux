#pragma once

#include <string>
#include <vector>

struct Config {
    std::string media_path;
    std::vector<std::string> outputs;
    bool loop = true;
    bool hardware_decode = true;
    bool mute_audio = false;                         // Enable audio by default
    double volume = 0.5;                             // Default to 50% volume
    std::string mpv_options;
    bool force_x11 = false;
    bool force_wayland = false;
    bool verbose = false;
    bool daemon = false;
    std::string log_level = "info";
    
    // New flags for GUI compatibility
    int fps = 30;                                    // -f, --fps
    bool silent = false;                             // -s, --silent (alias for mute_audio)
    bool noautomute = false;                         // --noautomute (don't auto-mute when other apps play audio)
    std::string scaling = "fit";                     // --scaling (stretch, fit, fill, default)
    std::string screen_root;                         // -r, --screen-root (alias for output)
    std::string background_id;                       // -b, --bg (alias for media_path)
    bool adaptive_fps = true;                        // Enable adaptive FPS for better CPU usage
    bool pause_on_fullscreen = true;                 // Pause when fullscreen apps are detected
    bool no_fullscreen_pause = false;                // Disable fullscreen pause detection
    
    static Config parse_args(int argc, char* argv[]);
    static void print_help(const char* program_name);
};
