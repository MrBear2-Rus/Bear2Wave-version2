#ifdef BEAR2WAVE_RENDER_OPENGL

#include "panels/WaveformGLRenderer.h"
#include "core/ui_theme.h"

#pragma comment(lib, "opengl32.lib")

#include <cstdio>
#include <cmath>
#include <cstring>
#include <cstddef>
#include <cstdint>
#include <algorithm>

// ============================================================================
// 嵌入式 GLSL 着色器 (OpenGL 3.3 Core Profile)
// ============================================================================

static const char* kVertexShader = R"GLSL(
#version 330 core

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec4 aColor;

uniform mat4 uProjection;

out vec4 vColor;

void main()
{
    gl_Position = uProjection * vec4(aPosition, 0.0, 1.0);
    vColor = aColor;
}
)GLSL";

static const char* kFragmentShader = R"GLSL(
#version 330 core

in vec4 vColor;
out vec4 FragColor;

void main()
{
    FragColor = vColor;
}
)GLSL";

static const char* kTextVertexShader = R"GLSL(
#version 330 core

layout(location = 0) in vec2 aPosition;
layout(location = 1) in vec2 aTexCoord;

uniform mat4 uProjection;

out vec2 vTexCoord;

void main()
{
    gl_Position = uProjection * vec4(aPosition, 0.0, 1.0);
    vTexCoord = aTexCoord;
}
)GLSL";

static const char* kTextFragmentShader = R"GLSL(
#version 330 core

in vec2 vTexCoord;
out vec4 FragColor;

uniform sampler2D uTex;

void main()
{
    FragColor = texture(uTex, vTexCoord);
}
)GLSL";

static void BuildScreenProjection(int width, int height, float out[16])
{
    const float L = 0.0f;
    const float R = static_cast<float>(width);
    const float T = 0.0f;
    const float B = static_cast<float>(height);
    out[0] = 2.0f / (R - L);   out[4] = 0.0f;                   out[8]  = 0.0f;  out[12] = -(R + L) / (R - L);
    out[1] = 0.0f;             out[5] = -2.0f / (B - T);         out[9]  = 0.0f;  out[13] = (B + T) / (B - T);
    out[2] = 0.0f;             out[6] = 0.0f;                   out[10] = -1.0f; out[14] = 0.0f;
    out[3] = 0.0f;             out[7] = 0.0f;                   out[11] = 0.0f;  out[15] = 1.0f;
}

// ============================================================================
// Shader 编译 / 链接
// ============================================================================

unsigned int WaveformGLRenderer::CompileShader(unsigned int type, const char* source)
{
    unsigned int shader = glCreateShader(type);
    glShaderSource(shader, 1, &source, nullptr);
    glCompileShader(shader);

    int success = 0;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if (!success) {
        char info[512];
        glGetShaderInfoLog(shader, sizeof(info), nullptr, info);
        wxLogError("WaveformGLRenderer: shader compile error:\n%s", info);
        glDeleteShader(shader);
        return 0;
    }
    return shader;
}

unsigned int WaveformGLRenderer::LinkProgram(unsigned int vs, unsigned int fs)
{
    unsigned int program = glCreateProgram();
    glAttachShader(program, vs);
    glAttachShader(program, fs);
    glLinkProgram(program);

    int success = 0;
    glGetProgramiv(program, GL_LINK_STATUS, &success);
    if (!success) {
        char info[512];
        glGetProgramInfoLog(program, sizeof(info), nullptr, info);
        wxLogError("WaveformGLRenderer: program link error:\n%s", info);
        glDeleteProgram(program);
        return 0;
    }
    return program;
}

// ============================================================================
// 生命周期
// ============================================================================

