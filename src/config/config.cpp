#include "universal-wallpaper/config.h"
#include <iostream>
#include <cstring>
#include <algorithm>

Config Config::parse_args(int argc, char* argv[]) {
    Config config;
    
    for (int i = 1; i < argc; i++) {
        std::string arg = argv[i];
        
        if (arg == "-h" || arg == "--help") {
            print_help(argv[0]);
            exit(0);
        }
        else if (arg == "-o" || arg == "--output") {
            if (i + 1 < argc) {
                config.outputs.push_back(argv[++i]);
            }
        }
        else if (arg == "--no-loop") {
            config.loop = false;
        }
        else if (arg == "--no-hardware-decode") {
            config.hardware_decode = false;
        }
        else if (arg == "--volume") {
            if (i + 1 < argc) {
                double vol = std::stod(argv[++i]);
                // Accept both 0.0-1.0 range and 0-100 range for convenience
                if (vol > 1.0) {
                    config.volume = vol / 100.0;  // Convert 0-100 to 0.0-1.0
                } else {
                    config.volume = vol;  // Already in 0.0-1.0 range
                }
                // Clamp to valid range
                config.volume = std::max(0.0, std::min(1.0, config.volume));
                
                // Volume > 0 enables audio unless explicitly muted
                if (config.volume > 0.0 && !config.silent) {
                    config.mute_audio = false;
                }
            }
        }
        else if (arg == "--mpv-options") {
            if (i + 1 < argc) {
                config.mpv_options = argv[++i];
            }
        }
        // New GUI compatibility flags
        else if (arg == "-f" || arg == "--fps") {
            if (i + 1 < argc) {
                config.fps = std::stoi(argv[++i]);
            }
        }
        else if (arg == "-s" || arg == "--silent") {
            config.silent = true;
            config.mute_audio = true;  // Silent means mute audio
        }
        else if (arg == "--noautomute") {
            config.noautomute = true;
        }
        else if (arg == "--scaling") {
            if (i + 1 < argc) {
                std::string scaling_value = argv[++i];
                if (scaling_value == "stretch" || scaling_value == "fit" || 
                    scaling_value == "fill" || scaling_value == "default") {
                    config.scaling = scaling_value;
                } else {
                    std::cerr << "Error: Invalid scaling mode. Use: stretch, fit, fill, or default\n";
                    exit(1);
                }
            }
        }
        else if (arg == "-r" || arg == "--screen-root") {
            if (i + 1 < argc) {
                config.screen_root = argv[++i];
                // Also add to outputs for compatibility
                config.outputs.clear();
                config.outputs.push_back(config.screen_root);
            }
        }
        else if (arg == "-b" || arg == "--bg") {
            if (i + 1 < argc) {
                config.background_id = argv[++i];
                // Set as media_path if not already set
                if (config.media_path.empty()) {
                    config.media_path = config.background_id;
                }
            }
        }
        else if (arg == "--force-x11") {
            config.force_x11 = true;
        }
        else if (arg == "--force-wayland") {
            config.force_wayland = true;
        }
        else if (arg == "-v" || arg == "--verbose") {
            config.verbose = true;
        }
        else if (arg == "-d" || arg == "--daemon") {
            config.daemon = true;
        }
        else if (arg == "--log-level") {
            if (i + 1 < argc) {
                config.log_level = argv[++i];
            }
        }
        else if (arg == "--no-adaptive-fps") {
            config.adaptive_fps = false;
        }
        else if (arg[0] != '-') {
            config.media_path = arg;
        }
    }
    
    if (config.media_path.empty()) {
        std::cerr << "Error: Media path is required\n";
        print_help(argv[0]);
        exit(1);
    }
    
    // If no outputs specified, use all available
    if (config.outputs.empty()) {
        config.outputs.push_back("ALL");
    }
    
    return config;
}

void Config::print_help(const char* program_name) {
    std::cout << "Wallpaper Not-Engine Linux - Video/Image wallpaper for X11 and Wayland\n\n";
    std::cout << "Usage: " << program_name << " [OPTIONS] <media_path>\n\n";
    std::cout << "Options:\n";
    std::cout << "  -h, --help                 Show this help message\n";
    std::cout << "  -o, --output OUTPUT        Set wallpaper on specific output (can be used multiple times)\n";
    std::cout << "                             Use 'ALL' for all outputs (default)\n";
    std::cout << "  -r, --screen-root OUTPUT   Alias for --output (for GUI compatibility)\n";
    std::cout << "  -b, --bg PATH              Alias for media path (for GUI compatibility)\n";
    std::cout << "  -f, --fps FPS              Set target FPS (default: 30)\n";
    std::cout << "  -s, --silent               Mute audio\n";
    std::cout << "  --noautomute               Don't automatically mute audio when other apps play sound\n";
    std::cout << "  --scaling MODE             Scaling mode: stretch, fit, fill, default (default: fit)\n";
    std::cout << "  --no-loop                  Don't loop the video\n";
    std::cout << "  --no-hardware-decode       Disable hardware decoding\n";
    std::cout << "  --volume VOLUME            Set audio volume (0.0-1.0 or 0-100, default: 0.5)\n";
    std::cout << "  --mpv-options OPTIONS      Additional MPV options\n";
    std::cout << "  --force-x11                Force X11 backend\n";
    std::cout << "  --force-wayland            Force Wayland backend\n";
    std::cout << "  -v, --verbose              Enable verbose output\n";
    std::cout << "  -d, --daemon               Run as daemon\n";
    std::cout << "  --log-level LEVEL          Set log level (debug, info, warn, error)\n";
    std::cout << "  --no-adaptive-fps          Disable adaptive FPS (always render at target FPS)\n";
    std::cout << "\nExamples:\n";
    std::cout << "  " << program_name << " /path/to/video.mp4\n";
    std::cout << "  " << program_name << " -o DP-1 /path/to/video.mp4\n";
    std::cout << "  " << program_name << " --fps 60 --scaling stretch --volume 0.8 /path/to/video.mp4\n";
    std::cout << "  " << program_name << " -b /path/to/video.mp4 -r DP-1 --silent\n";
    std::cout << "  " << program_name << " --volume 1.0 --noautomute /path/to/video.mp4\n";
    std::cout << "  " << program_name << " --mpv-options \"--shuffle\" /path/to/playlist\n";
}
