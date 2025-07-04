cmake_minimum_required(VERSION 3.16)
project(wallpaper_ne_linux 
    VERSION 1.0.0 
    DESCRIPTION "Wallpaper Not-Engine Linux - wallpaper application for X11 and Wayland"
    LANGUAGES C CXX)

# Add our cmake modules to the module path
list(APPEND CMAKE_MODULE_PATH "${CMAKE_CURRENT_SOURCE_DIR}/cmake")

# Include compiler flags configuration
include(CompilerFlags)

# Find required packages
find_package(PkgConfig REQUIRED)

# MPV (for media playback)
pkg_check_modules(MPV REQUIRED mpv)

# OpenGL/EGL (for rendering)
find_package(OpenGL REQUIRED)
pkg_check_modules(EGL REQUIRED egl)

# X11 support
find_package(X11 REQUIRED)
pkg_check_modules(XRANDR REQUIRED xrandr)

# Wayland support
pkg_check_modules(WAYLAND_CLIENT REQUIRED wayland-client)
pkg_check_modules(WAYLAND_EGL REQUIRED wayland-egl)

# PulseAudio support (for audio detection)
pkg_check_modules(PULSEAUDIO REQUIRED libpulse)

# Protocol generation for Wayland
find_program(WAYLAND_SCANNER wayland-scanner)
find_program(WGET_PROGRAM wget)
find_program(CURL_PROGRAM curl)

if(NOT WAYLAND_SCANNER)
    message(WARNING "wayland-scanner not found, Wayland support will be limited")
    set(WAYLAND_PROTOCOLS_AVAILABLE FALSE)
else()
    set(WAYLAND_PROTOCOLS_AVAILABLE TRUE)
endif()

# Generate wayland protocol files
set(PROTOCOLS_DIR "${CMAKE_CURRENT_SOURCE_DIR}/protocols")
file(MAKE_DIRECTORY ${PROTOCOLS_DIR})

set(PROTOCOL_SOURCES "")