bool WaveformGLRenderer::Initialize()
{
    if (m_initialized)
        return true;

    // 编译 shader
    unsigned int vs = CompileShader(GL_VERTEX_SHADER, kVertexShader);
    if (!vs) return false;
    unsigned int fs = CompileShader(GL_FRAGMENT_SHADER, kFragmentShader);
    if (!fs) { glDeleteShader(vs); return false; }

    m_program = LinkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!m_program) return false;

    m_uniformProj = glGetUniformLocation(m_program, "uProjection");

    // 创建 VAO
    glGenVertexArrays(1, &m_vao);
    glBindVertexArray(m_vao);

    // 创建 VBO（动态绘制，每帧更新）
    glGenBuffers(1, &m_vbo);
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);

    // 顶点属性：aPosition (vec2)
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(GLVertex),
                          reinterpret_cast<void*>(offsetof(GLVertex, x)));
    glEnableVertexAttribArray(0);

    // 顶点属性：aColor (vec4, normalized from uint8)
    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(GLVertex),
                          reinterpret_cast<void*>(offsetof(GLVertex, r)));
    glEnableVertexAttribArray(1);

    // 创建 IBO
    glGenBuffers(1, &m_ibo);

    glBindVertexArray(0);

    // 预分配初始容量（减少运行时分配）
    m_bgBatch.vertices.reserve(2048);
    m_bgBatch.indices.reserve(4096);
    m_lineBatch.vertices.reserve(16384);
    m_lineBatch.indices.reserve(8192);
    m_quadBatch.vertices.reserve(4096);
    m_quadBatch.indices.reserve(4096);

    m_bgBatch.mode = GL_TRIANGLES;
    m_lineBatch.mode = GL_LINES;
    m_quadBatch.mode = GL_TRIANGLES;
    m_minimapBgBatch.mode = GL_TRIANGLES;
    m_minimapLineBatch.mode = GL_LINES;

    if (!InitTextOverlayResources()) {
        wxLogWarning("WaveformGLRenderer: text overlay init failed");
    }

    m_initialized = true;
    wxLogDebug("[WaveformGL] Initialized OpenGL renderer (program=%u, vao=%u)", m_program, m_vao);
    return true;
}

void WaveformGLRenderer::Shutdown()
{
    if (m_textTex) { glDeleteTextures(1, &m_textTex); m_textTex = 0; }
    if (m_textVBO) { glDeleteBuffers(1, &m_textVBO); m_textVBO = 0; }
    if (m_textVAO) { glDeleteVertexArrays(1, &m_textVAO); m_textVAO = 0; }
    if (m_textProgram) { glDeleteProgram(m_textProgram); m_textProgram = 0; }
    if (m_ibo) { glDeleteBuffers(1, &m_ibo); m_ibo = 0; }
    if (m_vbo) { glDeleteBuffers(1, &m_vbo); m_vbo = 0; }
    if (m_vao) { glDeleteVertexArrays(1, &m_vao); m_vao = 0; }
    if (m_program) { glDeleteProgram(m_program); m_program = 0; }
    m_initialized = false;
}

// ============================================================================
// 帧管理
// ============================================================================

void WaveformGLRenderer::BeginFrame(int width, int height)
{
    m_frameWidth = width;
    m_frameHeight = height;

    // 清空所有批次
    m_bgBatch.vertices.clear();
    m_bgBatch.indices.clear();
    m_lineBatch.vertices.clear();
    m_lineBatch.indices.clear();
    m_quadBatch.vertices.clear();
    m_quadBatch.indices.clear();
    m_minimapBgBatch.vertices.clear();
    m_minimapBgBatch.indices.clear();
    m_minimapLineBatch.vertices.clear();
    m_minimapLineBatch.indices.clear();

    // 清屏
    glViewport(0, 0, width, height);
    float clear[4];
    CurrentThemeGlClear(clear);
    glClearColor(clear[0], clear[1], clear[2], clear[3]);
    glClear(GL_COLOR_BUFFER_BIT);

    // 启用混合（用于半透明覆盖层）
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
}

void WaveformGLRenderer::EndFrame()
{
    Flush();
}

