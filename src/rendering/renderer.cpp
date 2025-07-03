#include "universal-wallpaper/renderer.h"
#include "universal-wallpaper/utils.h"
#include <EGL/eglext.h>
#include <wayland-egl.h>
#include <iostream>
#include <vector>
#include <cstring>
#include <cstdlib>

// Fallback definitions for Wayland platform
#ifndef EGL_PLATFORM_WAYLAND_KHR
#define EGL_PLATFORM_WAYLAND_KHR 0x31D8
#endif

// EGL debugging functions
static const char* egl_error_string(EGLint error) {
    switch (error) {
        case EGL_SUCCESS: return "EGL_SUCCESS";
        case EGL_NOT_INITIALIZED: return "EGL_NOT_INITIALIZED";
        case EGL_BAD_ACCESS: return "EGL_BAD_ACCESS";
        case EGL_BAD_ALLOC: return "EGL_BAD_ALLOC";
        case EGL_BAD_ATTRIBUTE: return "EGL_BAD_ATTRIBUTE";
        case EGL_BAD_CONTEXT: return "EGL_BAD_CONTEXT";
        case EGL_BAD_CONFIG: return "EGL_BAD_CONFIG";
        case EGL_BAD_CURRENT_SURFACE: return "EGL_BAD_CURRENT_SURFACE";
        case EGL_BAD_DISPLAY: return "EGL_BAD_DISPLAY";
        case EGL_BAD_SURFACE: return "EGL_BAD_SURFACE";
        case EGL_BAD_MATCH: return "EGL_BAD_MATCH";
        case EGL_BAD_PARAMETER: return "EGL_BAD_PARAMETER";
        case EGL_BAD_NATIVE_PIXMAP: return "EGL_BAD_NATIVE_PIXMAP";
        case EGL_BAD_NATIVE_WINDOW: return "EGL_BAD_NATIVE_WINDOW";
        case EGL_CONTEXT_LOST: return "EGL_CONTEXT_LOST";
        default: return "Unknown EGL error";
    }
}

static void check_egl_error(const char* operation) {
    EGLint error = eglGetError();
    if (error != EGL_SUCCESS) {
        std::cerr << "[EGL ERROR] " << operation << " failed: " << egl_error_string(error) 
                  << " (0x" << std::hex << error << std::dec << ")" << std::endl;
    }
}

static void print_egl_config_info(EGLDisplay display, EGLConfig config) {
    EGLint value;
    
    std::cout << "[EGL CONFIG] Configuration details:" << std::endl;
    
    if (eglGetConfigAttrib(display, config, EGL_BUFFER_SIZE, &value))
        std::cout << "  Buffer Size: " << value << std::endl;
    if (eglGetConfigAttrib(display, config, EGL_RED_SIZE, &value))
        std::cout << "  Red Size: " << value << std::endl;
    if (eglGetConfigAttrib(display, config, EGL_GREEN_SIZE, &value))
        std::cout << "  Green Size: " << value << std::endl;
    if (eglGetConfigAttrib(display, config, EGL_BLUE_SIZE, &value))
        std::cout << "  Blue Size: " << value << std::endl;
    if (eglGetConfigAttrib(display, config, EGL_ALPHA_SIZE, &value))
        std::cout << "  Alpha Size: " << value << std::endl;
    if (eglGetConfigAttrib(display, config, EGL_DEPTH_SIZE, &value))
        std::cout << "  Depth Size: " << value << std::endl;
    if (eglGetConfigAttrib(display, config, EGL_STENCIL_SIZE, &value))
        std::cout << "  Stencil Size: " << value << std::endl;
    if (eglGetConfigAttrib(display, config, EGL_SURFACE_TYPE, &value)) {
        std::cout << "  Surface Types: ";
        if (value & EGL_WINDOW_BIT) std::cout << "WINDOW ";
        if (value & EGL_PIXMAP_BIT) std::cout << "PIXMAP ";
        if (value & EGL_PBUFFER_BIT) std::cout << "PBUFFER ";
        std::cout << std::endl;
    }
    if (eglGetConfigAttrib(display, config, EGL_RENDERABLE_TYPE, &value)) {
        std::cout << "  Renderable Types: ";
        if (value & EGL_OPENGL_BIT) std::cout << "OPENGL ";
        if (value & EGL_OPENGL_ES_BIT) std::cout << "OPENGL_ES ";
        if (value & EGL_OPENGL_ES2_BIT) std::cout << "OPENGL_ES2 ";
        if (value & EGL_OPENVG_BIT) std::cout << "OPENVG ";
        std::cout << std::endl;
    }
}

