#pragma once
#ifndef MAIN_H
#define MAIN_H

/**
 * Legacy umbrella header (optional).
 *
 * Layout of the application sources:
 *   Main.cpp              — wxWidgets entry (MyApp, wxIMPLEMENT_APP).
 *   ui/MainFrame.hpp      — Main window: menus, signal tree, toolbar, file I/O, CSV/FST/VCD wiring.
 *   panels/WaveformPanel.hpp — OpenGL waveform trace panel (large, header-only for now).
 *   AIAnalysisPanel.h / AIAnalysisPanel.cpp — AI analysis side panel.
 *   waveform_constants.h — SIGNAL_ROW_HEIGHT, LEFT_MARGIN, WAVE_PADDING.
 */

#include "ui/MainFrame.hpp"

#endif
