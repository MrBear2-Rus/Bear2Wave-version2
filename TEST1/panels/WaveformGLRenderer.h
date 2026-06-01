#pragma once

#ifdef BEAR2WAVE_RENDER_OPENGL

#include <wx/wx.h>
#include <wx/glcanvas.h>
#include "panels/gl_load.h"

#include <vector>
#include <cstdint>

class WaveformPanel;

// ============================================================================
// 统一 GPU 顶点格式：12 字节，对齐友好
// ============================================================================
struct GLVertex {
    float x, y;           // 屏幕坐标（左上角为原点，Y 轴向下）
    std::uint8_t r, g, b, a; // 预解析 RGBA 颜色
};

// ============================================================================
// 批次：一组顶点 + 索引，按同一种 GL 图元模式提交
// ============================================================================
struct GLBatch {
    std::vector<GLVertex> vertices;
    std::vector<unsigned int> indices;
    GLenum mode = GL_LINES; // GL_LINES, GL_TRIANGLES, GL_LINE_STRIP
};

// ============================================================================
// WaveformGLRenderer — 管理 OpenGL 状态、Shader、VBO/VAO/IBO 和批次提交
// ============================================================================
class WaveformGLRenderer {
public:
    WaveformGLRenderer() = default;

    // 初始化：创建 shader program + VBO/VAO/IBO（需要当前 GL context）
    bool Initialize();

    // 释放所有 GL 资源
    void Shutdown();

    bool IsInitialized() const { return m_initialized; }

    // ----- 帧生命周期 -----
    void BeginFrame(int width, int height);
    void EndFrame();

    // ----- 文字纹理叠加（解决 wxDC+SwapBuffers 闪烁问题）-----
    // 将文字层 RGBA 上传为 GL 纹理（DirectWrite 直传 alpha）
    void UploadTextOverlayRgba(int width, int height, const std::uint8_t* rgba);
    // 将 wxBitmap 文字层上传为 GL 纹理，对白底做抠图
    void UploadTextOverlay(const class wxBitmap& bmp);
    void DrawTextOverlayQuad();
    int  FrameWidth()  const { return m_frameWidth; }
    int  FrameHeight() const { return m_frameHeight; }

    // ----- 批次构建（按绘制顺序添加，最后统一 Flush）-----

    // 背景：行条纹矩形
    void AddRowStripe(int x, int y, int w, int h,
                      std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);

    // 背景：时间轴 band
    void AddAxisBand(int x, int y, int w, int h,
                     std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);

    // 背景：Range-fill 矩形
    void AddRangeFill(int x, int y, int w, int h,
                      std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);

    // 背景：Comment 行背景
    void AddCommentRowBg(int x, int y, int w, int h,
                         std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);

    // 背景：选中信号高亮
    void AddSelectedHighlight(int x, int y, int w, int h,
                              std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);

    // Digital：一条水平线段
    void AddDigitalLine(int x1, int y, int x2, int y2,
                        std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);

    // Digital：一条垂直线段（跳变）
    void AddDigitalTransition(int x, int y1, int y2,
                              std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);

    // Bus/Text/Transaction：一个填充矩形
    void AddBusRect(int x, int y, int w, int h,
                    std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);

    // Analog：一条水平线段（模拟连线）
    void AddAnalogLine(int x1, int y, int x2, int y2,
                       std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);

    // Grid：一条竖线（从 y0 到 y1）
    void AddGridLine(int x, int y0, int y1,
                     std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);

    // Blackout：半透明矩形
    void AddBlackoutRect(int x, int y, int w, int h,
                         std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 128);

    // Marker：垂直线
    void AddMarkerLine(int x, int y0, int y1,
                       std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);

    // Playhead：虚线（用跳变模拟）
    void AddPlayheadLine(int x, int y0, int y1,
                         std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);

    // Cursor value label 背景
    void AddCursorLabelBg(int x, int y, int w, int h,
                          std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);

    // Marker label 背景
    void AddMarkerLabelBg(int x, int y, int w, int h,
                          std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);

    // Measurement bar 背景
    void AddMeasureBarBg(int x, int y, int w, int h,
                         std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);

    // Minimap 背景
    void AddMinimapBg(int x, int y, int w, int h,
                      std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);

    // Minimap 信号线
    void AddMinimapSignalLine(int x1, int y, int x2,
                              std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);

    // Minimap 视窗框
    void AddMinimapViewRect(int x, int y, int w, int h,
                            std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);

    // Minimap 外边框（绘制在 minimap 背景之上）
    void AddMinimapBorder(int x, int y, int w, int h,
                          std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);

    // 滚动条 track
    void AddScrollBarTrack(int x, int y, int w, int h,
                           std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);

    // 滚动条 thumb
    void AddScrollBarThumb(int x, int y, int w, int h,
                           std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 255);

    // Selection overlay
    void AddSelectionOverlay(int x, int y, int w, int h,
                             std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a = 60);

    // ----- 提交所有批次到 GPU -----
    void Flush();

private:
    // 辅助：往当前背景批次添加一个矩形（2 三角形 = 6 顶点索引）
    void PushBackgroundQuad(float x, float y, float w, float h,
                            std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a);

    // 辅助：往当前线条批次添加一条线段
    void PushLine(float x1, float y1, float x2, float y2,
                  std::uint8_t r, std::uint8_t g, std::uint8_t b, std::uint8_t a);

    // 编译单个 shader
    static unsigned int CompileShader(unsigned int type, const char* source);

    // 链接 program
    static unsigned int LinkProgram(unsigned int vs, unsigned int fs);

    GLuint m_vao = 0;
    GLuint m_vbo = 0;
    GLuint m_ibo = 0;
    GLuint m_program = 0;
    int m_uniformProj = -1;

    // 文字纹理叠加
    GLuint m_textProgram = 0;
    GLuint m_textVAO = 0;
    GLuint m_textVBO = 0;
    GLuint m_textTex = 0;
    int m_textUniformProj = -1;
    int m_textUniformTex = -1;
    int m_textPrevW = 0;
    int m_textPrevH = 0;

    bool InitTextOverlayResources();
    GLBatch m_bgBatch;         // 所有背景矩形 (GL_TRIANGLES)
    GLBatch m_lineBatch;       // 所有线条 (GL_LINES)
    GLBatch m_quadBatch;       // bus rects, markers, overlays (GL_TRIANGLES)
    GLBatch m_minimapBgBatch;  // minimap 背景（须在 lineBatch 之后绘制）
    GLBatch m_minimapLineBatch; // minimap 信号线 + 视窗框

    bool m_initialized = false;
    int m_frameWidth = 0;
    int m_frameHeight = 0;
};

#endif // BEAR2WAVE_RENDER_OPENGL