// Simple vertex shader for fullscreen quad (for future use)
[[maybe_unused]] static const char* vertex_shader_source = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec2 aTexCoord;

out vec2 TexCoord;

void main() {
    gl_Position = vec4(aPos, 0.0, 1.0);
    TexCoord = aTexCoord;
}
)";

// Simple fragment shader for texture rendering (for future use)
[[maybe_unused]] static const char* fragment_shader_source = R"(
#version 330 core
out vec4 FragColor;

in vec2 TexCoord;
uniform sampler2D ourTexture;

void main() {
    FragColor = texture(ourTexture, TexCoord);
}
)";

Renderer::Renderer() = default;

Renderer::~Renderer() {
    destroy();
}

bool Renderer::initialize() {
    log_info("Initializing renderer");
    return true;
}

void Renderer::destroy() {
    cleanup_framebuffer_cache();
    destroy_context();
}

bool Renderer::create_context(void* native_display) {
    return setup_egl(native_display);
}

bool Renderer::setup_egl(void* native_display) {
    bool supports_surfaceless = false;
    
    // Get EGL display - detect platform type
    if (native_display) {
        // Check if we're running under Wayland or X11
        const char* wayland_display = getenv("WAYLAND_DISPLAY");
        const char* display_env = getenv("DISPLAY");
        
        if (wayland_display && strlen(wayland_display) > 0) {
            // We're on Wayland
            egl_display_ = eglGetPlatformDisplay(EGL_PLATFORM_WAYLAND_KHR, native_display, nullptr);
            check_egl_error("eglGetPlatformDisplay(Wayland)");
            if (egl_display_ != EGL_NO_DISPLAY) {
                log_info("Using Wayland platform EGL display");
            }
        }
        
        if (egl_display_ == EGL_NO_DISPLAY) {
            // Try X11 or generic display
            egl_display_ = eglGetDisplay((EGLNativeDisplayType)native_display);
            check_egl_error("eglGetDisplay(X11/native)");
            if (egl_display_ != EGL_NO_DISPLAY) {
                if (display_env && strlen(display_env) > 0) {
                    log_info("Using X11 platform EGL display");
                } else {
                    log_info("Using generic EGL display");
                }
            }
        }
    } else {
        egl_display_ = eglGetDisplay(EGL_DEFAULT_DISPLAY);
        check_egl_error("eglGetDisplay(default)");
        log_info("Using default EGL display");
    }
    
    if (egl_display_ == EGL_NO_DISPLAY) {
        log_error("Failed to get EGL display");
        check_egl_error("eglGetDisplay final check");
        return false;
    }
    
    // Initialize EGL
    EGLint major, minor;
    if (!eglInitialize(egl_display_, &major, &minor)) {
        log_error("Failed to initialize EGL");
        check_egl_error("eglInitialize");
        return false;
    }
    
    log_info("EGL " + std::to_string(major) + "." + std::to_string(minor) + " initialized");
    
    // Print EGL vendor and version info
    const char* egl_vendor = eglQueryString(egl_display_, EGL_VENDOR);
    const char* egl_version = eglQueryString(egl_display_, EGL_VERSION);
    const char* extensions = eglQueryString(egl_display_, EGL_EXTENSIONS);
    
    log_info("EGL Vendor: " + std::string(egl_vendor ? egl_vendor : "Unknown"));
    log_info("EGL Version: " + std::string(egl_version ? egl_version : "Unknown"));
    std::cout << "[EGL DEBUG] Available extensions: " << (extensions ? extensions : "None") << std::endl;
    
    // Choose EGL config - check for surfaceless context support first
    EGLConfig configs[256];
    EGLint num_configs;
    
    // Check if EGL_KHR_surfaceless_context is available
    if (extensions && strstr(extensions, "EGL_KHR_surfaceless_context")) {
        supports_surfaceless = true;
        log_info("EGL_KHR_surfaceless_context is supported - using surfaceless rendering");
    }
    
    // First try: prefer pbuffer support if available, but don't require it
    EGLint preferred_attribs[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT,
        EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
        EGL_NONE
    };
    
    if (!eglChooseConfig(egl_display_, preferred_attribs, configs, 256, &num_configs)) {
        log_error("eglChooseConfig failed");
        check_egl_error("eglChooseConfig");
        return false;
    }
    
    if (num_configs == 0) {
        log_info("No EGL configurations with pbuffer support found, trying basic OpenGL support");
        
        // Fallback: basic OpenGL configuration (will use surfaceless context)
        EGLint basic_attribs[] = {
            EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT,
            EGL_NONE
        };
        
        if (!eglChooseConfig(egl_display_, basic_attribs, configs, 256, &num_configs) || num_configs == 0) {
            log_error("Failed to find any EGL configuration with OpenGL support");
            check_egl_error("eglChooseConfig basic");
            return false;
        }
    }
    
    log_info("Found " + std::to_string(num_configs) + " EGL configurations, using the first one");
    egl_config_ = configs[0];
    
    // Print detailed configuration info
    print_egl_config_info(egl_display_, egl_config_);
    
    // Bind OpenGL API
    if (!eglBindAPI(EGL_OPENGL_API)) {
        log_error("Failed to bind OpenGL API");
        check_egl_error("eglBindAPI");
        return false;
    }
    
    // Try to create OpenGL context with fallback versions
    EGLint context_attribs_33[] = {
        EGL_CONTEXT_MAJOR_VERSION, 3,
        EGL_CONTEXT_MINOR_VERSION, 3,
        EGL_CONTEXT_OPENGL_PROFILE_MASK, EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_NONE
    };
    
    egl_context_ = eglCreateContext(egl_display_, egl_config_, EGL_NO_CONTEXT, context_attribs_33);
    check_egl_error("eglCreateContext(3.3 core)");
    
    if (egl_context_ == EGL_NO_CONTEXT) {
        // Try OpenGL 3.0
        EGLint context_attribs_30[] = {
            EGL_CONTEXT_MAJOR_VERSION, 3,
            EGL_CONTEXT_MINOR_VERSION, 0,
            EGL_NONE
        };
        egl_context_ = eglCreateContext(egl_display_, egl_config_, EGL_NO_CONTEXT, context_attribs_30);
        check_egl_error("eglCreateContext(3.0)");
        
        if (egl_context_ == EGL_NO_CONTEXT) {
            // Try any OpenGL context
            EGLint context_attribs_any[] = { EGL_NONE };
            egl_context_ = eglCreateContext(egl_display_, egl_config_, EGL_NO_CONTEXT, context_attribs_any);
            check_egl_error("eglCreateContext(any)");
            
            if (egl_context_ == EGL_NO_CONTEXT) {
                log_error("Failed to create EGL context");
                check_egl_error("eglCreateContext final");
                return false;
            }
            log_info("Created basic OpenGL context");
        } else {
            log_info("Created OpenGL 3.0 context");
        }
    } else {
        log_info("Created OpenGL 3.3 core context");
    }
    
    // Create surface for rendering - try pbuffer first, then use surfaceless context
    EGLint pbuffer_attribs[] = {
        EGL_WIDTH, 1,
        EGL_HEIGHT, 1,
        EGL_NONE
    };
    
    // Check if the selected config supports pbuffer surfaces
    EGLint surface_type;
    if (eglGetConfigAttrib(egl_display_, egl_config_, EGL_SURFACE_TYPE, &surface_type) && 
        (surface_type & EGL_PBUFFER_BIT)) {
        
        log_info("Config supports pbuffer surfaces, creating pbuffer");
        egl_surface_ = eglCreatePbufferSurface(egl_display_, egl_config_, pbuffer_attribs);
        check_egl_error("eglCreatePbufferSurface");
        
        if (egl_surface_ == EGL_NO_SURFACE) {
            log_error("Failed to create pbuffer surface, falling back to surfaceless context");
            if (supports_surfaceless) {
                egl_surface_ = EGL_NO_SURFACE;
                log_info("Using surfaceless context (EGL_NO_SURFACE)");
            } else {
                log_error("Neither pbuffer nor surfaceless context are supported");
                return false;
            }
        } else {
            log_info("Created pbuffer surface successfully");
        }
    } else {
        log_info("Config does not support pbuffer surfaces");
        if (supports_surfaceless) {
            egl_surface_ = EGL_NO_SURFACE;
            log_info("Using surfaceless context (EGL_NO_SURFACE)");
        } else {
            log_error("Neither pbuffer nor surfaceless context are supported");
            return false;
        }
    }
    
    if (!make_current()) {
        check_egl_error("eglMakeCurrent");
        return false;
    }
    
    // Load OpenGL extensions
    if (!load_gl_extensions()) {
        log_error("Failed to load OpenGL extensions");
        return false;
    }
    
    log_info("OpenGL context created successfully");
    
    // Print OpenGL info
    const char* vendor = (const char*)glGetString(GL_VENDOR);
    const char* renderer = (const char*)glGetString(GL_RENDERER);
    const char* version = (const char*)glGetString(GL_VERSION);
    
    log_info("OpenGL Vendor: " + std::string(vendor ? vendor : "Unknown"));
    log_info("OpenGL Renderer: " + std::string(renderer ? renderer : "Unknown"));
    log_info("OpenGL Version: " + std::string(version ? version : "Unknown"));
    
    return true;
}