bool WaveformGLRenderer::InitTextOverlayResources()
{
    unsigned int vs = CompileShader(GL_VERTEX_SHADER, kTextVertexShader);
    if (!vs) return false;
    unsigned int fs = CompileShader(GL_FRAGMENT_SHADER, kTextFragmentShader);
    if (!fs) { glDeleteShader(vs); return false; }

    m_textProgram = LinkProgram(vs, fs);
    glDeleteShader(vs);
    glDeleteShader(fs);
    if (!m_textProgram) return false;

    m_textUniformProj = glGetUniformLocation(m_textProgram, "uProjection");
    m_textUniformTex = glGetUniformLocation(m_textProgram, "uTex");

    glGenVertexArrays(1, &m_textVAO);
    glGenBuffers(1, &m_textVBO);
    glBindVertexArray(m_textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_textVBO);
    glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(6 * 4 * sizeof(float)),
                 nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(4 * sizeof(float)),
                          reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, static_cast<GLsizei>(4 * sizeof(float)),
                          reinterpret_cast<void*>(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glBindVertexArray(0);

    glGenTextures(1, &m_textTex);
    return true;
}

void WaveformGLRenderer::UploadTextOverlayRgba(int width, int height, const std::uint8_t* rgba)
{
    if (!m_textProgram || !m_textTex || !rgba || width <= 0 || height <= 0)
        return;

    glBindTexture(GL_TEXTURE_2D, m_textTex);
    if (width != m_textPrevW || height != m_textPrevH) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba);
    m_textPrevW = width;
    m_textPrevH = height;
}

void WaveformGLRenderer::UploadTextOverlay(const wxBitmap& bmp)
{
    if (!m_textProgram || !m_textTex)
        return;

    wxImage img = bmp.ConvertToImage();
    if (!img.IsOk())
        return;

    const int w = img.GetWidth();
    const int h = img.GetHeight();
    if (w <= 0 || h <= 0)
        return;

    const unsigned char* rgb = img.GetData();
    if (!rgb)
        return;

    std::vector<unsigned char> rgba(static_cast<size_t>(w) * h * 4);
    for (int y = 0; y < h; ++y) {
        for (int x = 0; x < w; ++x) {
            const int src = (y * w + x) * 3;
            const int dst = (y * w + x) * 4;
            const unsigned char r = rgb[src + 0];
            const unsigned char g = rgb[src + 1];
            const unsigned char b = rgb[src + 2];
            rgba[static_cast<size_t>(dst) + 0] = r;
            rgba[static_cast<size_t>(dst) + 1] = g;
            rgba[static_cast<size_t>(dst) + 2] = b;
            // 白底文字层：仅对近中性白/灰抠透明；有色标签底（如游标值淡黄）保持不透明
            const unsigned char maxc = std::max(r, std::max(g, b));
            const unsigned char minc = std::min(r, std::min(g, b));
            const unsigned chroma = static_cast<unsigned>(maxc) - minc;
            const unsigned avg =
                (static_cast<unsigned>(r) + static_cast<unsigned>(g) + static_cast<unsigned>(b)) / 3u;
            unsigned char a;
            if (chroma <= 10u) {
                a = static_cast<unsigned char>(255u - avg);
            } else if (avg <= 180u) {
                a = static_cast<unsigned char>(255u - avg);
            } else {
                a = 255u;
            }
            rgba[static_cast<size_t>(dst) + 3] = a;
        }
    }

    glBindTexture(GL_TEXTURE_2D, m_textTex);
    if (w != m_textPrevW || h != m_textPrevH) {
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    }
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
    m_textPrevW = w;
    m_textPrevH = h;
}

