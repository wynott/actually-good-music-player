#pragma once

#include <string>

struct net_info
{
    std::string ip_address;
    int port;
};

bool start_network(int port, net_info& info);
void stop_network();