void Renderer::destroy_context() {
    if (egl_display_ != EGL_NO_DISPLAY) {
        eglMakeCurrent(egl_display_, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        
        if (egl_surface_ != EGL_NO_SURFACE) {
            eglDestroySurface(egl_display_, egl_surface_);
            egl_surface_ = EGL_NO_SURFACE;
        }
        
        if (egl_context_ != EGL_NO_CONTEXT) {
            eglDestroyContext(egl_display_, egl_context_);
            egl_context_ = EGL_NO_CONTEXT;
        }
        
        eglTerminate(egl_display_);
        egl_display_ = EGL_NO_DISPLAY;
    }
}

bool Renderer::make_current() {
    if (egl_display_ == EGL_NO_DISPLAY || egl_context_ == EGL_NO_CONTEXT) {
        return false;
    }
    
    // Note: egl_surface_ can be EGL_NO_SURFACE for surfaceless contexts
    return eglMakeCurrent(egl_display_, egl_surface_, egl_surface_, egl_context_);
}

void Renderer::swap_buffers() {
    if (egl_display_ != EGL_NO_DISPLAY && egl_surface_ != EGL_NO_SURFACE) {
        eglSwapBuffers(egl_display_, egl_surface_);
    }
}

Renderer::FramebufferInfo Renderer::create_framebuffer(int width, int height) {
    FramebufferInfo info{};
    info.width = width;
    info.height = height;
    
    // Create texture
    glGenTextures(1, &info.texture);
    glBindTexture(GL_TEXTURE_2D, info.texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    // Create framebuffer
    glGenFramebuffers(1, &info.fbo);
    glBindFramebuffer(GL_FRAMEBUFFER, info.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, info.texture, 0);
    
    if (glCheckFramebufferStatus(GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE) {
        log_error("Framebuffer not complete");
        destroy_framebuffer(info);
        return {};
    }
    
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
    check_gl_error("create_framebuffer");
    
    return info;
}

void Renderer::destroy_framebuffer(const FramebufferInfo& info) {
    if (info.fbo != 0) {
        glDeleteFramebuffers(1, &info.fbo);
    }
    if (info.texture != 0) {
        glDeleteTextures(1, &info.texture);
    }
}

void Renderer::bind_framebuffer(const FramebufferInfo& info) {
    glBindFramebuffer(GL_FRAMEBUFFER, info.fbo);
    set_viewport(0, 0, info.width, info.height);
}

void Renderer::bind_default_framebuffer() {
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void Renderer::clear(float r, float g, float b, float a) {
    glClearColor(r, g, b, a);
    glClear(GL_COLOR_BUFFER_BIT);
}

void Renderer::set_viewport(int x, int y, int width, int height) {
    glViewport(x, y, width, height);
}

GLuint Renderer::create_texture(int width, int height, const void* data) {
    GLuint texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGB, width, height, 0, GL_RGB, GL_UNSIGNED_BYTE, data);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    check_gl_error("create_texture");
    return texture;
}

void Renderer::update_texture(GLuint texture, int width, int height, const void* data) {
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, width, height, GL_RGB, GL_UNSIGNED_BYTE, data);
    glBindTexture(GL_TEXTURE_2D, 0);
    
    check_gl_error("update_texture");
}

void Renderer::destroy_texture(GLuint texture) {
    if (texture != 0) {
        glDeleteTextures(1, &texture);
    }
}

GLuint Renderer::compile_shader(GLenum type, const char* source) {
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);
    
    GLint success;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetShaderInfoLog(shader, 512, nullptr, info_log);
        log_error("Shader compilation failed: " + std::string(info_log));
        glDeleteShader(shader);
        return 0;
    }
    
    return shader;
}