if(WAYLAND_PROTOCOLS_AVAILABLE)
    # Download wlroots protocols if not found locally
    set(WLR_LAYER_SHELL_XML "${PROTOCOLS_DIR}/wlr-layer-shell-unstable-v1.xml")
    set(XDG_OUTPUT_XML "${PROTOCOLS_DIR}/xdg-output-unstable-v1.xml")
    set(XDG_SHELL_XML "${PROTOCOLS_DIR}/xdg-shell.xml")
    
    # Check if protocol XML files exist, if not download them
    if(NOT EXISTS ${WLR_LAYER_SHELL_XML})
        message(STATUS "Downloading wlr-layer-shell-unstable-v1.xml")
        if(WGET_PROGRAM)
            execute_process(
                COMMAND ${WGET_PROGRAM} -O ${WLR_LAYER_SHELL_XML}
                https://gitlab.freedesktop.org/wlroots/wlr-protocols/-/raw/master/unstable/wlr-layer-shell-unstable-v1.xml
                RESULT_VARIABLE DOWNLOAD_RESULT
            )
        elseif(CURL_PROGRAM)
            execute_process(
                COMMAND ${CURL_PROGRAM} -o ${WLR_LAYER_SHELL_XML}
                https://gitlab.freedesktop.org/wlroots/wlr-protocols/-/raw/master/unstable/wlr-layer-shell-unstable-v1.xml
                RESULT_VARIABLE DOWNLOAD_RESULT
            )
        else()
            message(FATAL_ERROR "Neither wget nor curl found. Cannot download protocol files.")
        endif()
        
        if(DOWNLOAD_RESULT)
            message(WARNING "Failed to download wlr-layer-shell-unstable-v1.xml")
        endif()
    endif()
    
    if(NOT EXISTS ${XDG_OUTPUT_XML})
        message(STATUS "Downloading xdg-output-unstable-v1.xml")
        if(WGET_PROGRAM)
            execute_process(
                COMMAND ${WGET_PROGRAM} -O ${XDG_OUTPUT_XML}
                https://gitlab.freedesktop.org/wayland/wayland-protocols/-/raw/main/unstable/xdg-output/xdg-output-unstable-v1.xml
                RESULT_VARIABLE DOWNLOAD_RESULT
            )
        elseif(CURL_PROGRAM)
            execute_process(
                COMMAND ${CURL_PROGRAM} -o ${XDG_OUTPUT_XML}
                https://gitlab.freedesktop.org/wayland/wayland-protocols/-/raw/main/unstable/xdg-output/xdg-output-unstable-v1.xml
                RESULT_VARIABLE DOWNLOAD_RESULT
            )
        endif()
        
        if(DOWNLOAD_RESULT)
            message(WARNING "Failed to download xdg-output-unstable-v1.xml")
        endif()
    endif()
    
    if(NOT EXISTS ${XDG_SHELL_XML})
        message(STATUS "Downloading xdg-shell.xml")
        if(WGET_PROGRAM)
            execute_process(
                COMMAND ${WGET_PROGRAM} -O ${XDG_SHELL_XML}
                https://gitlab.freedesktop.org/wayland/wayland-protocols/-/raw/main/stable/xdg-shell/xdg-shell.xml
                RESULT_VARIABLE DOWNLOAD_RESULT
            )
        elseif(CURL_PROGRAM)
            execute_process(
                COMMAND ${CURL_PROGRAM} -o ${XDG_SHELL_XML}
                https://gitlab.freedesktop.org/wayland/wayland-protocols/-/raw/main/stable/xdg-shell/xdg-shell.xml
                RESULT_VARIABLE DOWNLOAD_RESULT
            )
        endif()
        
        if(DOWNLOAD_RESULT)
            message(WARNING "Failed to download xdg-shell.xml")
        endif()
    endif()
    
    # Generate protocol headers and sources
    set(PROTOCOL_SOURCES "")
    
    foreach(PROTOCOL_XML ${WLR_LAYER_SHELL_XML} ${XDG_OUTPUT_XML} ${XDG_SHELL_XML})
        if(EXISTS ${PROTOCOL_XML})
            get_filename_component(PROTOCOL_NAME ${PROTOCOL_XML} NAME_WE)
            set(PROTOCOL_H "${PROTOCOLS_DIR}/${PROTOCOL_NAME}.h")
            set(PROTOCOL_C "${PROTOCOLS_DIR}/${PROTOCOL_NAME}.c")
            
            # Generate header
            add_custom_command(
                OUTPUT ${PROTOCOL_H}
                COMMAND ${WAYLAND_SCANNER} client-header ${PROTOCOL_XML} ${PROTOCOL_H}
                DEPENDS ${PROTOCOL_XML}
                COMMENT "Generating ${PROTOCOL_NAME}.h"
            )
            
            # Generate source
            add_custom_command(
                OUTPUT ${PROTOCOL_C}
                COMMAND ${WAYLAND_SCANNER} private-code ${PROTOCOL_XML} ${PROTOCOL_C}
                DEPENDS ${PROTOCOL_XML}
                COMMENT "Generating ${PROTOCOL_NAME}.c"
            )
            
            list(APPEND PROTOCOL_SOURCES ${PROTOCOL_C})
        endif()
    endforeach()
else()
    # Fallback to pre-generated files if they exist
    set(PROTOCOL_SOURCES
        protocols/wlr-layer-shell-unstable-v1.c
        protocols/xdg-output-unstable-v1.c
        protocols/xdg-shell.c
    )
    
    # Verify protocol files exist
    foreach(PROTOCOL_FILE ${PROTOCOL_SOURCES})
        if(NOT EXISTS "${CMAKE_CURRENT_SOURCE_DIR}/${PROTOCOL_FILE}")
            message(WARNING "Protocol file ${PROTOCOL_FILE} not found")
            list(REMOVE_ITEM PROTOCOL_SOURCES ${PROTOCOL_FILE})
        endif()
    endforeach()
endif()

# Source files organized by component
set(CORE_SOURCES
    src/core/main.cpp
    src/core/utils.cpp
    src/core/display_manager.cpp
    src/core/fullscreen_detector.cpp
)

set(BACKEND_SOURCES
    src/backends/x11_backend.cpp
    src/backends/wayland_backend.cpp
)

set(MEDIA_SOURCES
    src/media/mpv_wrapper.cpp
)

set(AUDIO_SOURCES
    src/audio/audio_detector.cpp
)

set(RENDERING_SOURCES
    src/rendering/renderer.cpp
)

