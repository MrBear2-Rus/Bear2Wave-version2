#include "panels/gl_load.h"

#include <windows.h>  // wglGetProcAddress

// ----- 函数指针定义 -----

#define GLFUNC(ret, name, params) PFN_##name##_T name = nullptr
GLFUNC(GLuint, glCreateShader, (GLenum type));
GLFUNC(void,   glShaderSource,  (GLuint shader, GLsizei count, const GLchar** string, const GLint* length));
GLFUNC(void,   glCompileShader, (GLuint shader));
GLFUNC(void,   glGetShaderiv,   (GLuint shader, GLenum pname, GLint* params));
GLFUNC(void,   glGetShaderInfoLog, (GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog));
GLFUNC(void,   glDeleteShader,  (GLuint shader));
GLFUNC(GLuint, glCreateProgram,  (void));
GLFUNC(void,   glAttachShader,   (GLuint program, GLuint shader));
GLFUNC(void,   glLinkProgram,    (GLuint program));
GLFUNC(void,   glGetProgramiv,   (GLuint program, GLenum pname, GLint* params));
GLFUNC(void,   glGetProgramInfoLog, (GLuint program, GLsizei bufSize, GLsizei* length, GLchar* infoLog));
GLFUNC(void,   glDeleteProgram,  (GLuint program));
GLFUNC(void,   glUseProgram,     (GLuint program));
GLFUNC(GLint,  glGetUniformLocation, (GLuint program, const GLchar* name));
GLFUNC(void,   glUniformMatrix4fv, (GLint location, GLsizei count, GLboolean transpose, const GLfloat* value));
GLFUNC(void,   glUniform1i,        (GLint location, GLint v));
GLFUNC(void,   glActiveTexture,    (GLenum texture));
GLFUNC(void,   glGenVertexArrays,    (GLsizei n, GLuint* arrays));
GLFUNC(void,   glBindVertexArray,    (GLuint array));
GLFUNC(void,   glDeleteVertexArrays, (GLsizei n, const GLuint* arrays));
GLFUNC(void,   glGenBuffers,         (GLsizei n, GLuint* buffers));
GLFUNC(void,   glBindBuffer,         (GLenum target, GLuint buffer));
GLFUNC(void,   glBufferData,         (GLenum target, GLsizeiptr size, const void* data, GLenum usage));
GLFUNC(void,   glBufferSubData,      (GLenum target, GLintptr offset, GLsizeiptr size, const void* data));
GLFUNC(void,   glDeleteBuffers,      (GLsizei n, const GLuint* buffers));
GLFUNC(void,   glVertexAttribPointer,  (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer));
GLFUNC(void,   glEnableVertexAttribArray, (GLuint index));
// glDrawElements is GL 1.1 — available directly, no wglGetProcAddress needed
#undef GLFUNC

// ----- 加载函数 -----

namespace {

template<typename T>
bool load_gl_func(T& fn, const char* name, const char* alt = nullptr)
{
    fn = reinterpret_cast<T>(wglGetProcAddress(name));
    if (!fn && alt)
        fn = reinterpret_cast<T>(wglGetProcAddress(alt));
    return fn != nullptr;
}

} // namespace

bool gl_load_functions()
{
    if (!load_gl_func(glCreateShader, "glCreateShader")) return false;
    if (!load_gl_func(glShaderSource, "glShaderSource")) return false;
    if (!load_gl_func(glCompileShader, "glCompileShader")) return false;
    if (!load_gl_func(glGetShaderiv, "glGetShaderiv")) return false;
    if (!load_gl_func(glGetShaderInfoLog, "glGetShaderInfoLog")) return false;
    if (!load_gl_func(glDeleteShader, "glDeleteShader")) return false;
    if (!load_gl_func(glCreateProgram, "glCreateProgram")) return false;
    if (!load_gl_func(glAttachShader, "glAttachShader")) return false;
    if (!load_gl_func(glLinkProgram, "glLinkProgram")) return false;
    if (!load_gl_func(glGetProgramiv, "glGetProgramiv")) return false;
    if (!load_gl_func(glGetProgramInfoLog, "glGetProgramInfoLog")) return false;
    if (!load_gl_func(glDeleteProgram, "glDeleteProgram")) return false;
    if (!load_gl_func(glUseProgram, "glUseProgram")) return false;
    if (!load_gl_func(glGetUniformLocation, "glGetUniformLocation")) return false;
    if (!load_gl_func(glUniformMatrix4fv, "glUniformMatrix4fv")) return false;
    if (!load_gl_func(glUniform1i, "glUniform1i")) return false;
    if (!load_gl_func(glActiveTexture, "glActiveTexture")) return false;
    if (!load_gl_func(glGenVertexArrays, "glGenVertexArrays", "glGenVertexArraysARB")) return false;
    if (!load_gl_func(glBindVertexArray, "glBindVertexArray", "glBindVertexArrayARB")) return false;
    if (!load_gl_func(glDeleteVertexArrays, "glDeleteVertexArrays", "glDeleteVertexArraysARB")) return false;
    if (!load_gl_func(glGenBuffers, "glGenBuffers", "glGenBuffersARB")) return false;
    if (!load_gl_func(glBindBuffer, "glBindBuffer", "glBindBufferARB")) return false;
    if (!load_gl_func(glBufferData, "glBufferData", "glBufferDataARB")) return false;
    if (!load_gl_func(glBufferSubData, "glBufferSubData", "glBufferSubDataARB")) return false;
    if (!load_gl_func(glDeleteBuffers, "glDeleteBuffers", "glDeleteBuffersARB")) return false;
    if (!load_gl_func(glVertexAttribPointer, "glVertexAttribPointer", "glVertexAttribPointerARB")) return false;
    if (!load_gl_func(glEnableVertexAttribArray, "glEnableVertexAttribArray", "glEnableVertexAttribArrayARB")) return false;
    return true;
}
