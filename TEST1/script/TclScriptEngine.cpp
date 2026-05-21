#include "script/TclScriptEngine.h"

#include <cstring>
#include <vector>

#if defined(BEAR2WAVE_WITH_TCL)
#include <tcl.h>
#endif

namespace {

#if defined(BEAR2WAVE_WITH_TCL)

struct TclHost {
    Tcl_Interp* interp = nullptr;
    const WaveformCommandHandlers* handlers = nullptr;
};

static TclHost* HostFromInterp(Tcl_Interp* interp)
{
    return static_cast<TclHost*>(Tcl_GetAssocData(interp, "bear2wave", nullptr));
}

static int SetError(Tcl_Interp* interp, const std::string& msg)
{
    Tcl_SetObjResult(interp, Tcl_NewStringObj(msg.c_str(), -1));
    return TCL_ERROR;
}

static int Cmd_nop(ClientData, Tcl_Interp* interp, int, Tcl_Obj* const*)
{
    Tcl_SetObjResult(interp, Tcl_NewStringObj("", -1));
    return TCL_OK;
}

static int Cmd_load(ClientData, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[])
{
    TclHost* host = HostFromInterp(interp);
    if (!host || !host->handlers || !host->handlers->loadTrace)
        return SetError(interp, "bear2wave: load handler not configured");
    if (objc < 2)
        return SetError(interp, "usage: bear2wave_load path");
    const char* path = Tcl_GetString(objv[1]);
    std::string err;
    if (!host->handlers->loadTrace(path, err))
        return SetError(interp, err.empty() ? "load failed" : err);
    Tcl_SetObjResult(interp, Tcl_NewStringObj("ok", -1));
    return TCL_OK;
}

static int Cmd_add(ClientData, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[])
{
    TclHost* host = HostFromInterp(interp);
    if (!host || !host->handlers || !host->handlers->addSignals)
        return SetError(interp, "bear2wave: add handler not configured");
    if (objc < 2)
        return SetError(interp, "usage: bear2wave_add name ?name ...?");
    std::vector<std::string> names;
    names.reserve(static_cast<size_t>(objc - 1));
    for (int i = 1; i < objc; ++i) {
        const char* s = Tcl_GetString(objv[i]);
        if (s && s[0])
            names.emplace_back(s);
    }
    std::string err;
    if (!host->handlers->addSignals(names, err))
        return SetError(interp, err.empty() ? "add failed" : err);
    Tcl_SetObjResult(interp, Tcl_NewIntObj(static_cast<int>(names.size())));
    return TCL_OK;
}

static int Cmd_add_list(ClientData, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[])
{
    if (objc < 2)
        return SetError(interp, "usage: bear2wave_add_list {name1 name2 ...}");
    int listLen = 0;
    if (Tcl_ListObjLength(interp, objv[1], &listLen) != TCL_OK)
        return SetError(interp, "expected a Tcl list");
    Tcl_Obj** elems = nullptr;
    if (Tcl_ListObjGetElements(interp, objv[1], &listLen, &elems) != TCL_OK)
        return SetError(interp, "invalid list");
    Tcl_Obj* fakev[256];
    int fakec = 1;
    fakev[0] = objv[0];
    const int cap = static_cast<int>(sizeof(fakev) / sizeof(fakev[0])) - 1;
    for (int i = 0; i < listLen && fakec < cap; ++i)
        fakev[fakec++] = elems[i];
    return Cmd_add(nullptr, interp, fakec, fakev);
}

static int Cmd_zoom(ClientData, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[])
{
    TclHost* host = HostFromInterp(interp);
    if (!host || !host->handlers)
        return SetError(interp, "bear2wave: zoom handler not configured");
    if (objc < 2)
        return SetError(interp, "usage: bear2wave_zoom full|in|out");
    const char* mode = Tcl_GetString(objv[1]);
    std::string err;
    bool ok = false;
    if (strcmp(mode, "full") == 0)
        ok = host->handlers->zoomFull && host->handlers->zoomFull(err);
    else if (strcmp(mode, "in") == 0)
        ok = host->handlers->zoomIn && host->handlers->zoomIn(err);
    else if (strcmp(mode, "out") == 0)
        ok = host->handlers->zoomOut && host->handlers->zoomOut(err);
    else
        return SetError(interp, "zoom mode must be full, in, or out");
    if (!ok)
        return SetError(interp, err.empty() ? "zoom failed" : err);
    Tcl_SetObjResult(interp, Tcl_NewStringObj("ok", -1));
    return TCL_OK;
}

static int Cmd_page(ClientData, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[])
{
    TclHost* host = HostFromInterp(interp);
    if (!host || !host->handlers)
        return SetError(interp, "bear2wave: page handler not configured");
    if (objc < 2)
        return SetError(interp, "usage: bear2wave_page left|right");
    const char* dir = Tcl_GetString(objv[1]);
    std::string err;
    bool ok = false;
    if (strcmp(dir, "left") == 0)
        ok = host->handlers->pageLeft && host->handlers->pageLeft(err);
    else if (strcmp(dir, "right") == 0)
        ok = host->handlers->pageRight && host->handlers->pageRight(err);
    else
        return SetError(interp, "page direction must be left or right");
    if (!ok)
        return SetError(interp, err.empty() ? "page failed" : err);
    Tcl_SetObjResult(interp, Tcl_NewStringObj("ok", -1));
    return TCL_OK;
}

static int Cmd_set_time(ClientData, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[])
{
    TclHost* host = HostFromInterp(interp);
    if (!host || !host->handlers || !host->handlers->setPlayhead)
        return SetError(interp, "bear2wave: set_time handler not configured");
    if (objc < 2)
        return SetError(interp, "usage: bear2wave_set_time time");
    long long t = 0;
    if (Tcl_GetLongFromObj(interp, objv[1], &t) != TCL_OK)
        return SetError(interp, "expected integer time");
    std::string err;
    if (!host->handlers->setPlayhead(t, err))
        return SetError(interp, err.empty() ? "set_time failed" : err);
    Tcl_SetObjResult(interp, Tcl_NewStringObj("ok", -1));
    return TCL_OK;
}

static int Cmd_set_range(ClientData, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[])
{
    TclHost* host = HostFromInterp(interp);
    if (!host || !host->handlers || !host->handlers->setTimeRange)
        return SetError(interp, "bear2wave: set_range handler not configured");
    if (objc < 3)
        return SetError(interp, "usage: bear2wave_set_range start end");
    long long a = 0, b = 0;
    if (Tcl_GetLongFromObj(interp, objv[1], &a) != TCL_OK || Tcl_GetLongFromObj(interp, objv[2], &b) != TCL_OK)
        return SetError(interp, "expected integer start end");
    std::string err;
    if (!host->handlers->setTimeRange(a, b, err))
        return SetError(interp, err.empty() ? "set_range failed" : err);
    Tcl_SetObjResult(interp, Tcl_NewStringObj("ok", -1));
    return TCL_OK;
}

static int Cmd_echo(ClientData, Tcl_Interp* interp, int objc, Tcl_Obj* const objv[])
{
    TclHost* host = HostFromInterp(interp);
    if (!host || !host->handlers || !host->handlers->logMessage)
        return TCL_OK;
    std::string msg;
    for (int i = 1; i < objc; ++i) {
        if (i > 1) msg.push_back(' ');
        const char* s = Tcl_GetString(objv[i]);
        if (s) msg += s;
    }
    host->handlers->logMessage(msg);
    Tcl_SetObjResult(interp, Tcl_NewStringObj(msg.c_str(), -1));
    return TCL_OK;
}

static void RegisterCommands(Tcl_Interp* interp)
{
    Tcl_CreateObjCommand(interp, "bear2wave_nop", Cmd_nop, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "bear2wave_load", Cmd_load, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "bear2wave_add", Cmd_add, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "bear2wave_add_list", Cmd_add_list, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "bear2wave_zoom", Cmd_zoom, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "bear2wave_page", Cmd_page, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "bear2wave_set_time", Cmd_set_time, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "bear2wave_set_range", Cmd_set_range, nullptr, nullptr);
    Tcl_CreateObjCommand(interp, "bear2wave_echo", Cmd_echo, nullptr, nullptr);
}

static const char* kCompatScript = R"(
namespace eval gtkwave {}
proc gtkwave::nop {} { return [bear2wave_nop] }
proc gtkwave::loadFile {path} { return [bear2wave_load $path] }
proc gtkwave::addSignalsFromList {lst} { return [bear2wave_add_list $lst] }
proc gtkwave::zoom_full {} { return [bear2wave_zoom full] }
proc gtkwave::zoom_in {} { return [bear2wave_zoom in] }
proc gtkwave::zoom_out {} { return [bear2wave_zoom out] }
proc gtkwave::page_left {} { return [bear2wave_page left] }
proc gtkwave::page_right {} { return [bear2wave_page right] }
)";