void WaveformGLRenderer::DrawTextOverlayQuad()
{
    if (!m_textProgram || !m_textTex || m_frameWidth <= 0 || m_frameHeight <= 0)
        return;

    const float W = static_cast<float>(m_frameWidth);
    const float H = static_cast<float>(m_frameHeight);
    // pos.xy, uv — wx 顶行上传后位于 GL 纹理 v=0，与屏幕 y 向下一致，无需再翻 UV
    const float verts[6 * 4] = {
        0.f, 0.f, 0.f, 0.f,
        W,   0.f, 1.f, 0.f,
        W,   H,   1.f, 1.f,
        0.f, 0.f, 0.f, 0.f,
        W,   H,   1.f, 1.f,
        0.f, H,   0.f, 1.f,
    };

    float proj[16];
    BuildScreenProjection(m_frameWidth, m_frameHeight, proj);

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(m_textProgram);
    glUniformMatrix4fv(m_textUniformProj, 1, GL_FALSE, proj);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, m_textTex);
    glUniform1i(m_textUniformTex, 0);

    glBindVertexArray(m_textVAO);
    glBindBuffer(GL_ARRAY_BUFFER, m_textVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0,
                    static_cast<GLsizeiptr>(sizeof(verts)), verts);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);
}

// ============================================================================
// 批次提交
// ============================================================================

void WaveformGLRenderer::Flush()
{
    if (!m_initialized)
        return;

    // 构建正交投影矩阵：屏幕坐标 → NDC
    // X: [0, W] → [-1, 1]
    // Y: [0, H] → [-1, 1]（Y 轴翻转：屏幕 Y 向下，NDC Y 向上）
    float proj[16];
    BuildScreenProjection(m_frameWidth, m_frameHeight, proj);

    glUseProgram(m_program);
    glUniformMatrix4fv(m_uniformProj, 1, GL_FALSE, proj);

    glBindVertexArray(m_vao);

    // 为所有批次上传顶点和索引到同一组 VBO/IBO（使用 glBufferData 整体分配）
    size_t totalVerts = m_bgBatch.vertices.size()
                      + m_lineBatch.vertices.size()
                      + m_quadBatch.vertices.size()
                      + m_minimapBgBatch.vertices.size()
                      + m_minimapLineBatch.vertices.size();
    size_t totalIdx   = m_bgBatch.indices.size()
                      + m_lineBatch.indices.size()
                      + m_quadBatch.indices.size()
                      + m_minimapBgBatch.indices.size()
                      + m_minimapLineBatch.indices.size();

    if (totalVerts == 0 || totalIdx == 0) {
        glBindVertexArray(0);
        return;
    }

    // 上传顶点（3 个批次连续存放）
    glBindBuffer(GL_ARRAY_BUFFER, m_vbo);
    glBufferData(GL_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(totalVerts * sizeof(GLVertex)),
                 nullptr, GL_DYNAMIC_DRAW);

    size_t voffBytes = 0;
    glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(voffBytes),
                    static_cast<GLsizeiptr>(m_bgBatch.vertices.size() * sizeof(GLVertex)),
                    m_bgBatch.vertices.data());
    voffBytes += m_bgBatch.vertices.size() * sizeof(GLVertex);

    glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(voffBytes),
                    static_cast<GLsizeiptr>(m_lineBatch.vertices.size() * sizeof(GLVertex)),
                    m_lineBatch.vertices.data());
    voffBytes += m_lineBatch.vertices.size() * sizeof(GLVertex);

    glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(voffBytes),
                    static_cast<GLsizeiptr>(m_quadBatch.vertices.size() * sizeof(GLVertex)),
                    m_quadBatch.vertices.data());
    voffBytes += m_quadBatch.vertices.size() * sizeof(GLVertex);

    glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(voffBytes),
                    static_cast<GLsizeiptr>(m_minimapBgBatch.vertices.size() * sizeof(GLVertex)),
                    m_minimapBgBatch.vertices.data());
    voffBytes += m_minimapBgBatch.vertices.size() * sizeof(GLVertex);

    glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(voffBytes),
                    static_cast<GLsizeiptr>(m_minimapLineBatch.vertices.size() * sizeof(GLVertex)),
                    m_minimapLineBatch.vertices.data());

    // 上传索引并绘制（每个批次需要按累积 baseVertex 调整索引）
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, m_ibo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER,
                 static_cast<GLsizeiptr>(totalIdx * sizeof(unsigned int)),
                 nullptr, GL_DYNAMIC_DRAW);

    unsigned int baseVertex = 0;
    size_t ioffBytes = 0;

    // 辅助 lambda：调整索引并上传，然后绘制
    auto drawBatch = [&](const GLBatch& batch, GLenum mode) {
        if (batch.indices.empty()) return;
        std::vector<unsigned int> adj;
        adj.reserve(batch.indices.size());
        for (unsigned int idx : batch.indices)
            adj.push_back(idx + baseVertex);
        glBufferSubData(GL_ELEMENT_ARRAY_BUFFER,
                        static_cast<GLintptr>(ioffBytes),
                        static_cast<GLsizeiptr>(adj.size() * sizeof(unsigned int)),
                        adj.data());
        glDrawElements(mode,
                       static_cast<GLsizei>(adj.size()),
                       GL_UNSIGNED_INT,
                       reinterpret_cast<void*>(static_cast<intptr_t>(ioffBytes)));
        baseVertex += static_cast<unsigned int>(batch.vertices.size());
        ioffBytes += adj.size() * sizeof(unsigned int);
    };

    drawBatch(m_bgBatch, GL_TRIANGLES);
    drawBatch(m_lineBatch, GL_LINES);
    drawBatch(m_quadBatch, GL_TRIANGLES);
    drawBatch(m_minimapBgBatch, GL_TRIANGLES);
    drawBatch(m_minimapLineBatch, GL_LINES);

    glBindVertexArray(0);
}

