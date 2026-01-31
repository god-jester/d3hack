#pragma once

#include <cstdint>

// ported from https://gbatemp.net/threads/tagnx-a-wip-to-play-some-online-third-party-games-on-banned-switches.665752/
namespace d3::tagnx {
    void InstallHooks();
}

namespace d3::tagnx::curl {
    struct CURL {};

    struct curl_slist {
        const char *data = nullptr;
        curl_slist *next = nullptr;
    };

    CURL *curl_easy_init();
    void curl_easy_setopt(CURL *curl, int opt, void *data);
    curl_slist *curl_slist_append(curl_slist *headers, const char *data);
    int curl_easy_perform(CURL *curl);
    std::int64_t curl_multi_init();
    void curl_multi_add_handle(std::int64_t multi_handle, CURL *easy_handle);
    std::int64_t curl_multi_perform(std::int64_t multi_handle, int *running);
}