GLuint Renderer::create_program(const char* vertex_source, const char* fragment_source) {
    GLuint vertex_shader = compile_shader(GL_VERTEX_SHADER, vertex_source);
    GLuint fragment_shader = compile_shader(GL_FRAGMENT_SHADER, fragment_source);
    
    if (vertex_shader == 0 || fragment_shader == 0) {
        if (vertex_shader != 0) glDeleteShader(vertex_shader);
        if (fragment_shader != 0) glDeleteShader(fragment_shader);
        return 0;
    }
    
    GLuint program = glCreateProgram();
    glAttachShader(program, vertex_shader);
    glAttachShader(program, fragment_shader);
    glLinkProgram(program);
    
    GLint success;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info_log[512];
        glGetProgramInfoLog(program, 512, nullptr, info_log);
        log_error("Program linking failed: " + std::string(info_log));
        glDeleteProgram(program);
        program = 0;
    }
    
    glDeleteShader(vertex_shader);
    glDeleteShader(fragment_shader);
    
    return program;
}

void Renderer::use_program(GLuint program) {
    glUseProgram(program);
}

void Renderer::destroy_program(GLuint program) {
    if (program != 0) {
        glDeleteProgram(program);
    }
}

void* Renderer::get_proc_address(void* ctx, const char* name) {
    return (void*)eglGetProcAddress(name);
}

