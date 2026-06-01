#pragma once

class MyFrame;

/** GTKWave-style multi-window compare: registry + optional playhead / time-view link. */
namespace WaveformCompare {

void RegisterFrame(MyFrame* frame);
void UnregisterFrame(MyFrame* frame);

bool LinkPlayheads();
void SetLinkPlayheads(bool enabled);

bool LinkTimeView();
void SetLinkTimeView(bool enabled);

void BroadcastPlayhead(MyFrame* source, long long t);
void BroadcastTimeView(MyFrame* source, long long offset, long long range);

/** Resize and place all viewer windows side by side on the primary display. */
void TileFramesHorizontally();

} // namespace WaveformCompare
