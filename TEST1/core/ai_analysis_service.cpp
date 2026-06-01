#include "ai_analysis_service.h"

#include "core/waveform_perf.h"
#include "panels/WaveformPanel.hpp"
#include "waveform_local_stats.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <algorithm>
#include <cctype>
#include <cstdlib>
#include <regex>
#include <set>
#include <sstream>

namespace AiAnalysisService {

int MaxChatTurns()
{
    return std::max(1, WaveformPerf::EnvInt("BEAR2WAVE_AI_MAX_CHAT_TURNS", 4));
}

int MaxMessagesForApi()
{
    return std::max(2, MaxChatTurns() * 2);
}

std::string BuildFullContext(const BuildPromptOptions& opt)
{
    if (!opt.panel)
        return {};

    WaveformAnalysis::AnalysisWindow window = opt.window;
    if (!opt.user_prompt.empty()) {
        const WaveformAnalysis::AnalysisWindow refined =
            WaveformAnalysis::RefineWindowForUserQuery(opt.panel, window, opt.user_prompt);
        if (refined.valid)
            window = refined;
    }

    std::vector<signal_t*> signals = opt.signals;
    AugmentSignalsWithTransactionDecodes(opt.panel, signals, opt.user_prompt);

    const WaveformAnalysis::AnalysisWindow* winPtr = window.valid ? &window : nullptr;

    WaveformAnalysis::PrepareSignals(opt.panel, signals);
    std::string ctx = WaveformAnalysis::BuildContext(opt.panel, signals, winPtr);

    const std::string txn = WaveformAnalysis::BuildTransactionDecodeContext(
        opt.panel, window.valid ? window : WaveformAnalysis::ViewportWindow(opt.panel), opt.user_prompt);
    if (!txn.empty())
        ctx += "\n" + txn;

    const std::string sup = WaveformAnalysis::BuildSupplement(
        opt.panel, signals, window.valid ? window : WaveformAnalysis::ViewportWindow(opt.panel),
        opt.template_index, opt.include_compare);
    if (!sup.empty())
        ctx += "\n" + sup;
    return ctx;
}

std::vector<LlmChatMessage> BuildMessageList(
    const std::deque<ChatTurn>& history,
    const std::string& new_user_content)
{
    std::vector<LlmChatMessage> msgs;
    const int maxMsgs = MaxMessagesForApi();
    const int keepTurns = MaxChatTurns();

    const size_t start = history.size() > (size_t)keepTurns
        ? history.size() - (size_t)keepTurns
        : 0;

    for (size_t i = start; i < history.size(); ++i) {
        if (!history[i].user.empty())
            msgs.push_back({"user", history[i].user});
        if (!history[i].assistant.empty())
            msgs.push_back({"assistant", history[i].assistant});
    }
    msgs.push_back({"user", new_user_content});

    if ((int)msgs.size() > maxMsgs) {
        const int drop = (int)msgs.size() - maxMsgs;
        msgs.erase(msgs.begin(), msgs.begin() + drop);
    }
    return msgs;
}

LlmChatResult RunChat(
    const Bear2WaveConfig& cfg,
    const std::vector<LlmChatMessage>& messages,
    std::atomic<bool>* cancel_flag)
{
    LlmChatRequest req;
    req.api_key = cfg.api_key;
    req.host = cfg.api_host;
    req.path = cfg.api_path;
    req.model = cfg.model;
    req.temperature = cfg.temperature;
    req.messages = messages;
    req.cancel_flag = cancel_flag;
    return LlmClient::PostChatCompletion(req);
}

void AppendChatTurn(std::deque<ChatTurn>& history, const std::string& user, const std::string& assistant)
{
    history.push_back({user, assistant});
    while (history.size() > (size_t)MaxChatTurns())
        history.pop_front();
}

std::vector<long long> ParseTimestampsFromText(const std::string& text)
{
    std::vector<long long> out = WaveformAnalysis::ParseTimestampsFromText(text);
    if (!out.empty())
        return out;
    try {
        static const std::regex re(R"((?:@|time\s*|=)?\s*(\d{2,})\s*(?:ns|ps|us|fs|ms)?)", std::regex::icase);
        auto begin = std::sregex_iterator(text.begin(), text.end(), re);
        auto end = std::sregex_iterator();
        std::set<long long> uniq;
        for (auto it = begin; it != end; ++it) {
            const long long v = std::stoll((*it)[1].str());
            if (v > 0 && uniq.insert(v).second)
                out.push_back(v);
        }
    } catch (...) {
    }
    return out;
}

bool ParsePhysicalTimeToSimUnits(vcd_t* vcd, const std::string& text, long long* out_sim)
{
    return WaveformAnalysis::ParsePhysicalTimeToSimUnits(vcd, text, out_sim);
}

bool QueryMentionsProtocolDecode(const std::string& query)
{
    return WaveformAnalysis::QueryMentionsProtocolDecode(query);
}

void AugmentSignalsWithTransactionDecodes(
    WaveformPanel* panel,
    std::vector<signal_t*>& signals,
    const std::string& query)
{
    if (!panel || !WaveformAnalysis::QueryMentionsProtocolDecode(query))
        return;
    std::set<signal_t*> have(signals.begin(), signals.end());
    for (signal_t* sig : WaveformAnalysis::CollectTransactionSyntheticSignals(panel)) {
        if (sig && have.insert(sig).second)
            signals.push_back(sig);
    }
}

static bool fuzzy_match(const std::string& hay, const std::string& needle)
{
    if (needle.empty())
        return true;
    return hay.find(needle) != std::string::npos;
}

std::vector<signal_t*> MatchSignalsByQuery(
    vcd_t* vcd,
    const std::vector<signal_t*>& pool,
    const std::string& query,
    size_t max_results)
{
    std::vector<signal_t*> out;
    if (query.empty())
        return out;

    std::string q = query;
    for (char& c : q)
        c = (char)std::tolower((unsigned char)c);

    auto try_add = [&](signal_t* s) {
        if (!s || out.size() >= max_results)
            return;
        std::string name = s->full_name[0] ? s->full_name : s->name;
        std::string mod = s->module_path;
        std::string blob = name + " " + mod;
        for (char& c : blob)
            c = (char)std::tolower((unsigned char)c);
        if (fuzzy_match(blob, q))
            out.push_back(s);
    };

    if (!pool.empty()) {
        for (signal_t* s : pool)
            try_add(s);
        return out;
    }

    if (!vcd)
        return out;
    for (signal_node_t* node = vcd->signals_head; node && out.size() < max_results; node = node->next)
        try_add(&node->signal);
    return out;
}

void ApplyOllamaPreset(Bear2WaveConfig& cfg)
{
    cfg.ApplyOllamaPreset();
}

} // namespace AiAnalysisService