void Renderer::check_gl_error(const char* operation) {
    GLenum error = glGetError();
    if (error != GL_NO_ERROR) {
        log_error("OpenGL error in " + std::string(operation) + ": " + std::to_string(error));
    }
}

// OpenGL debugging functions
bool Renderer::load_gl_extensions() {
    // Load OpenGL extension functions
    glGenFramebuffers = (PFNGLGENFRAMEBUFFERSPROC)eglGetProcAddress("glGenFramebuffers");
    glBindFramebuffer = (PFNGLBINDFRAMEBUFFERPROC)eglGetProcAddress("glBindFramebuffer");
    glFramebufferTexture2D = (PFNGLFRAMEBUFFERTEXTURE2DPROC)eglGetProcAddress("glFramebufferTexture2D");
    glCheckFramebufferStatus = (PFNGLCHECKFRAMEBUFFERSTATUSPROC)eglGetProcAddress("glCheckFramebufferStatus");
    glDeleteFramebuffers = (PFNGLDELETEFRAMEBUFFERSPROC)eglGetProcAddress("glDeleteFramebuffers");
    glCreateShader = (PFNGLCREATESHADERPROC)eglGetProcAddress("glCreateShader");
    glShaderSource = (PFNGLSHADERSOURCEPROC)eglGetProcAddress("glShaderSource");
    glCompileShader = (PFNGLCOMPILESHADERPROC)eglGetProcAddress("glCompileShader");
    glGetShaderiv = (PFNGLGETSHADERIVPROC)eglGetProcAddress("glGetShaderiv");
    glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)eglGetProcAddress("glGetShaderInfoLog");
    glDeleteShader = (PFNGLDELETESHADERPROC)eglGetProcAddress("glDeleteShader");
    glCreateProgram = (PFNGLCREATEPROGRAMPROC)eglGetProcAddress("glCreateProgram");
    glAttachShader = (PFNGLATTACHSHADERPROC)eglGetProcAddress("glAttachShader");
    glLinkProgram = (PFNGLLINKPROGRAMPROC)eglGetProcAddress("glLinkProgram");
    glGetProgramiv = (PFNGLGETPROGRAMIVPROC)eglGetProcAddress("glGetProgramiv");
    glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)eglGetProcAddress("glGetProgramInfoLog");
    glDeleteProgram = (PFNGLDELETEPROGRAMPROC)eglGetProcAddress("glDeleteProgram");
    glUseProgram = (PFNGLUSEPROGRAMPROC)eglGetProcAddress("glUseProgram");
    
    // Load additional OpenGL functions for rendering
    glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)eglGetProcAddress("glGenVertexArrays");
    glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)eglGetProcAddress("glBindVertexArray");
    glGenBuffers = (PFNGLGENBUFFERSPROC)eglGetProcAddress("glGenBuffers");
    glBindBuffer = (PFNGLBINDBUFFERPROC)eglGetProcAddress("glBindBuffer");
    glBufferData = (PFNGLBUFFERDATAPROC)eglGetProcAddress("glBufferData");
    glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)eglGetProcAddress("glVertexAttribPointer");
    glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)eglGetProcAddress("glEnableVertexAttribArray");
    glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)eglGetProcAddress("glGetUniformLocation");
    glUniform1i = (PFNGLUNIFORM1IPROC)eglGetProcAddress("glUniform1i");
    
    if (!glGenFramebuffers || !glBindFramebuffer || !glFramebufferTexture2D ||
        !glCheckFramebufferStatus || !glDeleteFramebuffers || !glCreateShader ||
        !glShaderSource || !glCompileShader || !glGetShaderiv ||
        !glGetShaderInfoLog || !glDeleteShader || !glCreateProgram ||
        !glAttachShader || !glLinkProgram || !glGetProgramiv ||
        !glGetProgramInfoLog || !glDeleteProgram || !glUseProgram ||
        !glGenVertexArrays || !glBindVertexArray || !glGenBuffers ||
        !glBindBuffer || !glBufferData || !glVertexAttribPointer ||
        !glEnableVertexAttribArray || !glGetUniformLocation || !glUniform1i) {
        std::cerr << "Failed to load OpenGL extension functions" << std::endl;
        return false;
    }
    
    return true;
}

