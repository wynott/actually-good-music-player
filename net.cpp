#include "net.h"

#include <atomic>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <string>
#include <thread>
#include <vector>

#if defined(_WIN32)
#include <winsock2.h>
#include <ws2tcpip.h>
#pragma comment(lib, "ws2_32.lib")
#else
#include <arpa/inet.h>
#include <netdb.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

namespace
{
#if defined(_WIN32)
SOCKET g_socket = INVALID_SOCKET;
bool g_wsa_started = false;
#else
int g_socket = -1;
#endif

std::atomic<bool> g_running(false);
std::thread g_thread;

std::string get_local_ip()
{
    char host[256] = {};
    if (gethostname(host, sizeof(host)) != 0)
    {
        return "127.0.0.1";
    }

    addrinfo hints = {};
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;

    addrinfo* result = nullptr;
    if (getaddrinfo(host, nullptr, &hints, &result) != 0)
    {
        return "127.0.0.1";
    }

    std::string best = "127.0.0.1";
    for (addrinfo* ptr = result; ptr != nullptr; ptr = ptr->ai_next)
    {
        sockaddr_in* addr = reinterpret_cast<sockaddr_in*>(ptr->ai_addr);
        char buffer[INET_ADDRSTRLEN] = {};
        if (inet_ntop(AF_INET, &addr->sin_addr, buffer, sizeof(buffer)) != nullptr)
        {
            if (std::strncmp(buffer, "127.", 4) != 0)
            {
                best = buffer;
                break;
            }
        }
    }

    freeaddrinfo(result);
    return best;
}
}

bool start_network(int port, net_info& info)
{
    info.ip_address = "127.0.0.1";
    info.port = port;

#if defined(_WIN32)
    WSADATA wsa_data;
    if (WSAStartup(MAKEWORD(2, 2), &wsa_data) != 0)
    {
        return false;
    }
    g_wsa_started = true;

    g_socket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
    if (g_socket == INVALID_SOCKET)
    {
        stop_network();
        return false;
    }
#else
    g_socket = socket(AF_INET, SOCK_STREAM, 0);
    if (g_socket < 0)
    {
        stop_network();
        return false;
    }
#endif

    int opt = 1;
#if defined(_WIN32)
    setsockopt(g_socket, SOL_SOCKET, SO_REUSEADDR, reinterpret_cast<const char*>(&opt), sizeof(opt));
#else
    setsockopt(g_socket, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt));
#endif

    sockaddr_in addr = {};
    addr.sin_family = AF_INET;
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    addr.sin_port = htons(static_cast<uint16_t>(port));

    if (bind(g_socket, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) != 0)
    {
        stop_network();
        return false;
    }

    if (listen(g_socket, 4) != 0)
    {
        stop_network();
        return false;
    }

    if (port == 0)
    {
        sockaddr_in bound = {};
        socklen_t len = sizeof(bound);
        if (getsockname(g_socket, reinterpret_cast<sockaddr*>(&bound), &len) == 0)
        {
            info.port = ntohs(bound.sin_port);
        }
    }

    info.ip_address = get_local_ip();

    g_running.store(true, std::memory_order_release);
    g_thread = std::thread([]()
    {
        while (g_running.load(std::memory_order_acquire))
        {
#if defined(_WIN32)
            SOCKET client = accept(g_socket, nullptr, nullptr);
            if (client == INVALID_SOCKET)
            {
                if (!g_running.load(std::memory_order_acquire))
                {
                    break;
                }
                continue;
            }
#else
            int client = accept(g_socket, nullptr, nullptr);
            if (client < 0)
            {
                if (!g_running.load(std::memory_order_acquire))
                {
                    break;
                }
                continue;
            }
#endif

            std::vector<char> buffer(1024);
            for (;;)
            {
#if defined(_WIN32)
                int received = recv(client, buffer.data(), static_cast<int>(buffer.size()), 0);
#else
                int received = static_cast<int>(recv(client, buffer.data(), buffer.size(), 0));
#endif
                if (received <= 0)
                {
                    break;
                }
                std::string message(buffer.data(), buffer.data() + received);
                std::cout << "\n[net] " << message << std::endl;
            }

#if defined(_WIN32)
            closesocket(client);
#else
            close(client);
#endif
        }
    });

    return true;
}

void stop_network()
{
#if defined(_WIN32)
    g_running.store(false, std::memory_order_release);
#else
    g_running.store(false, std::memory_order_release);
#endif

#if defined(_WIN32)
    if (g_socket != INVALID_SOCKET)
    {
        closesocket(g_socket);
        g_socket = INVALID_SOCKET;
    }
    if (g_wsa_started)
    {
        WSACleanup();
        g_wsa_started = false;
    }
#else
    if (g_socket >= 0)
    {
        close(g_socket);
        g_socket = -1;
    }
#endif

    if (g_thread.joinable())
    {
        g_thread.join();
    }
}
