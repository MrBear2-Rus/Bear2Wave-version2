#pragma once

#include <atomic>
#include <string>
#include <vector>

struct LlmChatMessage {
    std::string role;
    std::string content;
};

struct LlmChatRequest {
    std::string api_key;
    std::string host = "api.deepseek.com";
    std::string path = "/v1/chat/completions";
    std::string model = "deepseek-chat";
    float temperature = 0.4f;
    /** Used when messages is empty. */
    std::string user_message_utf8;
    std::vector<LlmChatMessage> messages;
    std::atomic<bool>* cancel_flag = nullptr;
};

struct LlmChatResult {
    bool ok = false;
    bool cancelled = false;
    std::string assistant_content;
    std::string raw_response;
    std::string error_code;
    std::string error_detail;
};

namespace LlmClient {

std::string EscapeJsonString(const std::string& s);
std::string BuildChatPayload(const LlmChatRequest& req);
LlmChatResult PostChatCompletion(const LlmChatRequest& req);

bool ParseChatCompletionContent(const std::string& json, std::string* out_content, std::string* out_error);

/** Smoke-test JSON parser (no network). Returns false on failure. */
bool SelfTestJsonParser();

} // namespace LlmClient