EGLSurface Renderer::create_egl_surface_for_wayland(wl_egl_window* egl_window) {
    if (!egl_window) {
        log_error("Invalid EGL window");
        return EGL_NO_SURFACE;
    }
    
    EGLSurface surface = eglCreateWindowSurface(egl_display_, egl_config_, (EGLNativeWindowType)egl_window, nullptr);
    check_egl_error("eglCreateWindowSurface");
    
    if (surface == EGL_NO_SURFACE) {
        log_error("Failed to create EGL window surface");
        return EGL_NO_SURFACE;
    }
    
    log_debug("Created EGL window surface successfully");
    return surface;
}

bool Renderer::render_texture_to_surface(EGLSurface target_surface, GLuint texture, int surface_width, int surface_height) {
    if (target_surface == EGL_NO_SURFACE || texture == 0) {
        return false;
    }
    
    // Save current surface
    EGLSurface current_surface = eglGetCurrentSurface(EGL_DRAW);
    
    // Make the target surface current
    if (!eglMakeCurrent(egl_display_, target_surface, target_surface, egl_context_)) {
        check_egl_error("eglMakeCurrent for target surface");
        return false;
    }
    
    // Set viewport to surface size
    glViewport(0, 0, surface_width, surface_height);
    
    // Clear the surface
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT);
    
    // Draw the texture as a fullscreen quad
    draw_fullscreen_quad(texture);
    
    // Swap buffers to display
    eglSwapBuffers(egl_display_, target_surface);
    
    // Restore original surface
    eglMakeCurrent(egl_display_, current_surface, current_surface, egl_context_);
    
    return true;
}

