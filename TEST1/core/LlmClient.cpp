#include "LlmClient.h"

#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#include <wininet.h>

#include <algorithm>
#include <cctype>
#include <sstream>

#pragma comment(lib, "wininet.lib")

namespace LlmClient {

static bool cancelled(const LlmChatRequest& req)
{
    return req.cancel_flag && req.cancel_flag->load();
}

std::string EscapeJsonString(const std::string& s)
{
    std::string out;
    out.reserve(s.size() + 16);
    for (unsigned char c : s) {
        switch (c) {
        case '\\': out += "\\\\"; break;
        case '"': out += "\\\""; break;
        case '\n': out += "\\n"; break;
        case '\r': out += "\\r"; break;
        case '\t': out += "\\t"; break;
        default:
            if (c >= 32)
                out += (char)c;
            break;
        }
    }
    return out;
}

std::string BuildChatPayload(const LlmChatRequest& req)
{
    std::vector<LlmChatMessage> msgs = req.messages;
    if (msgs.empty() && !req.user_message_utf8.empty())
        msgs.push_back({"user", req.user_message_utf8});

    std::ostringstream os;
    os << "{\"model\":\"" << EscapeJsonString(req.model) << "\",\"messages\":[";
    for (size_t i = 0; i < msgs.size(); ++i) {
        if (i)
            os << ',';
        os << "{\"role\":\"" << EscapeJsonString(msgs[i].role)
           << "\",\"content\":\"" << EscapeJsonString(msgs[i].content) << "\"}";
    }
    os << "],\"temperature\":" << req.temperature << "}";
    return os.str();
}

static bool parse_json_string_at(const std::string& json, size_t quote_pos, std::string* out, size_t* out_end)
{
    if (quote_pos >= json.size() || json[quote_pos] != '"')
        return false;
    std::string val;
    size_t i = quote_pos + 1;
    while (i < json.size()) {
        const char c = json[i++];
        if (c == '"') {
            if (out)
                *out = val;
            if (out_end)
                *out_end = i;
            return true;
        }
        if (c == '\\' && i < json.size()) {
            const char e = json[i++];
            switch (e) {
            case '"': val += '"'; break;
            case '\\': val += '\\'; break;
            case '/': val += '/'; break;
            case 'n': val += '\n'; break;
            case 'r': val += '\r'; break;
            case 't': val += '\t'; break;
            case 'b': val += '\b'; break;
            case 'f': val += '\f'; break;
            case 'u':
                if (i + 3 < json.size())
                    i += 4;
                break;
            default: val += e; break;
            }
        } else {
            val += c;
        }
    }
    return false;
}

static size_t find_json_string_value(const std::string& json, const char* key, size_t from = 0)
{
    const std::string needle = std::string("\"") + key + "\"";
    size_t pos = json.find(needle, from);
    while (pos != std::string::npos) {
        pos += needle.size();
        while (pos < json.size() && std::isspace((unsigned char)json[pos]))
            ++pos;
        if (pos < json.size() && json[pos] == ':') {
            ++pos;
            while (pos < json.size() && std::isspace((unsigned char)json[pos]))
                ++pos;
            if (pos < json.size() && json[pos] == '"')
                return pos;
        }
        pos = json.find(needle, pos);
    }
    return std::string::npos;
}

bool ParseChatCompletionContent(const std::string& json, std::string* out_content, std::string* out_error)
{
    if (out_content)
        out_content->clear();
    if (out_error)
        out_error->clear();

    const size_t errKey = find_json_string_value(json, "message", json.find("\"error\""));
    if (errKey != std::string::npos) {
        std::string msg;
        parse_json_string_at(json, errKey, &msg, nullptr);
        if (out_error)
            *out_error = msg;
        return false;
    }

    size_t searchFrom = 0;
    const size_t choices = json.find("\"choices\"");
    if (choices != std::string::npos)
        searchFrom = choices;

    const size_t msgKey = json.find("\"message\"", searchFrom);
    if (msgKey == std::string::npos)
        return false;

    const size_t contentQuote = find_json_string_value(json, "content", msgKey);
    if (contentQuote == std::string::npos)
        return false;

    std::string content;
    if (!parse_json_string_at(json, contentQuote, &content, nullptr))
        return false;

    if (out_content)
        *out_content = content;
    return !content.empty();
}

bool SelfTestJsonParser()
{
    const char* sample =
        "{\"choices\":[{\"message\":{\"content\":\"hello\\nworld\"}}]}";
    std::string content;
    if (!ParseChatCompletionContent(sample, &content, nullptr))
        return false;
    return content == "hello\nworld";
}

static std::wstring to_wide(const std::string& utf8)
{
    if (utf8.empty())
        return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), nullptr, 0);
    if (n <= 0)
        return {};
    std::wstring w((size_t)n, L'\0');
    MultiByteToWideChar(CP_UTF8, 0, utf8.c_str(), (int)utf8.size(), w.data(), n);
    return w;
}

