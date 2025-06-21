# FindWaylandProtocols.cmake
# 
# Find and generate Wayland protocol bindings
#
# Variables set by this module:
#   WAYLAND_PROTOCOLS_FOUND - True if wayland-protocols are available
#   WAYLAND_SCANNER_EXECUTABLE - Path to wayland-scanner
#   WAYLAND_PROTOCOLS_DIR - Directory containing protocol files
#
# Functions provided:
#   wayland_generate_protocol(target protocol_file)
#   wayland_download_protocol(url output_file)

find_program(WAYLAND_SCANNER_EXECUTABLE wayland-scanner)
find_program(PKG_CONFIG_EXECUTABLE pkg-config)
find_program(WGET_PROGRAM wget)
find_program(CURL_PROGRAM curl)

if(PKG_CONFIG_EXECUTABLE)
    execute_process(
        COMMAND ${PKG_CONFIG_EXECUTABLE} --variable=pkgdatadir wayland-protocols
        OUTPUT_VARIABLE WAYLAND_PROTOCOLS_DIR
        OUTPUT_STRIP_TRAILING_WHITESPACE
        RESULT_VARIABLE WAYLAND_PROTOCOLS_DIR_RESULT
    )
    
    if(WAYLAND_PROTOCOLS_DIR_RESULT EQUAL 0)
        set(WAYLAND_PROTOCOLS_FOUND TRUE)
    endif()
endif()

if(NOT WAYLAND_PROTOCOLS_FOUND)
    # Fallback: look for common installation paths
    find_path(WAYLAND_PROTOCOLS_DIR
        NAMES stable/xdg-shell/xdg-shell.xml
        PATHS
            /usr/share/wayland-protocols
            /usr/local/share/wayland-protocols
    )
    
    if(WAYLAND_PROTOCOLS_DIR)
        set(WAYLAND_PROTOCOLS_FOUND TRUE)
    endif()
endif()

include(FindPackageHandleStandardArgs)
find_package_handle_standard_args(WaylandProtocols
    REQUIRED_VARS
        WAYLAND_SCANNER_EXECUTABLE
    FOUND_VAR WAYLAND_PROTOCOLS_FOUND
)

# Function to download protocol files
function(wayland_download_protocol url output_file)
    if(NOT EXISTS ${output_file})
        message(STATUS "Downloading protocol from ${url}")
        if(WGET_PROGRAM)
            execute_process(
                COMMAND ${WGET_PROGRAM} -O ${output_file} ${url}
                RESULT_VARIABLE DOWNLOAD_RESULT
            )
        elseif(CURL_PROGRAM)
            execute_process(
                COMMAND ${CURL_PROGRAM} -o ${output_file} ${url}
                RESULT_VARIABLE DOWNLOAD_RESULT
            )
        else()
            message(FATAL_ERROR "Neither wget nor curl found. Cannot download protocol files.")
        endif()
        
        if(DOWNLOAD_RESULT)
            message(WARNING "Failed to download protocol from ${url}")
        else()
            message(STATUS "Successfully downloaded protocol to ${output_file}")
        endif()
    endif()
endfunction()

# Function to generate protocol bindings
function(wayland_generate_protocol target protocol_file)
    if(NOT WAYLAND_PROTOCOLS_FOUND)
        message(FATAL_ERROR "Wayland protocols not found")
    endif()
    
    get_filename_component(protocol_name ${protocol_file} NAME_WE)
    set(protocol_h "${CMAKE_CURRENT_BINARY_DIR}/${protocol_name}.h")
    set(protocol_c "${CMAKE_CURRENT_BINARY_DIR}/${protocol_name}.c")
    
    # Generate header
    add_custom_command(
        OUTPUT ${protocol_h}
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} client-header ${protocol_file} ${protocol_h}
        DEPENDS ${protocol_file}
        COMMENT "Generating ${protocol_name}.h"
    )
    
    # Generate source
    add_custom_command(
        OUTPUT ${protocol_c}
        COMMAND ${WAYLAND_SCANNER_EXECUTABLE} private-code ${protocol_file} ${protocol_c}
        DEPENDS ${protocol_file}
        COMMENT "Generating ${protocol_name}.c"
    )
    
    target_sources(${target} PRIVATE ${protocol_h} ${protocol_c})
    target_include_directories(${target} PRIVATE ${CMAKE_CURRENT_BINARY_DIR})
endfunction()

mark_as_advanced(
    WAYLAND_SCANNER_EXECUTABLE
    WAYLAND_PROTOCOLS_DIR
    WGET_PROGRAM
    CURL_PROGRAM
)
