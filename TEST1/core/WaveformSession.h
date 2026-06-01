#pragma once

#include <string>
#include <vector>

#include <wx/string.h>

#include "core/WaveformRadix.h"

/** One trace row in a session (GTKWave +signal line + optional @radix). */
struct SessionTrace {
    std::string name;
    int gtkwaveRadixCode = -1; /**< -1 = default (binary); else 0,1,2,4,5,6,7 */
};

/** Viewer + UI layout restored from .gtkw / .b2w. */
struct WaveformSessionData {
    std::string tracePath;
    long long timeOffset = 0;
    long long displayRange = 0;
    long long playhead = 0;
    bool showCursorValue = false;
    std::vector<SessionTrace> traces;

    /** GTKWave [treeopen] module paths (e.g. TOP.cpu). */
    std::vector<std::string> treeOpenPaths;

    int windowX = -1;
    int windowY = -1;
    int windowW = 0;
    int windowH = 0;

    /** Splitter sash positions (pixels). */
    int splitterTreeWave = 0;
    int splitterMainAi = 0;
    int splitterTreeList = 0;
};

namespace WaveformSession {

bool Load(const wxString& path, WaveformSessionData& out, wxString& err);
bool Save(const wxString& path, const WaveformSessionData& data, wxString& err);

bool IsSupportedExtension(const wxString& path);

} // namespace WaveformSession