// ============================================================================
// 内部辅助
// ============================================================================

static void PushQuadToBatch(GLBatch& batch,
    float x, float y, float w, float h,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a);

static void PushWireQuadToBatch(GLBatch& batch,
    float x, float y, float w, float h,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a);

void WaveformGLRenderer::PushBackgroundQuad(
    float x, float y, float w, float h,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    GLBatch& batch = m_bgBatch;
    unsigned int base = static_cast<unsigned int>(batch.vertices.size());

    // 4 个顶点（左上、右上、右下、左下）
    GLVertex v0 = { x,     y,     r, g, b, a };
    GLVertex v1 = { x + w, y,     r, g, b, a };
    GLVertex v2 = { x + w, y + h, r, g, b, a };
    GLVertex v3 = { x,     y + h, r, g, b, a };
    batch.vertices.insert(batch.vertices.end(), { v0, v1, v2, v3 });

    // 2 个三角形（CCW 序，但 2D 无所谓）
    batch.indices.insert(batch.indices.end(), {
        base, base + 1, base + 2,
        base, base + 2, base + 3
    });
}

void WaveformGLRenderer::PushLine(
    float x1, float y1, float x2, float y2,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    GLBatch& batch = m_lineBatch;
    unsigned int base = static_cast<unsigned int>(batch.vertices.size());

    GLVertex v0 = { x1, y1, r, g, b, a };
    GLVertex v1 = { x2, y2, r, g, b, a };
    batch.vertices.push_back(v0);
    batch.vertices.push_back(v1);

    batch.indices.push_back(base);
    batch.indices.push_back(base + 1);
}

// ============================================================================
// Public API — 背景
// ============================================================================

void WaveformGLRenderer::AddRowStripe(int x, int y, int w, int h,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    PushBackgroundQuad(static_cast<float>(x), static_cast<float>(y),
                       static_cast<float>(w), static_cast<float>(h), r, g, b, a);
}

void WaveformGLRenderer::AddAxisBand(int x, int y, int w, int h,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    PushBackgroundQuad(static_cast<float>(x), static_cast<float>(y),
                       static_cast<float>(w), static_cast<float>(h), r, g, b, a);
}

void WaveformGLRenderer::AddRangeFill(int x, int y, int w, int h,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    PushBackgroundQuad(static_cast<float>(x), static_cast<float>(y),
                       static_cast<float>(w), static_cast<float>(h), r, g, b, a);
}

