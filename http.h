#pragma once

#include <string>
#include <vector>

bool http_get(const std::string& url, const std::string& user_agent, const std::string& accept, std::vector<unsigned char>& data);
std::string url_encode(const std::string& value);
bool http_init();
void http_cleanup();
bool http_is_available();