static bool InitInterp(Tcl_Interp* interp, std::string& err)
{
    if (Tcl_Init(interp) != TCL_OK) {
        const char* msg = Tcl_GetStringResult(interp);
        err = msg ? msg : "Tcl_Init failed";
        return false;
    }
    RegisterCommands(interp);
    if (Tcl_EvalEx(interp, kCompatScript, -1, 0) != TCL_OK) {
        const char* msg = Tcl_GetStringResult(interp);
        err = msg ? msg : "failed to install gtkwave compatibility procs";
        return false;
    }
    return true;
}

static bool RunInterp(Tcl_Interp* interp, const std::string& script, std::string& err)
{
    if (Tcl_EvalEx(interp, script.c_str(), -1, 0) != TCL_OK) {
        const char* msg = Tcl_GetStringResult(interp);
        err = msg ? msg : "Tcl evaluation failed";
        return false;
    }
    return true;
}

#endif /* BEAR2WAVE_WITH_TCL */

} // namespace

bool TclScriptEngine::IsAvailable()
{
#if defined(BEAR2WAVE_WITH_TCL)
    return true;
#else
    return false;
#endif
}

bool TclScriptEngine::RunString(const std::string& script, const WaveformCommandHandlers& handlers, std::string& err)
{
#if !defined(BEAR2WAVE_WITH_TCL)
    (void)script;
    (void)handlers;
    err = "Tcl support was not compiled in. Define BEAR2WAVE_WITH_TCL and link libtcl "
          "(e.g. vcpkg install tcl:x64-windows).";
    return false;
#else
    if (!handlers.IsComplete()) {
        err = "Waveform command handlers incomplete";
        return false;
    }
    TclHost host;
    host.handlers = &handlers;
    host.interp = Tcl_CreateInterp();
    if (!host.interp) {
        err = "Tcl_CreateInterp failed";
        return false;
    }
    Tcl_SetAssocData(host.interp, "bear2wave", nullptr, &host);
    if (!InitInterp(host.interp, err)) {
        Tcl_DeleteInterp(host.interp);
        return false;
    }
    const bool ok = RunInterp(host.interp, script, err);
    Tcl_DeleteInterp(host.interp);
    return ok;
#endif
}

