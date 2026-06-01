#include "ui/WaveformCompareHub.h"

#include "ui/MainFrame.hpp"

#include <wx/display.h>
#include <algorithm>
#include <vector>

namespace WaveformCompare {

namespace {

std::vector<MyFrame*>& Registry()
{
    static std::vector<MyFrame*> frames;
    return frames;
}

bool& LinkPlayheadsFlag()
{
    static bool on = false;
    return on;
}

bool& LinkTimeViewFlag()
{
    static bool on = false;
    return on;
}

} // namespace

void RegisterFrame(MyFrame* frame)
{
    if (!frame)
        return;
    auto& reg = Registry();
    if (std::find(reg.begin(), reg.end(), frame) == reg.end())
        reg.push_back(frame);
}

void UnregisterFrame(MyFrame* frame)
{
    auto& reg = Registry();
    reg.erase(std::remove(reg.begin(), reg.end(), frame), reg.end());
}

bool LinkPlayheads() { return LinkPlayheadsFlag(); }
void SetLinkPlayheads(bool enabled) { LinkPlayheadsFlag() = enabled; }

bool LinkTimeView() { return LinkTimeViewFlag(); }
void SetLinkTimeView(bool enabled) { LinkTimeViewFlag() = enabled; }

void BroadcastPlayhead(MyFrame* source, long long t)
{
    if (!LinkPlayheads() || !source)
        return;
    for (MyFrame* frame : Registry()) {
        if (!frame || frame == source)
            continue;
        frame->ApplyLinkedPlayhead(t);
    }
}

void BroadcastTimeView(MyFrame* source, long long offset, long long range)
{
    if (!LinkTimeView() || !source)
        return;
    for (MyFrame* frame : Registry()) {
        if (!frame || frame == source)
            continue;
        frame->ApplyLinkedTimeView(offset, range);
    }
}

void TileFramesHorizontally()
{
    std::vector<MyFrame*> frames = Registry();
    frames.erase(std::remove(frames.begin(), frames.end(), nullptr), frames.end());
    if (frames.empty())
        return;

    MyFrame* anchor = frames[0];
    for (MyFrame* f : frames) {
        if (f && f->IsShown())
            anchor = f;
    }
    if (!anchor)
        anchor = frames[0];

    const int dispIdx = wxDisplay::GetFromWindow(anchor);
    wxDisplay disp(dispIdx >= 0 ? dispIdx : 0);
    const wxRect scr = disp.GetClientArea();

    const int n = static_cast<int>(frames.size());
    const int w = std::max(320, scr.width / n);
    int x = scr.x;
    for (int i = 0; i < n; ++i) {
        MyFrame* f = frames[static_cast<size_t>(i)];
        if (!f)
            continue;
        const int ww = (i == n - 1) ? (scr.x + scr.width - x) : w;
        f->SetSize(x, scr.y, ww, scr.height);
        f->Raise();
        x += ww;
    }
}

} // namespace WaveformCompare
