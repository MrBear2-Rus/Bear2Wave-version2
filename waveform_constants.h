#pragma once

/** Layout shared by the main frame and the waveform GL panel. */
#define SIGNAL_ROW_HEIGHT 60
#define LEFT_MARGIN 180
#define WAVE_PADDING 40

/** wxSlider maps 0..kSliderDivisions to full trace when trace length exceeds INT_MAX. */
#ifndef BEAR2WAVE_SLIDER_DIVISIONS
#define BEAR2WAVE_SLIDER_DIVISIONS 10000
#endif
