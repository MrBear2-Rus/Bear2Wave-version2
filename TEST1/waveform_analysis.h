#pragma once

#include <string>
#include <vector>

#include "vcd.h"

class WaveformPanel;

namespace WaveformAnalysis {

void PrepareSignals(WaveformPanel* panel, const std::vector<signal_t*>& signals);
std::string BuildContext(WaveformPanel* panel, const std::vector<signal_t*>& signals);
bool IsPlaceholderApiKey(const std::string& key);
int MaxEdgesPerSignal();
int MaxContextChars();

}
