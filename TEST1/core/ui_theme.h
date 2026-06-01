#pragma once

#include <wx/colour.h>

/** Application UI theme (waveform plot + panel chrome). Default: Light. */
enum class UiThemeId {
    Light = 0,
    Dark = 1,
};

struct UiThemeColors {
    wxColour panelBg;
    wxColour plotBg;
    wxColour rowStripe;
    wxColour axisBand;
    wxColour scrollBarBg;
    wxColour signalNameText;
    wxColour signalNameDim;
    wxColour traceDefault;
    wxColour selectedRow;
    wxColour commentRow;
    wxColour gridMajor;
    wxColour gridMinor;
    wxColour playhead;
    wxColour baseline;
    wxColour measureLine;
    wxColour cursorValueBg;
    wxColour markerLabelBg;
    wxColour hoverHighlight;
    wxColour textOverlayBg; /* GL text layer clear (transparent areas) */
};

UiThemeId CurrentThemeId();
const UiThemeColors& CurrentTheme();
bool IsDarkTheme();
void SetTheme(UiThemeId id);

/** RGBA 0..1 for glClearColor. */
void CurrentThemeGlClear(float out_rgba[4]);

void LoadThemePreference();
void SaveThemePreference();
