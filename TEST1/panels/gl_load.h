#pragma once
// ============================================================================
// gl_load.h — 零依赖 OpenGL 3.3 函数加载器
//
// 用法：
//   1. 创建 GL context 并 SetCurrent
//   2. 调用 gl_load_functions()，失败返回 false
//   3. 之后所有 GL 3.3 函数指针即可正常使用
//
// 不需要 GLEW / GLAD / vcpkg — 仅依赖系统 opengl32.dll + wglGetProcAddress
// ============================================================================

#include <windows.h>   // must precede gl/GL.h (provides APIENTRY, WINGDIAPI)
#include <gl/GL.h>
#include <cstddef>

// ----- Windows gl/GL.h 仅覆盖 GL 1.1：补全类型与常量 -----

#ifndef GLchar
typedef char GLchar;
#endif
#ifndef GLboolean
typedef unsigned char GLboolean;
#endif
#ifndef GLsizeiptr
typedef ptrdiff_t GLsizeiptr;
#endif
#ifndef GLintptr
typedef ptrdiff_t GLintptr;
#endif

// ----- OpenGL 3.3 常量（Windows gl/GL.h 仅覆盖 1.1）-----

#ifndef GL_VERTEX_SHADER
#define GL_VERTEX_SHADER         0x8B31
#endif
#ifndef GL_FRAGMENT_SHADER
#define GL_FRAGMENT_SHADER       0x8B30
#endif
#ifndef GL_COMPILE_STATUS
#define GL_COMPILE_STATUS        0x8B81
#endif
#ifndef GL_LINK_STATUS
#define GL_LINK_STATUS           0x8B82
#endif
#ifndef GL_INFO_LOG_LENGTH
#define GL_INFO_LOG_LENGTH       0x8B84
#endif
#ifndef GL_ARRAY_BUFFER
#define GL_ARRAY_BUFFER          0x8892
#endif
#ifndef GL_ELEMENT_ARRAY_BUFFER
#define GL_ELEMENT_ARRAY_BUFFER  0x8893
#endif
#ifndef GL_DYNAMIC_DRAW
#define GL_DYNAMIC_DRAW          0x88E8
#endif
#ifndef GL_FRAGMENT_SHADER_DERIVATIVE_HINT
#define GL_FRAGMENT_SHADER_DERIVATIVE_HINT 0x8B8B
#endif
#ifndef GL_TEXTURE0
#define GL_TEXTURE0              0x84C0
#endif
#ifndef GL_CLAMP_TO_EDGE
#define GL_CLAMP_TO_EDGE         0x812F
#endif

// glGenTextures / glBindTexture / glTexImage2D / glTexParameteri — GL 1.1, opengl32.dll 直接导出

// ----- 函数指针声明 -----

#define GLFUNC(ret, name, params) \
    using PFN_##name##_T = ret (__stdcall *)params; \
    extern PFN_##name##_T name

// Shader
GLFUNC(GLuint, glCreateShader, (GLenum type));
GLFUNC(void,   glShaderSource,  (GLuint shader, GLsizei count, const GLchar** string, const GLint* length));
GLFUNC(void,   glCompileShader, (GLuint shader));
GLFUNC(void,   glGetShaderiv,   (GLuint shader, GLenum pname, GLint* params));
GLFUNC(void,   glGetShaderInfoLog, (GLuint shader, GLsizei bufSize, GLsizei* length, GLchar* infoLog));
GLFUNC(void,   glDeleteShader,  (GLuint shader));

// Program
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

// VAO / VBO
GLFUNC(void,   glGenVertexArrays,    (GLsizei n, GLuint* arrays));
GLFUNC(void,   glBindVertexArray,    (GLuint array));
GLFUNC(void,   glDeleteVertexArrays, (GLsizei n, const GLuint* arrays));
GLFUNC(void,   glGenBuffers,         (GLsizei n, GLuint* buffers));
GLFUNC(void,   glBindBuffer,         (GLenum target, GLuint buffer));
GLFUNC(void,   glBufferData,         (GLenum target, GLsizeiptr size, const void* data, GLenum usage));
GLFUNC(void,   glBufferSubData,      (GLenum target, GLintptr offset, GLsizeiptr size, const void* data));
GLFUNC(void,   glDeleteBuffers,      (GLsizei n, const GLuint* buffers));

// Vertex attrib
GLFUNC(void,   glVertexAttribPointer,  (GLuint index, GLint size, GLenum type, GLboolean normalized, GLsizei stride, const void* pointer));
GLFUNC(void,   glEnableVertexAttribArray, (GLuint index));

// glDrawElements is GL 1.1 — already declared by <gl/GL.h>, no need to load

#undef GLFUNC

// ----- 加载所有函数指针（需要在有效 GL context 绑定后调用）-----
// 返回 true 表示全部加载成功
bool gl_load_functions();