set(CONFIG_SOURCES
    src/config/config.cpp
)

# Combine all sources
set(ALL_SOURCES
    ${CORE_SOURCES}
    ${BACKEND_SOURCES}
    ${MEDIA_SOURCES}
    ${AUDIO_SOURCES}
    ${RENDERING_SOURCES}
    ${CONFIG_SOURCES}
    ${PROTOCOL_SOURCES}
)

# Include directories
set(INCLUDE_DIRS
    include
    protocols
    ${MPV_INCLUDE_DIRS}
    ${EGL_INCLUDE_DIRS}
    ${X11_INCLUDE_DIR}
    ${WAYLAND_CLIENT_INCLUDE_DIRS}
    ${WAYLAND_EGL_INCLUDE_DIRS}
    ${PULSEAUDIO_INCLUDE_DIRS}
)

# Create the main executable
add_executable(${PROJECT_NAME} ${ALL_SOURCES})

# Set C language for protocol files
foreach(PROTOCOL_FILE ${PROTOCOL_SOURCES})
    set_source_files_properties(${PROTOCOL_FILE} PROPERTIES LANGUAGE C)
endforeach()

# Apply compiler flags
set_wallpaper_ne_compile_options(${PROJECT_NAME})

# Set include directories
target_include_directories(${PROJECT_NAME} PRIVATE ${INCLUDE_DIRS})

# Link libraries
target_link_libraries(${PROJECT_NAME}
    ${MPV_LIBRARIES}
    ${OPENGL_LIBRARIES}
    ${EGL_LIBRARIES}
    ${X11_LIBRARIES}
    ${X11_Xrandr_LIB}
    ${WAYLAND_CLIENT_LIBRARIES}
    ${WAYLAND_EGL_LIBRARIES}
    ${PULSEAUDIO_LIBRARIES}
    pthread
    dl
)

# Set compile definitions and flags
target_compile_options(${PROJECT_NAME} PRIVATE
    ${MPV_CFLAGS_OTHER}
    ${EGL_CFLAGS_OTHER}
    ${WAYLAND_CLIENT_CFLAGS_OTHER}
    ${WAYLAND_EGL_CFLAGS_OTHER}
    ${PULSEAUDIO_CFLAGS_OTHER}
)

# Installation
install(TARGETS ${PROJECT_NAME} 
    RUNTIME DESTINATION bin
    COMPONENT Runtime
)

# Optional: Install development headers if needed
install(DIRECTORY include/
    DESTINATION include
    COMPONENT Development
    FILES_MATCHING PATTERN "*.h"
)

# Optional: Install examples
install(DIRECTORY examples/
    DESTINATION share/${PROJECT_NAME}/examples
    COMPONENT Examples
    FILES_MATCHING 
    PATTERN "*.mp4"
    PATTERN "*.gif" 
    PATTERN "*.jpg"
    PATTERN "*.png"
)

# Print build information
message(STATUS "")
message(STATUS "Wallpaper Not-Engine Linux Configuration Summary:")
message(STATUS "  Version: ${PROJECT_VERSION}")
message(STATUS "  Build type: ${CMAKE_BUILD_TYPE}")
message(STATUS "  Install prefix: ${CMAKE_INSTALL_PREFIX}")
message(STATUS "  C++ standard: ${CMAKE_CXX_STANDARD}")
message(STATUS "")
message(STATUS "Dependencies:")
message(STATUS "  MPV: ${MPV_VERSION}")
message(STATUS "  OpenGL: Found")
message(STATUS "  EGL: Found") 
message(STATUS "  X11: Found")
message(STATUS "  Wayland: Found")
message(STATUS "  PulseAudio: Found")
message(STATUS "")
message(STATUS "Components:")
message(STATUS "  Core: ${CORE_SOURCES}")
message(STATUS "  Backends: ${BACKEND_SOURCES}")
message(STATUS "  Media: ${MEDIA_SOURCES}")
message(STATUS "  Audio: ${AUDIO_SOURCES}")
message(STATUS "  Rendering: ${RENDERING_SOURCES}")
message(STATUS "  Config: ${CONFIG_SOURCES}")
if(PROTOCOL_SOURCES)
    message(STATUS "  Protocols: ${PROTOCOL_SOURCES}")
endif()
message(STATUS "")
