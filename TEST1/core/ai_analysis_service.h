#pragma once

#include <atomic>
#include <deque>
#include <string>
#include <vector>

#include "bear2wave_config.h"
#include "core/LlmClient.h"
#include "vcd.h"
#include "waveform_analysis.h"

class WaveformPanel;

namespace AiAnalysisService {

struct ChatTurn {
    std::string user;
    std::string assistant;
};

struct BuildPromptOptions {
    WaveformPanel* panel = nullptr;
    std::vector<signal_t*> signals;
    WaveformAnalysis::AnalysisWindow window;
    std::string user_prompt;
    int template_index = 0;
    bool include_compare = false;
    bool include_handshake_hint = false;
    bool include_reset_hint = false;
};

int MaxChatTurns();
int MaxMessagesForApi();

std::string BuildFullContext(const BuildPromptOptions& opt);
std::vector<LlmChatMessage> BuildMessageList(
    const std::deque<ChatTurn>& history,
    const std::string& new_user_content);

LlmChatResult RunChat(
    const Bear2WaveConfig& cfg,
    const std::vector<LlmChatMessage>& messages,
    std::atomic<bool>* cancel_flag);

void AppendChatTurn(std::deque<ChatTurn>& history, const std::string& user, const std::string& assistant);

std::vector<long long> ParseTimestampsFromText(const std::string& text);

/** Convert e.g. "1.2ms" in user text to VCD/FST integer time using trace timescale. */
bool ParsePhysicalTimeToSimUnits(vcd_t* vcd, const std::string& text, long long* out_sim);

/** True when the prompt looks like a protocol/decode question (I2C, NACK, …). */
bool QueryMentionsProtocolDecode(const std::string& query);

/** Add displayed TXN decode rows when the query is protocol-related. */
void AugmentSignalsWithTransactionDecodes(
    WaveformPanel* panel,
    std::vector<signal_t*>& signals,
    const std::string& query);

std::vector<signal_t*> MatchSignalsByQuery(
    vcd_t* vcd,
    const std::vector<signal_t*>& pool,
    const std::string& query,
    size_t max_results = 40);

void ApplyOllamaPreset(Bear2WaveConfig& cfg);

} // namespace AiAnalysisService
