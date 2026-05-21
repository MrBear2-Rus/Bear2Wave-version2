#pragma once

#include "core/WaveformCommandHandlers.h"

#include <string>

/** Embedded Tcl (libtcl) script runner; requires BEAR2WAVE_WITH_TCL at compile time. */
class TclScriptEngine {
public:
    static bool IsAvailable();

    /** Run a .tcl file on the UI thread. Returns false on Tcl/runtime error (see err). */
    static bool RunFile(const std::string& utf8Path, const WaveformCommandHandlers& handlers, std::string& err);

    /** Run a Tcl snippet (e.g. one-liner from REPL). */
    static bool RunString(const std::string& script, const WaveformCommandHandlers& handlers, std::string& err);
};