void Renderer::draw_fullscreen_quad(GLuint texture) {
    // Simple implementation - create a fullscreen quad and render the texture
    static GLuint program = 0;
    static GLuint vao = 0;
    static GLuint vbo = 0;
    
    // Initialize shader program on first use
    if (program == 0) {
        const char* vertex_shader = R"(
            #version 330 core
            layout (location = 0) in vec2 aPos;
            layout (location = 1) in vec2 aTexCoord;
            
            out vec2 TexCoord;
            
            void main() {
                gl_Position = vec4(aPos, 0.0, 1.0);
                TexCoord = aTexCoord;
            }
        )";
        
        const char* fragment_shader = R"(
            #version 330 core
            out vec4 FragColor;
            
            in vec2 TexCoord;
            uniform sampler2D ourTexture;
            
            void main() {
                FragColor = texture(ourTexture, TexCoord);
            }
        )";
        
        program = create_program(vertex_shader, fragment_shader);
        if (program == 0) {
            log_error("Failed to create wallpaper shader program");
            return;
        }
        
        // Create fullscreen quad
        float vertices[] = {
            // positions   // texture coords
            -1.0f,  1.0f,  0.0f, 1.0f,
            -1.0f, -1.0f,  0.0f, 0.0f,
             1.0f, -1.0f,  1.0f, 0.0f,
             1.0f,  1.0f,  1.0f, 1.0f
        };
        
        unsigned int indices[] = {
            0, 1, 2,
            0, 2, 3
        };
        
        glGenVertexArrays(1, &vao);
        glGenBuffers(1, &vbo);
        GLuint ebo;
        glGenBuffers(1, &ebo);
        
        glBindVertexArray(vao);
        
        glBindBuffer(GL_ARRAY_BUFFER, vbo);
        glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);
        
        glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
        glBufferData(GL_ELEMENT_ARRAY_BUFFER, sizeof(indices), indices, GL_STATIC_DRAW);
        
        // Position attribute
        glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)0);
        glEnableVertexAttribArray(0);
        
        // Texture coord attribute
        glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
        glEnableVertexAttribArray(1);
        
        glBindVertexArray(0);
    }
    
    // Use the shader program
    use_program(program);
    
    // Bind texture
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, texture);
    glUniform1i(glGetUniformLocation(program, "ourTexture"), 0);
    
    // Draw the quad
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, 6, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);
    
    check_gl_error("draw_fullscreen_quad");
}

Renderer::FramebufferInfo Renderer::get_or_create_framebuffer(int width, int height) {
    // Check if we already have a framebuffer for this size
    auto key = std::make_pair(width, height);
    auto it = framebuffer_cache_.find(key);
    
    if (it != framebuffer_cache_.end()) {
        log_debug("Reusing cached framebuffer: " + std::to_string(width) + "x" + std::to_string(height));
        return it->second;
    }
    
    // Create new framebuffer and cache it
    FramebufferInfo info = create_framebuffer(width, height);
    if (info.fbo != 0) {
        framebuffer_cache_[key] = info;
        log_debug("Created and cached new framebuffer: " + std::to_string(width) + "x" + std::to_string(height));
    }
    
    return info;
}

void Renderer::cleanup_framebuffer_cache() {
    log_debug("Cleaning up framebuffer cache (" + std::to_string(framebuffer_cache_.size()) + " entries)");
    
    for (auto& pair : framebuffer_cache_) {
        destroy_framebuffer(pair.second);
    }
    
    framebuffer_cache_.clear();
}
