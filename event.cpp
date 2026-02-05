#include "event.h"

#include <vector>

EventBus& EventBus::instance()
{
    static EventBus bus;
    return bus;
}

int EventBus::subscribe(const std::string& name, Handler handler)
{
    if (!handler)
    {
        return 0;
    }

    std::lock_guard<std::mutex> lock(_mutex);
    int id = _next_id++;
    _subscriptions.emplace(id, Subscription{name, std::move(handler)});
    return id;
}

void EventBus::unsubscribe(int id)
{
    if (id == 0)
    {
        return;
    }

    std::lock_guard<std::mutex> lock(_mutex);
    _subscriptions.erase(id);
}

void EventBus::publish(const Event& event)
{
    std::vector<Handler> handlers;
    {
        std::lock_guard<std::mutex> lock(_mutex);
        handlers.reserve(_subscriptions.size());
        for (const auto& entry : _subscriptions)
        {
            const Subscription& sub = entry.second;
            if (sub.name == event.name)
            {
                handlers.push_back(sub.handler);
            }
        }
    }

    for (const auto& handler : handlers)
    {
        handler(event);
    }
}