void WaveformGLRenderer::AddCommentRowBg(int x, int y, int w, int h,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    PushBackgroundQuad(static_cast<float>(x), static_cast<float>(y),
                       static_cast<float>(w), static_cast<float>(h), r, g, b, a);
}

void WaveformGLRenderer::AddSelectedHighlight(int x, int y, int w, int h,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    PushBackgroundQuad(static_cast<float>(x), static_cast<float>(y),
                       static_cast<float>(w), static_cast<float>(h), r, g, b, a);
}

void WaveformGLRenderer::AddBlackoutRect(int x, int y, int w, int h,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    PushBackgroundQuad(static_cast<float>(x), static_cast<float>(y),
                       static_cast<float>(w), static_cast<float>(h), r, g, b, a);
}

void WaveformGLRenderer::AddSelectionOverlay(int x, int y, int w, int h,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    PushQuadToBatch(m_quadBatch,
                    static_cast<float>(x), static_cast<float>(y),
                    static_cast<float>(w), static_cast<float>(h),
                    r, g, b, a);
}

// ============================================================================
// Public API — 线条（Digital, Analog, Grid, Markers, Playhead）
// ============================================================================

void WaveformGLRenderer::AddDigitalLine(int x1, int y, int x2, int y2,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    PushLine(static_cast<float>(x1), static_cast<float>(y),
             static_cast<float>(x2), static_cast<float>(y2), r, g, b, a);
}

void WaveformGLRenderer::AddDigitalTransition(int x, int y1, int y2,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    PushLine(static_cast<float>(x), static_cast<float>(y1),
             static_cast<float>(x), static_cast<float>(y2), r, g, b, a);
}

void WaveformGLRenderer::AddAnalogLine(int x1, int y, int x2, int y2,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    PushLine(static_cast<float>(x1), static_cast<float>(y),
             static_cast<float>(x2), static_cast<float>(y2), r, g, b, a);
}

void WaveformGLRenderer::AddGridLine(int x, int y0, int y1,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    PushLine(static_cast<float>(x), static_cast<float>(y0),
             static_cast<float>(x), static_cast<float>(y1), r, g, b, a);
}

void WaveformGLRenderer::AddMarkerLine(int x, int y0, int y1,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    PushLine(static_cast<float>(x), static_cast<float>(y0),
             static_cast<float>(x), static_cast<float>(y1), r, g, b, a);
}

void WaveformGLRenderer::AddPlayheadLine(int x, int y0, int y1,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    // Playhead 用虚线效果：每隔 8 像素画一段实线
    const int dashLen = 8;
    const int gapLen = 6;
    int yy = y0;
    bool dash = true;
    while (yy < y1) {
        int segEnd = dash ? std::min(yy + dashLen, y1) : yy;
        if (dash) {
            PushLine(static_cast<float>(x), static_cast<float>(yy),
                     static_cast<float>(x), static_cast<float>(segEnd), r, g, b, a);
        }
        yy += dash ? dashLen : gapLen;
        dash = !dash;
    }
}

void WaveformGLRenderer::AddMinimapSignalLine(int x1, int y, int x2,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    GLBatch& batch = m_minimapLineBatch;
    unsigned int base = static_cast<unsigned int>(batch.vertices.size());
    GLVertex v0 = { static_cast<float>(x1), static_cast<float>(y), r, g, b, a };
    GLVertex v1 = { static_cast<float>(x2), static_cast<float>(y), r, g, b, a };
    batch.vertices.push_back(v0);
    batch.vertices.push_back(v1);
    batch.indices.push_back(base);
    batch.indices.push_back(base + 1);
}

// ============================================================================
// Public API — Quad（Bus 矩形、Label 背景、Minimap、ScrollBar）
// ============================================================================

