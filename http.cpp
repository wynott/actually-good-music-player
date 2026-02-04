#include "http.h"

#include <curl/curl.h>

static bool g_http_ready = false;

static size_t write_callback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
    size_t total = size * nmemb;
    auto* buffer = reinterpret_cast<std::vector<unsigned char>*>(userdata);
    buffer->insert(buffer->end(), ptr, ptr + total);
    return total;
}

bool http_get(const std::string& url, const std::string& user_agent, const std::string& accept, std::vector<unsigned char>& data)
{
    data.clear();

    if (!g_http_ready)
    {
        return false;
    }

    CURL* curl = curl_easy_init();
    if (!curl)
    {
        return false;
    }

    curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
    curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
    curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, write_callback);
    curl_easy_setopt(curl, CURLOPT_WRITEDATA, &data);
    curl_easy_setopt(curl, CURLOPT_USERAGENT, user_agent.c_str());

    struct curl_slist* headers = nullptr;
    if (!accept.empty())
    {
        std::string header = "Accept: " + accept;
        headers = curl_slist_append(headers, header.c_str());
        curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
    }

    CURLcode result = curl_easy_perform(curl);

    if (headers)
    {
        curl_slist_free_all(headers);
    }
    curl_easy_cleanup(curl);

    return result == CURLE_OK && !data.empty();
}

std::string url_encode(const std::string& value)
{
    static const char* hex = "0123456789ABCDEF";
    std::string out;
    for (unsigned char ch : value)
    {
        if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || (ch >= '0' && ch <= '9') ||
            ch == '-' || ch == '_' || ch == '.' || ch == '~')
        {
            out.push_back(static_cast<char>(ch));
        }
        else
        {
            out.push_back('%');
            out.push_back(hex[(ch >> 4) & 0xF]);
            out.push_back(hex[ch & 0xF]);
        }
    }
    return out;
}

bool http_init()
{
    CURLcode result = curl_global_init(CURL_GLOBAL_DEFAULT);
    g_http_ready = (result == CURLE_OK);
    return g_http_ready;
}

void http_cleanup()
{
    if (g_http_ready)
    {
        curl_global_cleanup();
        g_http_ready = false;
    }
}

bool http_is_available()
{
    return g_http_ready;
}
