# CompilerFlags.cmake
#
# Set up compiler flags and options for the project

# Set C++ standard
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

# Compiler-specific flags
if(CMAKE_CXX_COMPILER_ID MATCHES "GNU|Clang")
    # Common GCC/Clang flags
    set(UNIVERSAL_WALLPAPER_CXX_FLAGS
        -Wall
        -Wextra
        -Wpedantic
        -Wno-unused-parameter
        -Wno-missing-field-initializers
    )
    
    # Debug flags
    set(UNIVERSAL_WALLPAPER_CXX_FLAGS_DEBUG
        -g
        -O0
        -DDEBUG
        -fsanitize=address
        -fsanitize=undefined
    )
    
    # Release flags
    set(UNIVERSAL_WALLPAPER_CXX_FLAGS_RELEASE
        -O3
        -DNDEBUG
        -flto
    )
    
    # Link flags for debug
    set(UNIVERSAL_WALLPAPER_LINK_FLAGS_DEBUG
        -fsanitize=address
        -fsanitize=undefined
    )
    
    # Link flags for release
    set(UNIVERSAL_WALLPAPER_LINK_FLAGS_RELEASE
        -flto
    )
    
elseif(CMAKE_CXX_COMPILER_ID STREQUAL "MSVC")
    # MSVC flags
    set(UNIVERSAL_WALLPAPER_CXX_FLAGS
        /W4
        /permissive-
    )
    
    set(UNIVERSAL_WALLPAPER_CXX_FLAGS_DEBUG
        /Od
        /Zi
        /DDEBUG
    )
    
    set(UNIVERSAL_WALLPAPER_CXX_FLAGS_RELEASE
        /O2
        /DNDEBUG
        /GL
    )
    
    set(UNIVERSAL_WALLPAPER_LINK_FLAGS_RELEASE
        /LTCG
    )
endif()

# Function to apply compiler flags to a target
function(set_wallpaper_ne_compile_options target)
    target_compile_options(${target} PRIVATE ${UNIVERSAL_WALLPAPER_CXX_FLAGS})
    target_compile_options(${target} PRIVATE
        $<$<CONFIG:Debug>:${UNIVERSAL_WALLPAPER_CXX_FLAGS_DEBUG}>
        $<$<CONFIG:Release>:${UNIVERSAL_WALLPAPER_CXX_FLAGS_RELEASE}>
        $<$<CONFIG:RelWithDebInfo>:${UNIVERSAL_WALLPAPER_CXX_FLAGS_RELEASE}>
        $<$<CONFIG:MinSizeRel>:${UNIVERSAL_WALLPAPER_CXX_FLAGS_RELEASE}>
    )
    
    target_link_options(${target} PRIVATE
        $<$<CONFIG:Debug>:${UNIVERSAL_WALLPAPER_LINK_FLAGS_DEBUG}>
        $<$<CONFIG:Release>:${UNIVERSAL_WALLPAPER_LINK_FLAGS_RELEASE}>
        $<$<CONFIG:RelWithDebInfo>:${UNIVERSAL_WALLPAPER_LINK_FLAGS_RELEASE}>
        $<$<CONFIG:MinSizeRel>:${UNIVERSAL_WALLPAPER_LINK_FLAGS_RELEASE}>
    )
endfunction()
