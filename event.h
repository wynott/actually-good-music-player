#pragma once

#include <functional>
#include <mutex>
#include <string>
#include <unordered_map>

struct Event
{
    std::string name;
    std::string payload;
};

class EventBus
{
public:
    using Handler = std::function<void(const Event&)>;

    static EventBus& instance();

    int subscribe(const std::string& name, Handler handler);
    void unsubscribe(int id);
    void publish(const Event& event);

private:
    EventBus() = default;

    struct Subscription
    {
        std::string name;
        Handler handler;
    };

    int _next_id = 1;
    std::unordered_map<int, Subscription> _subscriptions;
    std::mutex _mutex;
};
