#pragma once

#include <functional>
#include <string>
#include <vector>

/** UI-neutral command surface for Tcl / future automation. */
struct WaveformCommandHandlers {
    std::function<bool(const std::string& path, std::string& err)> loadTrace;
    std::function<bool(const std::vector<std::string>& names, std::string& err)> addSignals;
    std::function<bool(std::string& err)> zoomFull;
    std::function<bool(std::string& err)> zoomIn;
    std::function<bool(std::string& err)> zoomOut;
    std::function<bool(std::string& err)> pageLeft;
    std::function<bool(std::string& err)> pageRight;
    std::function<bool(long long t, std::string& err)> setPlayhead;
    std::function<bool(long long from, long long to, std::string& err)> setTimeRange;
    std::function<void(const std::string& msg)> logMessage;

    bool IsComplete() const
    {
        return static_cast<bool>(loadTrace) && static_cast<bool>(addSignals) && static_cast<bool>(zoomFull)
            && static_cast<bool>(zoomIn) && static_cast<bool>(zoomOut) && static_cast<bool>(pageLeft)
            && static_cast<bool>(pageRight) && static_cast<bool>(setPlayhead)
            && static_cast<bool>(setTimeRange);
    }
};
