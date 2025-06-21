# Wallpaper Not-Engine Linux

A lightweight wallpaper application for Linux that supports both X11 and Wayland with video wallpapers, audio detection, and multi-monitor setups.
Made as an add-on for my [GUI project](https://github.com/MikiDevLog/wallpaperengine-gui).

## Features

- GPU acceleration + support for many video cards for decoding due to built-in MPV support
- Support for audio output + its control via flags
- Enough options for customization
- Tested on AMD rx6800xt video card on KDE Wayland
- Tested on 4k images and 4k video

# IMPORTANT

- Tests on x11 were not conducted, please, if you encounter any problems - open the issue and paste the log
- Tests on a multi-monitor setup were not conducted
- Be careful what videos you launch, some videos may have a codec whose HW decoding by your video card does not support, which can lead to a load on your CPU and not the GPU. Many 4k videos downloaded from YouTube may have AV1. If you have a card older than the RX 6xxx series or the RTX 30xx series, you will need to convert the video to a supported format (ffmpeg is the easiest and fastest option) (don't repeat my mistake, don't decode to H.264 10-bit, which may also be impossible to decode on your video card; make sure that the conversion is done to the 8-bit version).

### Supported Media Formats
Any format supported by MPV, including:
- MP4, AVI, MKV (video files)
- GIF (animated images)
- PNG, JPG (audio files)
- And many more...

## Prerequisites

### Required Dependencies
- **MPV**: Media playback library
- **OpenGL/EGL**: For rendering
- **PulseAudio**: Audio detection functionality
- **Wayland development libraries** (for Wayland support)
- **X11 development libraries** (for X11 support)

### Build Dependencies

#### Fedora / RHEL / CentOS
```bash
# Install development tools
sudo dnf groupinstall "Development Tools"
sudo dnf install cmake gcc-c++

# Install runtime dependencies
sudo dnf install mpv-devel mesa-libGL-devel mesa-libEGL-devel \
                 wayland-devel wayland-protocols-devel \
                 libX11-devel libXrandr-devel pulseaudio-libs-devel
```

#### Ubuntu / Debian
```bash
# Install development tools
sudo apt install build-essential cmake

# Install runtime dependencies
sudo apt install libmpv-dev libgl1-mesa-dev libegl1-mesa-dev \
                 libwayland-dev wayland-protocols \
                 libx11-dev libxrandr-dev libpulse-dev
```

#### Arch Linux
```bash
# Install development tools
sudo pacman -S base-devel cmake

# Install runtime dependencies
sudo pacman -S mpv mesa wayland wayland-protocols \
               libx11 libxrandr libpulse
```

## Building

```bash
git clone https://github.com/yourusername/wallpaper_not-engine_linux.git
cd wallpaper_not-engine_linux
mkdir build && cd build
cmake ..
make -j$(nproc)
```

## Usage

### Basic Usage
```bash
# Set a video wallpaper
./wallpaper_ne_linux /path/to/video.mp4

# Force specific backend
./wallpaper_ne_linux --force-wayland /path/to/video.mp4
./wallpaper_ne_linux --force-x11 /path/to/video.mp4

# Set wallpaper on specific output
./wallpaper_ne_linux --output DP-1 /path/to/video.mp4

# Advanced usage with multiple options
./wallpaper_ne_linux --fps 60 --scaling stretch --volume 0.8 /path/to/video.mp4

# Silent mode with no auto-mute
./wallpaper_ne_linux --silent --noautomute /path/to/video.mp4

# Using GUI compatibility flags
./wallpaper_ne_linux -b /path/to/video.mp4 -r DP-1 --silent
```

### Command Line Options
- `-h, --help` - Show help message
- `-o, --output OUTPUT` - Set wallpaper on specific output (can be used multiple times)
- `-r, --screen-root OUTPUT` - Alias for --output (for GUI compatibility)
- `-b, --bg PATH` - Alias for media path (for GUI compatibility)
- `-f, --fps FPS` - Set target FPS (default: 30)
- `-s, --silent` - Mute audio
- `-v, --verbose` - Enable verbose output
- `--noautomute` - Don't automatically mute audio when other apps play sound
- `--scaling MODE` - Scaling mode: stretch, fit, fill, default (default: fit)
- `--no-loop` - Don't loop the video
- `--no-hardware-decode` - Disable hardware decoding
- `--volume VOLUME` - Set audio volume (0.0-1.0 or 0-100, default: 0.5)
- `--mpv-options OPTIONS` - Additional MPV options
- `--force-x11` - Force X11 backend
- `--force-wayland` - Force Wayland backend
- `--log-level LEVEL` - Set log level (debug, info, warn, error)

## Contributing

Contributions are welcome! Please feel free to submit pull requests or open issues.

## License

This project is licensed under the MIT License - see the [LICENSE](LICENSE) file for details.
**TL;DR: Do what you want.**

## Troubleshooting

### Common Build Issues

#### Missing Dependencies
```bash
# Check for missing development packages
# On Fedora/RHEL/CentOS:
sudo dnf install wayland-scanner

# On Ubuntu/Debian:
sudo apt install wayland-scanner++
```

### Runtime Issues

#### Application Won't Start
```bash
# Check dependencies
ldd build/wallpaper_ne_linux

# Run with verbose output
wallpaper_ne_linux --verbose /path/to/media.mp4
```

#### Display Backend Issues
```bash
# Force specific backend if auto-detection fails
wallpaper_ne_linux --force-x11 /path/to/media.mp4
# or
wallpaper_ne_linux --force-wayland /path/to/media.mp4
```

# TODO

- Optimize CPU consumption
- Add cool features

# Plans:
- Add rendering of different widgets to the wallpaper surface with customization via json (aka web wallpapers from Wallpaper Engine)
- Make my own version of the scene engine
- Fully introduce fallback to the CPU and try to optimize it enough so that it does not stutter when playing 4k video