LlmChatResult PostChatCompletion(const LlmChatRequest& req)
{
    LlmChatResult result;
    if (cancelled(req)) {
        result.cancelled = true;
        result.error_code = "cancelled";
        return result;
    }

    const std::string payload = BuildChatPayload(req);
    const std::wstring hostW = to_wide(req.host);
    const std::wstring pathW = to_wide(req.path);
    const std::wstring authHdr = L"Authorization: Bearer " + to_wide(req.api_key);

    HINTERNET hInternet = nullptr;
    HINTERNET hConnect = nullptr;
    HINTERNET hRequest = nullptr;

    hInternet = InternetOpen(L"Bear2Wave", INTERNET_OPEN_TYPE_PRECONFIG, nullptr, nullptr, 0);
    if (!hInternet) {
        result.error_code = "net_init";
        return result;
    }

    hConnect = InternetConnect(hInternet, hostW.c_str(), INTERNET_DEFAULT_HTTPS_PORT,
        nullptr, nullptr, INTERNET_SERVICE_HTTP, 0, 0);
    if (!hConnect) {
        InternetCloseHandle(hInternet);
        result.error_code = "connect";
        return result;
    }

    hRequest = HttpOpenRequest(hConnect, L"POST", pathW.c_str(), nullptr, nullptr, nullptr,
        INTERNET_FLAG_SECURE | INTERNET_FLAG_RELOAD, 0);
    if (!hRequest) {
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        result.error_code = "open_request";
        return result;
    }

    const std::wstring headers = L"Content-Type: application/json\r\n" + authHdr + L"\r\n";
    if (!HttpAddRequestHeaders(hRequest, headers.c_str(), (DWORD)-1L, HTTP_ADDREQ_FLAG_ADD)) {
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        result.error_code = "headers";
        return result;
    }

    if (!HttpSendRequestA(hRequest, nullptr, 0, (LPVOID)payload.data(), (DWORD)payload.size())) {
        InternetCloseHandle(hRequest);
        InternetCloseHandle(hConnect);
        InternetCloseHandle(hInternet);
        result.error_code = "send";
        return result;
    }

    DWORD statusCode = 0;
    DWORD statusSize = sizeof(statusCode);
    HttpQueryInfo(hRequest, HTTP_QUERY_STATUS_CODE | HTTP_QUERY_FLAG_NUMBER,
        &statusCode, &statusSize, nullptr);

    std::string body;
    char buffer[4096];
    DWORD dwRead = 0;
    while (InternetReadFile(hRequest, buffer, sizeof(buffer) - 1, &dwRead) && dwRead > 0) {
        if (cancelled(req)) {
            result.cancelled = true;
            result.error_code = "cancelled";
            InternetCloseHandle(hRequest);
            InternetCloseHandle(hConnect);
            InternetCloseHandle(hInternet);
            return result;
        }
        buffer[dwRead] = '\0';
        body.append(buffer, dwRead);
    }

    InternetCloseHandle(hRequest);
    InternetCloseHandle(hConnect);
    InternetCloseHandle(hInternet);

    result.raw_response = body;

    if (cancelled(req)) {
        result.cancelled = true;
        result.error_code = "cancelled";
        return result;
    }

    if (body.empty()) {
        result.error_code = "empty_response";
        return result;
    }

    std::string apiErr;
    std::string content;
    if (!ParseChatCompletionContent(body, &content, &apiErr)) {
        if (statusCode == 401)
            result.error_code = "http_401";
        else if (statusCode == 429)
            result.error_code = "http_429";
        else if (statusCode >= 500) {
            result.error_code = "http_5xx";
            result.error_detail = std::to_string(statusCode);
        } else if (!apiErr.empty()) {
            result.error_code = "api_error";
            result.error_detail = apiErr;
        } else {
            result.error_code = "parse_response";
        }
        return result;
    }

    result.ok = true;
    result.assistant_content = content;
    return result;
}

} // namespace LlmClient
