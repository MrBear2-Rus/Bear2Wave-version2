#pragma once

#include <wx/dc.h>
#include <wx/gdicmn.h>

class WaveformPanel;
class WaveformTextCanvas;

#ifdef BEAR2WAVE_RENDER_OPENGL
class WaveformGLRenderer;
#endif

namespace WaveformPainter {

// 原始软件渲染路径（wxDC）
void Paint(WaveformPanel& panel);

#ifdef BEAR2WAVE_RENDER_OPENGL
// OpenGL 渲染路径：GPU 几何 + wxDC 文字叠加
void PaintGL(WaveformPanel& panel, WaveformGLRenderer& gl);

// 纯文字叠加层（被 Paint 和 PaintGL 共用）
void PaintTextOverlay(WaveformPanel& panel, WaveformTextCanvas& canvas, wxSize& size,
                      int viewW, double scale, int cursorX, int scrollPx,
                      int firstRow, int lastRow);
#endif

} // namespace WaveformPainter