bool TclScriptEngine::RunFile(const std::string& utf8Path, const WaveformCommandHandlers& handlers, std::string& err)
{
#if !defined(BEAR2WAVE_WITH_TCL)
    (void)utf8Path;
    (void)handlers;
    err = "Tcl support was not compiled in. Define BEAR2WAVE_WITH_TCL and link libtcl "
          "(e.g. vcpkg install tcl:x64-windows).";
    return false;
#else
    if (!handlers.IsComplete()) {
        err = "Waveform command handlers incomplete";
        return false;
    }
    TclHost host;
    host.handlers = &handlers;
    host.interp = Tcl_CreateInterp();
    if (!host.interp) {
        err = "Tcl_CreateInterp failed";
        return false;
    }
    Tcl_SetAssocData(host.interp, "bear2wave", nullptr, &host);
    if (!InitInterp(host.interp, err)) {
        Tcl_DeleteInterp(host.interp);
        return false;
    }
    if (Tcl_EvalFile(host.interp, utf8Path.c_str()) != TCL_OK) {
        const char* msg = Tcl_GetStringResult(host.interp);
        err = msg ? msg : "Tcl_EvalFile failed";
        Tcl_DeleteInterp(host.interp);
        return false;
    }
    Tcl_DeleteInterp(host.interp);
    return true;
#endif
}