static void PushQuadToBatch(GLBatch& batch,
    float x, float y, float w, float h,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    unsigned int base = static_cast<unsigned int>(batch.vertices.size());

    GLVertex v0 = { x,     y,     r, g, b, a };
    GLVertex v1 = { x + w, y,     r, g, b, a };
    GLVertex v2 = { x + w, y + h, r, g, b, a };
    GLVertex v3 = { x,     y + h, r, g, b, a };
    batch.vertices.insert(batch.vertices.end(), { v0, v1, v2, v3 });

    batch.indices.insert(batch.indices.end(), {
        base, base + 1, base + 2,
        base, base + 2, base + 3
    });
}

static void PushWireQuadToBatch(GLBatch& batch,
    float x, float y, float w, float h,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    // 四条边用线条表示（线框矩形）
    unsigned int bv = static_cast<unsigned int>(batch.vertices.size());

    GLVertex v0 = { x,     y,     r, g, b, a };
    GLVertex v1 = { x + w, y,     r, g, b, a };
    GLVertex v2 = { x + w, y + h, r, g, b, a };
    GLVertex v3 = { x,     y + h, r, g, b, a };
    batch.vertices.insert(batch.vertices.end(), { v0, v1, v2, v3 });

    // 四条边
    batch.indices.insert(batch.indices.end(), { bv, bv + 1, bv + 1, bv + 2, bv + 2, bv + 3, bv + 3, bv });
}

void WaveformGLRenderer::AddBusRect(int x, int y, int w, int h,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    PushQuadToBatch(m_quadBatch,
                    static_cast<float>(x), static_cast<float>(y),
                    static_cast<float>(std::max(1, w)),
                    static_cast<float>(std::max(1, h)),
                    r, g, b, a);
}

void WaveformGLRenderer::AddCursorLabelBg(int x, int y, int w, int h,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    PushQuadToBatch(m_quadBatch,
                    static_cast<float>(x), static_cast<float>(y),
                    static_cast<float>(w), static_cast<float>(h),
                    r, g, b, a);
}

void WaveformGLRenderer::AddMarkerLabelBg(int x, int y, int w, int h,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    PushQuadToBatch(m_quadBatch,
                    static_cast<float>(x), static_cast<float>(y),
                    static_cast<float>(w), static_cast<float>(h),
                    r, g, b, a);
}

void WaveformGLRenderer::AddMeasureBarBg(int x, int y, int w, int h,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    PushQuadToBatch(m_quadBatch,
                    static_cast<float>(x), static_cast<float>(y),
                    static_cast<float>(w), static_cast<float>(h),
                    r, g, b, a);
}

void WaveformGLRenderer::AddMinimapBg(int x, int y, int w, int h,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    PushQuadToBatch(m_minimapBgBatch,
                    static_cast<float>(x), static_cast<float>(y),
                    static_cast<float>(w), static_cast<float>(h),
                    r, g, b, a);
}

void WaveformGLRenderer::AddMinimapViewRect(int x, int y, int w, int h,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    PushWireQuadToBatch(m_minimapLineBatch,
                        static_cast<float>(x), static_cast<float>(y),
                        static_cast<float>(std::max(2, w)),
                        static_cast<float>(std::max(2, h)),
                        r, g, b, a);
}

void WaveformGLRenderer::AddMinimapBorder(int x, int y, int w, int h,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    PushWireQuadToBatch(m_minimapLineBatch,
                        static_cast<float>(x), static_cast<float>(y),
                        static_cast<float>(std::max(2, w)),
                        static_cast<float>(std::max(2, h)),
                        r, g, b, a);
}

void WaveformGLRenderer::AddScrollBarTrack(int x, int y, int w, int h,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    PushQuadToBatch(m_quadBatch,
                    static_cast<float>(x), static_cast<float>(y),
                    static_cast<float>(w), static_cast<float>(h),
                    r, g, b, a);
}

void WaveformGLRenderer::AddScrollBarThumb(int x, int y, int w, int h,
    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a)
{
    PushQuadToBatch(m_quadBatch,
                    static_cast<float>(x), static_cast<float>(y),
                    static_cast<float>(w), static_cast<float>(h),
                    r, g, b, a);
}

#endif // BEAR2WAVE_RENDER_OPENGL
