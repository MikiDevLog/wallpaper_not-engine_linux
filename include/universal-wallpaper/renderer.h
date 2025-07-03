#pragma once

#include <memory>
#include <EGL/egl.h>
#include <GL/gl.h>
#include <GL/glext.h>
#include <wayland-egl.h>
#include <map>
#include <utility>

// OpenGL extension function declarations
#ifndef GL_VERSION_3_0
typedef void (APIENTRY *PFNGLGENFRAMEBUFFERSPROC)(GLsizei n, GLuint *framebuffers);
typedef void (APIENTRY *PFNGLBINDFRAMEBUFFERPROC)(GLenum target, GLuint framebuffer);
typedef void (APIENTRY *PFNGLFRAMEBUFFERTEXTURE2DPROC)(GLenum target, GLenum attachment, GLenum textarget, GLuint texture, GLint level);
typedef GLenum (APIENTRY *PFNGLCHECKFRAMEBUFFERSTATUSPROC)(GLenum target);
typedef void (APIENTRY *PFNGLDELETEFRAMEBUFFERSPROC)(GLsizei n, const GLuint *framebuffers);
typedef GLuint (APIENTRY *PFNGLCREATESHADERPROC)(GLenum type);
typedef void (APIENTRY *PFNGLSHADERSOURCEPROC)(GLuint shader, GLsizei count, const GLchar *const*string, const GLint *length);
typedef void (APIENTRY *PFNGLCOMPILESHADERPROC)(GLuint shader);
typedef void (APIENTRY *PFNGLGETSHADERIVPROC)(GLuint shader, GLenum pname, GLint *params);
typedef void (APIENTRY *PFNGLGETSHADERINFOLOGPROC)(GLuint shader, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (APIENTRY *PFNGLDELETESHADERPROC)(GLuint shader);
typedef GLuint (APIENTRY *PFNGLCREATEPROGRAMPROC)(void);
typedef void (APIENTRY *PFNGLATTACHSHADERPROC)(GLuint program, GLuint shader);
typedef void (APIENTRY *PFNGLLINKPROGRAMPROC)(GLuint program);
typedef void (APIENTRY *PFNGLGETPROGRAMIVPROC)(GLuint program, GLenum pname, GLint *params);
typedef void (APIENTRY *PFNGLGETPROGRAMINFOLOGPROC)(GLuint program, GLsizei bufSize, GLsizei *length, GLchar *infoLog);
typedef void (APIENTRY *PFNGLUSEPROGRAMPROC)(GLuint program);
typedef void (APIENTRY *PFNGLDELETEPROGRAMPROC)(GLuint program);

// Additional OpenGL function typedefs for VAO and VBO
typedef void (APIENTRY *PFNGLGENVERTEXARRAYSPROC)(GLsizei n, GLuint *arrays);
typedef void (APIENTRY *PFNGLBINDVERTEXARRAYPROC)(GLuint array);
typedef void (APIENTRY *PFNGLGENBUFFERSPROC)(GLsizei n, GLuint *buffers);
typedef void (APIENTRY *PFNGLBINDBUFFERPROC)(GLenum target, GLuint buffer);
typedef void (APIENTRY *PFNGLBUFFERDATAPROC)(GLenum target, GLsizeiptr size, const void *data, GLenum usage);
typedef void (APIENTRY *PFNGLVERTEXATTRIBPOINTERPROC)(GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void *pointer);
typedef void (APIENTRY *PFNGLENABLEVERTEXATTRIBARRAYPROC)(GLuint index);
typedef GLint (APIENTRY *PFNGLGETUNIFORMLOCATIONPROC)(GLuint program, const GLchar *name);
typedef void (APIENTRY *PFNGLUNIFORM1IPROC)(GLint location, GLint value);
#endif

class Renderer {
public:
    struct FramebufferInfo {
        GLuint fbo;
        GLuint texture;
        int width;
        int height;
    };
    
    Renderer();
    ~Renderer();
    
    bool initialize();
    void destroy();
    
    // OpenGL context management
    bool create_context(void* native_display = nullptr);
    void destroy_context();
    bool make_current();
    void swap_buffers();
    
    // Framebuffer management
    FramebufferInfo create_framebuffer(int width, int height);
    void destroy_framebuffer(const FramebufferInfo& info);
    void bind_framebuffer(const FramebufferInfo& info);
    void bind_default_framebuffer();
    
    // Cached framebuffer management for better performance
    FramebufferInfo get_or_create_framebuffer(int width, int height);
    void cleanup_framebuffer_cache();
    
    // Rendering utilities
    void clear(float r = 0.0f, float g = 0.0f, float b = 0.0f, float a = 1.0f);
    void set_viewport(int x, int y, int width, int height);
    
    // Texture utilities
    GLuint create_texture(int width, int height, const void* data = nullptr);
    void update_texture(GLuint texture, int width, int height, const void* data);
    void destroy_texture(GLuint texture);
    
    // Shader utilities
    GLuint compile_shader(GLenum type, const char* source);
    GLuint create_program(const char* vertex_source, const char* fragment_source);
    void use_program(GLuint program);
    void destroy_program(GLuint program);
    
    // OpenGL function pointer getter for MPV
    static void* get_proc_address(void* ctx, const char* name);
    
    // Methods for rendering to Wayland surfaces
    EGLSurface create_egl_surface_for_wayland(wl_egl_window* egl_window);
    bool render_texture_to_surface(EGLSurface target_surface, GLuint texture, int surface_width, int surface_height);
    void draw_fullscreen_quad(GLuint texture);
    
    EGLDisplay get_egl_display() const { return egl_display_; }
    EGLContext get_egl_context() const { return egl_context_; }

private:
    EGLDisplay egl_display_ = EGL_NO_DISPLAY;
    EGLContext egl_context_ = EGL_NO_CONTEXT;
    EGLSurface egl_surface_ = EGL_NO_SURFACE;
    EGLConfig egl_config_;
    
    // OpenGL extension function pointers
    PFNGLGENFRAMEBUFFERSPROC glGenFramebuffers = nullptr;
    PFNGLBINDFRAMEBUFFERPROC glBindFramebuffer = nullptr;
    PFNGLFRAMEBUFFERTEXTURE2DPROC glFramebufferTexture2D = nullptr;
    PFNGLCHECKFRAMEBUFFERSTATUSPROC glCheckFramebufferStatus = nullptr;
    PFNGLDELETEFRAMEBUFFERSPROC glDeleteFramebuffers = nullptr;
    PFNGLCREATESHADERPROC glCreateShader = nullptr;
    PFNGLSHADERSOURCEPROC glShaderSource = nullptr;
    PFNGLCOMPILESHADERPROC glCompileShader = nullptr;
    PFNGLGETSHADERIVPROC glGetShaderiv = nullptr;
    PFNGLGETSHADERINFOLOGPROC glGetShaderInfoLog = nullptr;
    PFNGLDELETESHADERPROC glDeleteShader = nullptr;
    PFNGLCREATEPROGRAMPROC glCreateProgram = nullptr;
    PFNGLATTACHSHADERPROC glAttachShader = nullptr;
    PFNGLLINKPROGRAMPROC glLinkProgram = nullptr;
    PFNGLGETPROGRAMIVPROC glGetProgramiv = nullptr;
    PFNGLGETPROGRAMINFOLOGPROC glGetProgramInfoLog = nullptr;
    PFNGLUSEPROGRAMPROC glUseProgram = nullptr;
    PFNGLDELETEPROGRAMPROC glDeleteProgram = nullptr;
    
    // Additional OpenGL function pointers for rendering
    PFNGLGENVERTEXARRAYSPROC glGenVertexArrays = nullptr;
    PFNGLBINDVERTEXARRAYPROC glBindVertexArray = nullptr;
    PFNGLGENBUFFERSPROC glGenBuffers = nullptr;
    PFNGLBINDBUFFERPROC glBindBuffer = nullptr;
    PFNGLBUFFERDATAPROC glBufferData = nullptr;
    PFNGLVERTEXATTRIBPOINTERPROC glVertexAttribPointer = nullptr;
    PFNGLENABLEVERTEXATTRIBARRAYPROC glEnableVertexAttribArray = nullptr;
    PFNGLGETUNIFORMLOCATIONPROC glGetUniformLocation = nullptr;
    PFNGLUNIFORM1IPROC glUniform1i = nullptr;
    
    // Framebuffer cache to avoid recreating framebuffers every frame
    std::map<std::pair<int, int>, FramebufferInfo> framebuffer_cache_;
    
    bool setup_egl(void* native_display);
    bool load_gl_extensions();
    void check_gl_error(const char* operation);
};
