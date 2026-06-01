/* Standalone smoke test for LlmClient JSON parsing (E1-5, no network). */
#include "core/LlmClient.h"

#include <cstdio>
#include <cstdlib>

int main()
{
    if (!LlmClient::SelfTestJsonParser()) {
        fprintf(stderr, "LlmClient::SelfTestJsonParser FAILED\n");
        return 1;
    }
    fprintf(stderr, "LlmClient::SelfTestJsonParser OK\n");
    return 0;
}
