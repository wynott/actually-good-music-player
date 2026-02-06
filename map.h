#pragma once

#include <sstream>
#include <stdexcept>
#include <string>
#include <type_traits>
#include <unordered_map>

#include <spdlog/spdlog.h>

namespace wynott {

template <typename Key, typename T>
class map {
public:
    using map_type = std::unordered_map<Key, T>;
    using iterator = typename map_type::iterator;
    using const_iterator = typename map_type::const_iterator;

    explicit map(std::string name)
        : _debug_name(std::move(name)) {}

    map() = delete;

    template <typename K>
    static std::string stringify_key(const K& key)
    {
        return stringify_key_impl(key, is_streamable<K>{});
    }

    T& at(const Key& key)
    {
        auto it = _map.find(key);
        if (it == _map.end())
        {
            spdlog::error("wynott::map<...>::at: key '{}' not found in '{}'", stringify_key(key), _debug_name);
            throw std::out_of_range("wynott::map: key not found");
        }
        return it->second;
    }

    const T& at(const Key& key) const
    {
        auto it = _map.find(key);
        if (it == _map.end())
        {
            spdlog::error("wynott::map<...>::at: key '{}' not found in '{}'", stringify_key(key), _debug_name);
            throw std::out_of_range("wynott::map: key not found");
        }
        return it->second;
    }

    T* try_at(const Key& key)
    {
        auto it = _map.find(key);
        if (it == _map.end())
        {
            spdlog::warn("wynott::map<...>::try_at: key '{}' not found in '{}'", stringify_key(key), _debug_name);
            return nullptr;
        }
        return &it->second;
    }

    const T* try_at(const Key& key) const
    {
        auto it = _map.find(key);
        if (it == _map.end())
        {
            spdlog::warn("wynott::map<...>::try_at (const): key '{}' not found in '{}'", stringify_key(key), _debug_name);
            return nullptr;
        }
        return &it->second;
    }

    template <typename... Args>
    T& get_or_emplace(const Key& key, Args&&... args)
    {
        auto [it, inserted] = _map.try_emplace(key, std::forward<Args>(args)...);
        if (inserted)
        {
            spdlog::debug(
                "wynott::map<...>::get_or_emplace: created new entry for key '{}' in '{}'",
                stringify_key(key),
                _debug_name);
        }
        return it->second;
    }

    T* insert(const Key& key, T value)
    {
        spdlog::debug(
            "wynott::map<...>::insert: created new entry for key '{}' in '{}'",
            stringify_key(key),
            _debug_name);

        auto [it, inserted] = _map.emplace(key, std::move(value));
        return &it->second;
    }

    void erase(const Key& key)
    {
        _map.erase(key);
    }

    bool contains(const Key& key) const
    {
        return _map.find(key) != _map.end();
    }

    void clear()
    {
        _map.clear();
    }

    size_t size() const
    {
        return _map.size();
    }

    bool empty() const
    {
        return _map.empty();
    }

    iterator begin() { return _map.begin(); }
    iterator end() { return _map.end(); }
    const_iterator begin() const { return _map.begin(); }
    const_iterator end() const { return _map.end(); }

private:
    template <typename K, typename = void>
    struct is_streamable : std::false_type {};

    template <typename K>
    struct is_streamable<K, std::void_t<decltype(std::declval<std::ostream&>() << std::declval<K>())>>
        : std::true_type {};

    template <typename K>
    static std::string stringify_key_impl(const K& key, std::true_type)
    {
        std::ostringstream oss;
        oss << key;
        return oss.str();
    }

    template <typename K>
    static std::string stringify_key_impl(const K&, std::false_type)
    {
        return "<unprintable key>";
    }

    map_type _map;
    std::string _debug_name;
};

} // namespace wynott
